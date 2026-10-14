using System;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;
using ai.ubiquity;
using ai.ubiquity.samples.common;

namespace ai.ubiquity.samples.bidshading
{
    public static class Example1
    {

        static readonly TimeSpan PredictionTimeout = TimeSpan.FromMilliseconds(100);
        static readonly TimeSpan AuctionPollDelay = TimeSpan.FromMilliseconds(500);
        static readonly TimeSpan WaitForFeedback = TimeSpan.FromSeconds(10);
        record PredictionResponse(
            string ModelId,
            long QueryId,
            float Price
        );

        static string ComputeChannelId(string modelId, long queryId)
        {
          return $"{modelId}:{queryId}";
        }

        static async Task PollingRoutineAsync(
            ConcurrentDictionary<string, Channel<PredictionResponse>> pendingChannels,
            CancellationToken token)
        {
            var client = Ubiquity.getClientHandle();
            try 
            {
                var status = Ubiquity.NO_DATA;
                do 
                {
                    status = client.poll();
                    if(status != Ubiquity.SUCCESS){
                        // Apply an exponential backoff strategy to avoid busy loop 
                        client.idle();
                        continue;
                    }
                    var response = new PredictionResponse(
                        client.getModelId(),
                        client.getQueryId(),
                        client.getPrice());

                    var channelId = ComputeChannelId(response.ModelId, response.QueryId);
                    await pendingChannels[channelId].Writer.WriteAsync(response, token);
                } while (!token.IsCancellationRequested);
            }
            finally 
            {
                client.close();
            }
        }

        static async Task AuctionFeedbackRoutineAsync(string modelId, long queryId)
        {
            var client = Ubiquity.getClientHandle();
            using var delay = new CancellationTokenSource(WaitForFeedback);
            try 
            {
                // Simulate waiting for a positive feedback 
                await Task.Delay(WaitForFeedback, delay.Token);
                // once your positive feedback is received, use it 
                Console.WriteLine($"Received a feedback notification for WinAuction");
                // Only call train when you win the auction (feedback level is always 0 for bid shading feedbacks)
                client.train(modelId, queryId, 0);
            }
            finally 
            {
                client.close();
            }
        }

        static async Task PredictionRoutineAsync(
            ConcurrentDictionary<string, Channel<PredictionResponse>> pendingChannels,
            string channelId)
        {
            try
            {
                // Wait for the prediction response to arrive on the channel, with a timeout
                using var timeout = new CancellationTokenSource(PredictionTimeout);

                var response = await pendingChannels[channelId].Reader.ReadAsync(timeout.Token);
                Console.WriteLine($"Received prediction response for prediction {channelId}");

                // Do someting with the response, e.g. send it to the ad exchange and get the auction result
                // available fields in the response 
                //      resp.ModelId
                //      resp.QueryId
                //      resp.Price
            }
            // If the timeout occurs, log a message
            catch (OperationCanceledException)
            {
                Console.WriteLine($"Timeout waiting for prediction {channelId}");
            }
        }
      
