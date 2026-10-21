using System;
using System.Collections.Generic;
using System.Threading;
using ai.ubiquity;
using ai.ubiquity.samples.common;

namespace ai.ubiquity.samples.trafficshaping
{
    public static class Example1
    {
        private const long PollTimeoutNanos = 200_000_000L;
        private const int ThroughputQps = 50;
        // How often to print progress in the console.
        private const int DumpPeriod = 1000;
        private const int DefaultNumQueries = 500_000;


        // Single cleanup routine used on every exit path.
        private static void Cleanup(ClientHandle? clientHandle, ApiUtil? apiUtil, string? modelId, bool clientOpened)
        {
            try
            {
                clientHandle?.close();
                if (clientOpened)
                    Ubiquity.closeClient();

                if (apiUtil is not null && modelId is not null)
                {
                    Console.WriteLine("Waiting 10 seconds for the last feedback to be processed...");
                    Thread.Sleep(10_000);
                    apiUtil.DeleteModel(modelId);
                
                }
            } catch (Exception e)
            {
                Console.Error.WriteLine("Couldn't cleanup ressources : " + e.Message);
            } 
        }

        public static int Main(string[] args)
        {
            if (args.Length < 2)
            {
                Console.WriteLine("Usage: ./traffic-shaping/example1 <API_TOKEN> <CLUSTER_ID> [NUM_QUERIES]");
                return 1;
            }
            string apiToken = args[0];
            string clusterId = args[1];
            int numQueries = args.Length < 3 ? DefaultNumQueries : int.Parse(args[2]);

            ApiUtil? apiUtil = null;
            string? modelId = null;
            bool clientOpened = false;
            ClientHandle? clientHandle = null;

            try {
                // 1. Create model via API
                // This model uses a predefined feature engineering as an example
                apiUtil = new ApiUtil(apiToken);
                modelId = apiUtil.CreateModel("traffic-shaping", BuildModelSchema(clusterId));
                if (string.IsNullOrEmpty(modelId))
                    throw new ArgumentException("Failed to create model");

                // 2. Create and connect a ubiquity-client
                Ubiquity.setCredentials(apiToken);
                Ubiquity.setDefaultLogLevel(Ubiquity.LOG_OFF);
                int statusCode = Ubiquity.openClient(clusterId);
                if (statusCode != Ubiquity.SUCCESS)
                    throw new InvalidOperationException($"Error during connection: {statusCode}");
                clientOpened = true;

                clientHandle = Ubiquity.getClientHandle();

                // 3. Prepare data
                Random random = new Random(123);
                List<BidRequest> requests = BuildRequests();
                List<Feedback> feedbacks = BuildFeedbacks();
                Dictionary<long, Feedback> feedbackByQueryId = new Dictionary<long, Feedback>();

                // useful for pacing the traffic sent to the ml cluster based on a fixed throughput
                long paceNanos = 1_000_000_000L / ThroughputQps;
                long nextSendNanoTime = GetTimeNanos();

                // 4. Loop and send prediction queries with their associated feedbacks.
                int count = 0;
                while (count < numQueries)
                {
                    if (GetTimeNanos() < nextSendNanoTime)
                    {
                        continue;
                    }

                    nextSendNanoTime += paceNanos;

                    // 4.1 prepare a query
                    // simulate the request Id by generating a random long
                    long queryId = random.NextInt64();
                    BidRequest request = requests[count % requests.Count];
                    Feedback feedback = feedbacks[count % feedbacks.Count];
                    feedbackByQueryId[queryId] = feedback;

                    clientHandle.startTrafficShaping();
    
                    // fill in the features as specified in the model schema
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

                    // 4.2. send the prediction request
                    if (clientHandle.predict(modelId, queryId) == Ubiquity.SUCCESS)
                    {
                        count++;
                    }

                    // 4.3. wait to poll prediction response
                    long deadline = GetTimeNanos() + PollTimeoutNanos;
                    do
                    {
                        statusCode = clientHandle.poll();
                        Thread.Yield();
                    }
                    while (statusCode == Ubiquity.NO_DATA && GetTimeNanos() < deadline);

                    if (statusCode != Ubiquity.SUCCESS)
                    {
                        continue;
                    }

                    // 4.4. read the response data
                    sbyte action = clientHandle.getAction();

                    // 4.5 Sending back feedback (if when applicable)
                        long completedQueryId = clientHandle.getQueryId();
                        Feedback? completedFeedback;
                        if (feedbackByQueryId.TryGetValue(completedQueryId, out completedFeedback))
                        {
                            // 4.5.1 send a positive feedback of level 0 to the cluster indicating that the DSP participated in an auction.
                            clientHandle.train(modelId, completedQueryId, "bid");
                            // 4.5.2. Try to send feedback of level 1 indicating that the DSP won the auction.
                            if (completedFeedback.Level1)
                            {
                                clientHandle.train(modelId, completedQueryId, "win");
                            }
                        }

                    if (count % DumpPeriod == 0)
                    {
                        Console.WriteLine(
                            $"Total queries = {count}, last response: action = {(action == 0 ? "NO_SEND" : "SEND")}");
                    }
                }
                return 0;
        }
        catch (Exception e) {
            Console.Error.WriteLine("Fatal error: " + e.Message);
            return 1;
        }
        finally {
            // close the opened resources
            Cleanup(clientHandle, apiUtil, modelId, clientOpened);
        }
        }

        private static string BuildModelSchema(string clusterId)
        {
            return $$"""
                        {
                            "cluster_id": "{{clusterId}}",
                            "name": "traffic-shaping/Example1",
                            "feedbacks": [
                                {
                                    "name": "bid",
                                    "max_delay": 65
                                },
                                {
                                    "name": "won",
                                    "max_delay": 130
                                }
                            ],
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

        private static List<Feedback> BuildFeedbacks()
        {
            return new List<Feedback>
            {
                new Feedback { Level1 = true, Level2 = false, Level3 = false },
                new Feedback { Level1 = false, Level2 = true, Level3 = false },
                new Feedback { Level1 = false, Level2 = false, Level3 = true },
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

        private sealed class Feedback
        {
            public bool Level1 { get; set; }
            public bool Level2 { get; set; }
            public bool Level3 { get; set; }
        }

    }
}
