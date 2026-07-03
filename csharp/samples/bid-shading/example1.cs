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

        static readonly ConcurrentDictionary<string, Channel<PredictionResponse>> Pending = new();
        static readonly ConcurrentDictionary<string, PredictionResponse> AuctionPending = new();

        static string ComputeChannelId(string modelId, long queryId)
        {
          return $"{modelId}:{queryId}";
        }

        static async Task PollingRoutineAsync(CancellationToken token)
        {
            using var client = Ubiquity.getClientHandle();

            while (!token.IsCancellationRequested)
            {
                if (client.poll() == Ubiquity.SUCCESS)
                {
                    var response = new PredictionResponse(
                        client.getModelId(),
                        client.getQueryId(),
                        client.getPrice());

                    var channelId = ComputeChannelId(response.ModelId, response.QueryId);

                    if (Pending.TryRemove(channelId, out var channel))
                        await channel.Writer.WriteAsync(response, token);
                    else
                        Console.WriteLine($"Received response for unknown channel {channelId}");
                }

                await Task.Yield();
            }
        }

        static async Task AuctionFeedbackRoutineAsync(
            CancellationToken token)
        {
            using var client = Ubiquity.getClientHandle();

            while (!token.IsCancellationRequested)
            {
                foreach (var kv in AuctionPending)
                {
                    if (!AuctionPending.TryRemove(kv.Key, out var resp))
                        continue;

                    // do someting with the response, e.g. send it to the ad exchange and get the auction result
                    // simulated with sleep 10sec
                    await Task.Delay(WaitForFeedback, token);

                    client.train(resp.ModelId, resp.QueryId, 0);
                }

                await Task.Delay(AuctionPollDelay, token);
            }
        }
      
        static async Task<int> Main(string[] args)
        {
            if (args.Length < 2)
            {
                Console.WriteLine("Usage: <apiToken> <clusterId> [numQueries]");
                return 1;
            }

            var apiToken = args[0];
            var clusterId = args[1];
            var numQueries = args.Length > 2 ? int.Parse(args[2]) : 500000;

            // Create model with the API token and cluster ID, and get the model ID
            ApiUtil apiUtil = new ApiUtil(apiToken);
            string modelId = apiUtil.CreateBidShadingModel(BuildBidShadingModelSchema(clusterId));

            if (string.IsNullOrEmpty(modelId))
                return 1;

            try
            {
                Ubiquity.setCredentials(apiToken);

                var status = Ubiquity.openClient(clusterId);
                if (status != Ubiquity.SUCCESS)
                {
                    Console.WriteLine($"Connection failed: {Ubiquity.getStatusString(status)}");
                    return 1;
                }

                try
                {
                    using var predictClient = Ubiquity.getClientHandle();

                    using var cts = new CancellationTokenSource();

                    // Start the polling and feedback routines as background tasks in separate threads
                    // Only one client handle per thread is allowed, so we create a new client handle for each routine
                    _ = Task.Run(() => PollingRoutineAsync(cts.Token));
                    _ = Task.Run(() => AuctionFeedbackRoutineAsync(cts.Token));

                    const int ThroughputQps = 50;
                    var pace = TimeSpan.FromSeconds(1.0 / ThroughputQps);
                    var nextSend = DateTime.UtcNow;

                    const int dumpPeriod = 1000;

                    var rng = Random.Shared;
                    var count = 0;

                    // Generate random bid requests and send them to the model for prediction
                    while (count < numQueries)
                    {
                        if (DateTime.UtcNow < nextSend)
                        {
                            await Task.Yield();
                            continue;
                        }

                        nextSend += pace;

                        var queryId = rng.NextInt64();

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
                        const int optimizationMode = 1; // WIN_RATE_PRESERVATION = 0, EXPECTED_SURPLUS_MAXIMIZATION = 1
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

                        // Start the prediction request and wait for it to be sent.
                        if (predictClient.predict(modelId, queryId) != Ubiquity.SUCCESS)
                            continue;

                        count++;

                        // Create a channel to receive the prediction response for this query
                        var channelId = ComputeChannelId(modelId, queryId);
                        var channel = Channel.CreateBounded<PredictionResponse>(1);

                        // Store the channel in the Pending dictionary so that the polling routine can find it when the response arrives
                        Pending[channelId] = channel;

                        // Start a background task to wait for the prediction response with a timeout (cancellation token)
                        _ = Task.Run(async () =>
                        {
                            try
                            {
                                // Wait for the prediction response to arrive on the channel, with a timeout
                                using var timeout =
                                    new CancellationTokenSource(PredictionTimeout);

                                var response =
                                    await channel.Reader.ReadAsync(timeout.Token);

                                AuctionPending[channelId] = response;

                                if (count % dumpPeriod == 0)
                                {
                                    Console.WriteLine(
                                        $"Total={count} " +
                                        $"Price={response.Price:F4} ");
                                }
                            }
                            // If the timeout occurs, remove the channel from the Pending dictionary and log a message
                            catch (OperationCanceledException)
                            {
                                Pending.TryRemove(channelId, out _);
                                Console.WriteLine(
                                    $"Timeout waiting for prediction {channelId}");
                            }
                        });
                    }

                    await Task.Delay(TimeSpan.FromSeconds(196));

                    cts.Cancel();
                }
                finally
                {
                    Ubiquity.closeClient();
                }
            }
            finally
            {
                apiUtil.DeleteModel(modelId);
            }

            return 0;
        }

        private static string BuildBidShadingModelSchema(string clusterId)
        {
            return $$"""
                        {
                            "cluster_id": "{{clusterId}}",
                            "name": "bidshading/Example1",
                            "feedback_delays": [
                                65
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
        
        private static long GetTimeNanos()
        {
            return DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() * 1_000_000L;
        }

        private static void FillFeatures(ClientHandle clientHandle, BidRequest request)
        {
            clientHandle.putNominal(request.AppBundle);
            clientHandle.putCategorical(request.ImpBannerPos);
            clientHandle.putCategorical(request.LocalTime);
            clientHandle.putNominal(request.DeviceOs);
            clientHandle.putNominal(request.DeviceGeoCity);
            clientHandle.putNominal(request.DeviceModel);
            clientHandle.putNominal(request.Bcat);
            clientHandle.putNominal(request.DeviceGeoZip);
            clientHandle.putNominal(request.UserYob);
            clientHandle.putNominal(request.AppPublisherName);
            clientHandle.putCategorical(request.DeviceConnectionType);
            clientHandle.putNominal(request.DeviceGeoRegion);
            clientHandle.putNominal(request.AppCat);
            clientHandle.putNominal(request.DeviceMake);
            clientHandle.putNominal(request.DeviceHxW);
            clientHandle.putCategorical(request.UserGender);
            clientHandle.putNominal(request.DeviceLanguage);
            clientHandle.putNominal(request.ImpDisplayManager);
            clientHandle.putNominal(request.ImpBannerBattr);
            clientHandle.putNominal(request.DeviceGeoCountry);
            clientHandle.putNominal(request.AppContentLanguage);
            clientHandle.putCategorical(request.AppContentContext);
            clientHandle.putBoolean(request.ImpBannerTopFrame ? (sbyte)1 : (sbyte)0);
            clientHandle.putCategorical(request.DeviceType);
            clientHandle.putBoolean(request.ImpInstl ? (sbyte)1 : (sbyte)0);
            clientHandle.putNominal(request.AppPageCat);
        }

        private static List<BidRequest> BuildRequests()
        {
            return new List<BidRequest>
            {
                new BidRequest
                {
                    AppBundle = "com.example.app1",
                    ImpBannerPos = 1,
                    LocalTime = 18,
                    DeviceOs = "android",
                    DeviceGeoCity = "paris",
                    DeviceModel = "pixel",
                    Bcat = "IAB1",
                    DeviceGeoZip = "75001",
                    UserYob = "1990",
                    AppPublisherName = "publisher-a",
                    DeviceConnectionType = 2,
                    DeviceGeoRegion = "idf",
                    AppCat = "IAB1-2",
                    DeviceMake = "google",
                    DeviceHxW = "1080x1920",
                    UserGender = 1,
                    DeviceLanguage = "fr",
                    ImpDisplayManager = "admob",
                    ImpBannerBattr = "none",
                    DeviceGeoCountry = "fr",
                    AppContentLanguage = "fr",
                    AppContentContext = 3,
                    ImpBannerTopFrame = true,
                    DeviceType = 1,
                    ImpInstl = false,
                    AppPageCat = "IAB9"
                },
                new BidRequest
                {
                    AppBundle = "com.example.app2",
                    ImpBannerPos = 2,
                    LocalTime = 9,
                    DeviceOs = "ios",
                    DeviceGeoCity = "lyon",
                    DeviceModel = "iphone",
                    Bcat = "IAB2",
                    DeviceGeoZip = "69001",
                    UserYob = "1988",
                    AppPublisherName = "publisher-b",
                    DeviceConnectionType = 1,
                    DeviceGeoRegion = "ara",
                    AppCat = "IAB2-3",
                    DeviceMake = "apple",
                    DeviceHxW = "1170x2532",
                    UserGender = 2,
                    DeviceLanguage = "en",
                    ImpDisplayManager = "gma",
                    ImpBannerBattr = "audio",
                    DeviceGeoCountry = "fr",
                    AppContentLanguage = "en",
                    AppContentContext = 2,
                    ImpBannerTopFrame = false,
                    DeviceType = 4,
                    ImpInstl = true,
                    AppPageCat = "IAB12"
                }
            };
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