        static async Task<int> Main(string[] args)
        {
            if (args.Length < 2)
            {
                Console.WriteLine("Usage: <apiToken> <clusterId>");
                return 1;
            }

            var apiToken = args[0];
            var clusterId = args[1];

            // Shared between the routines but owned by Main: a local, not a static.
            var pendingChannels = new ConcurrentDictionary<string, Channel<PredictionResponse>>();
            using var pollingCts = new CancellationTokenSource();

            bool clientOpened = false;
            ClientHandle? predictClient = null;
            ApiUtil? apiUtil = null;
            string? modelId = null;

            try
            {
                // Create model with the API token and cluster ID, and get the model ID
                // This model uses a predefined feature engineering as an example
                apiUtil = new ApiUtil(apiToken);
                modelId = apiUtil.CreateModel("bid-shading", BuildModelSchema(clusterId));
                if (string.IsNullOrEmpty(modelId))
                    throw new ArgumentException("Failed to create model");
        
                Ubiquity.setCredentials(apiToken);
                Ubiquity.setDefaultLogLevel(Ubiquity.LOG_OFF);

                var status = Ubiquity.openClient(clusterId);
                if (status != Ubiquity.SUCCESS)
                    throw new InvalidOperationException($"Connection failed: {Ubiquity.getStatusString(status)}");
                clientOpened = true;

                predictClient = Ubiquity.getClientHandle();

                // simulate the request Id by generating a random long
                var rng = Random.Shared;
                var queryId = rng.NextInt64();

                // Create a channel to receive the prediction response for this query
                var channelId = ComputeChannelId(modelId, queryId);
                pendingChannels[channelId] = Channel.CreateBounded<PredictionResponse>(1);
                
                // Start the polling and feedback routines as background tasks in separate threads
                // Client handles are not thread-safe, so we create a new client handle for each routine
                var pollingRoutine = Task.Run(() => PollingRoutineAsync(pendingChannels, pollingCts.Token));
                var feedbackRoutine = Task.Run(() => AuctionFeedbackRoutineAsync(modelId, queryId));

                // Random bid request, in a real scenario this would come from the ad exchange
                var request = new BidRequest
                {
                    AppBundle = "com.example.app" + rng.Next(1, 100),
                    ImpBannerPos = (sbyte)rng.Next(0, 5),
                    LocalTime = (sbyte)rng.Next(0, 24),
                    DeviceOs = rng.Next(0, 2) == 0 ? "android" : "ios",
                    DeviceGeoCity = "city" + rng.Next(1, 100),
                    DeviceModel = "model" + rng.Next(1, 50),
                    Bcat = "IAB" + rng.Next(1, 20),
                    DeviceGeoZip = rng.Next(10000, 99999).ToString(),
                    UserYob = (1980 + rng.Next(0, 40)).ToString(),
                    AppPublisherName = "publisher-" + rng.Next(1, 10),
                    DeviceConnectionType = (sbyte)rng.Next(0, 3),
                    DeviceGeoRegion = "region" + rng.Next(1, 10),
                    AppCat = "IAB" + rng.Next(1, 20) + "-" + rng.Next(1, 5),
                    DeviceMake = "make" + rng.Next(1, 10),
                    DeviceHxW = $"{rng.Next(720, 1440)}x{rng.Next(1280, 2560)}",
                    UserGender = (sbyte)rng.Next(0, 3),
                    DeviceLanguage = rng.Next(0, 2) == 0 ? "en" : "fr",
                    ImpDisplayManager = rng.Next(0, 2) == 0 ? "admob" : "gma",
                    ImpBannerBattr = rng.Next(0, 2) == 0 ? "none" : "audio",
                    DeviceGeoCountry = rng.Next(0, 2) == 0 ? "us" : "fr",
                    AppContentLanguage = rng.Next(0, 2) == 0 ? "en" : "fr",
                    AppContentContext = (sbyte)rng.Next(0, 5),
                    ImpBannerTopFrame = rng.Next(0, 2) == 0,
                    DeviceType = (sbyte)rng.Next(0, 5),
                    ImpInstl = rng.Next(0, 2) == 0,
                    AppPageCat = "IAB" + rng.Next(1, 20)
                };

                const float bidPrice = 5.0f; 
                const float bidFloor = 0.01f;
                // Use any of the available modes: WIN_RATE_PRESERVATION, EXPECTED_SURPLUS_MAXIMIZATION
                const int optimizationMode = Ubiquity.EXPECTED_SURPLUS_MAXIMIZATION;
                predictClient.startBidShading(bidPrice, bidFloor, optimizationMode);

                predictClient.putNominal(request.AppBundle);
                predictClient.putCategorical(request.ImpBannerPos);
                predictClient.putCategorical(request.LocalTime);
                predictClient.putNominal(request.DeviceOs);
                predictClient.putNominal(request.DeviceGeoCity);
                predictClient.putNominal(request.DeviceModel);
                predictClient.putNominal(request.Bcat);
                predictClient.putNominal(request.DeviceGeoZip);
                predictClient.putNominal(request.UserYob);
                predictClient.putNominal(request.AppPublisherName);
                predictClient.putCategorical(request.DeviceConnectionType);
                predictClient.putNominal(request.DeviceGeoRegion);
                predictClient.putNominal(request.AppCat);
                predictClient.putNominal(request.DeviceMake);
                predictClient.putNominal(request.DeviceHxW);
                predictClient.putCategorical(request.UserGender);
                predictClient.putNominal(request.DeviceLanguage);
                predictClient.putNominal(request.ImpDisplayManager);
                predictClient.putNominal(request.ImpBannerBattr);
                predictClient.putNominal(request.DeviceGeoCountry);
                predictClient.putNominal(request.AppContentLanguage);
                predictClient.putCategorical(request.AppContentContext);
                predictClient.putBoolean((sbyte)(request.ImpBannerTopFrame ? 1 : 0));
                predictClient.putCategorical(request.DeviceType);
                predictClient.putBoolean((sbyte)(request.ImpInstl ? 1 : 0));
                predictClient.putNominal(request.AppPageCat);

                // Send the prediction request, which is syncrhonous. 
                // Retry until success
                do 
                {
                    status = predictClient.predict(modelId, queryId);
                }while(status != Ubiquity.SUCCESS);

                // Start a background task to wait for the prediction response with a timeout
                var predictionRoutine = PredictionRoutineAsync(pendingChannels, channelId);

                // Wait for the prediction and the feedback routines to be done
                await Task.WhenAll(predictionRoutine, feedbackRoutine);
                return 0;
             }
            catch (Exception e)
            {
                Console.Error.WriteLine("Fatal error: " + e.Message);
                return 1;
            }
            finally
            {
                // Single cleanup path on every exit (success, failure, exception).
                await CleanupAsync(pollingCts, predictClient, clientOpened, apiUtil, modelId);
            }
        }

        // Close acquired resources in reverse order: polling routine, client, then model.
        // resources set as parameters, so no static state is needed.
        static async Task CleanupAsync(
            CancellationTokenSource pollingCts,
            ClientHandle? predictClient,
            bool clientOpened,
            ApiUtil? apiUtil,
            string? modelId)
        {
            // 1. Stop the polling routine
            pollingCts.Cancel();

            // 2. Close the client
            try
            {
                predictClient?.close();
                if (clientOpened)
                    Ubiquity.closeClient();
            }
            catch (Exception e)
            {
                Console.Error.WriteLine("Couldn't close client : " + e.Message);
            }

            // 3. Delete the model (only if it was created)
            if (apiUtil is not null && modelId is not null)
            {
                try
                {
                    Console.WriteLine("Waiting 10 seconds for the last feedback to be processed...");
                    await Task.Delay(10);
                    apiUtil.DeleteModel(modelId);
                }
                catch (Exception e)
                {
                    Console.Error.WriteLine("Couldn't delete created model : " + e.Message);
                }
            }
        }


        private static string BuildModelSchema(string clusterId)
        {
            return $$"""
                        {
                            "cluster_id": "{{clusterId}}",
                            "name": "bidshading/Example1",
                            "feedbacks": [
                                {
                                    "name": "win",
                                    "max_delay": 1800
                                }
                            ],
                            "min_bid_price": 0.01,
                            "max_bid_price": 10.0,
                            "features": [
                                {
                                    "name": "app.bundle",
                                    "type": "nominal"
                                },
                                {
                                    "name": "imp.banner.pos",
                                    "type": "categorical"
                                },
                                {
                                    "name": "localtime",
                                    "type": "categorical"
                                },
                                {
                                    "name": "device.os",
                                    "type": "nominal"
                                },
                                {
                                    "name": "device.geo.city",
                                    "type": "nominal"
                                },
                                {
                                    "name": "device.model",
                                    "type": "nominal"
                                },
                                {
                                    "name": "bcat",
                                    "type": "nominal"
                                },
                                {
                                    "name": "device.geo.zip",
                                    "type": "nominal"
                                },
                                {
                                    "name": "user.yob",
                                    "type": "nominal"
                                },
                                {
                                    "name": "app.publisher.name",
                                    "type": "nominal"
                                },
                                {
                                    "name": "device.connectiontype",
                                    "type": "categorical"
                                },
                                {
                                    "name": "device.geo.region",
                                    "type": "nominal"
                                },
                                {
                                    "name": "app.cat",
                                    "type": "nominal"
                                },
                                {
                                    "name": "device.make",
                                    "type": "nominal"
                                },
                                {
                                    "name": "device.HxW",
                                    "type": "nominal"
                                },
                                {
                                    "name": "user.gender",
                                    "type": "categorical"
                                },
                                {
                                    "name": "device.language",
                                    "type": "nominal"
                                },
                                {
                                    "name": "imp.displaymanager",
                                    "type": "nominal"
                                },
                                {
                                    "name": "imp.banner.battr",
                                    "type": "nominal"
                                },
                                {
                                    "name": "device.geo.country",
                                    "type": "nominal"
                                },
                                {
                                    "name": "app.content.language",
                                    "type": "nominal"
                                },
                                {
                                    "name": "app.content.context",
                                    "type": "categorical"
                                },
                                {
                                    "name": "imp.banner.topframe",
                                    "type": "boolean"
                                },
                                {
                                    "name": "device.devicetype",
                                    "type": "categorical"
                                },
                                {
                                    "name": "imp.instl",
                                    "type": "boolean"
                                },
                                {
                                    "name": "app.pagecat",
                                    "type": "nominal"
                                }
                            ]
                        }
                        """;
        }
        
        private sealed class BidRequest
        {
            public string AppBundle { get; set; } = string.Empty;
            public sbyte ImpBannerPos { get; set; }
            public sbyte LocalTime { get; set; }
            public string DeviceOs { get; set; } = string.Empty;
            public string DeviceGeoCity { get; set; } = string.Empty;
            public string DeviceModel { get; set; } = string.Empty;
            public string Bcat { get; set; } = string.Empty;
            public string DeviceGeoZip { get; set; } = string.Empty;
            public string UserYob { get; set; } = string.Empty;
            public string AppPublisherName { get; set; } = string.Empty;
            public sbyte DeviceConnectionType { get; set; }
            public string DeviceGeoRegion { get; set; } = string.Empty;
            public string AppCat { get; set; } = string.Empty;
            public string DeviceMake { get; set; } = string.Empty;
            public string DeviceHxW { get; set; } = string.Empty;
            public sbyte UserGender { get; set; }
            public string DeviceLanguage { get; set; } = string.Empty;
            public string ImpDisplayManager { get; set; } = string.Empty;
            public string ImpBannerBattr { get; set; } = string.Empty;
            public string DeviceGeoCountry { get; set; } = string.Empty;
            public string AppContentLanguage { get; set; } = string.Empty;
            public sbyte AppContentContext { get; set; }
            public bool ImpBannerTopFrame { get; set; }
            public sbyte DeviceType { get; set; }
            public bool ImpInstl { get; set; }
            public string AppPageCat { get; set; } = string.Empty;
        }
    }
}
