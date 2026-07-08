using System;
using System.Collections.Generic;
using System.Threading;
using ai.ubiquity;
using ai.ubiquity.samples.common;

namespace ai.ubiquity.samples.lineitem
{
    public static class Example1
    {
        private const long PollTimeoutNanos = 200_000_000L;
        private const int ThroughputQps = 50;

        public static void Main(string[] args)
        {
            if (args.Length < 2)
            {
                throw new ArgumentException("Usage: ./line-item/example1 <API_TOKEN> <CLUSTER_ID> [NUM_QUERIES]");
            }

            string apiToken = args[0];
            string clusterId = args[1];
            int numQueries = args.Length < 3 ? 500000 : int.Parse(args[2]);
            ApiUtil apiUtil = new ApiUtil(apiToken);
            string modelId = apiUtil.CreateLineItemModel(BuildLineItemModelSchema(clusterId));

            Ubiquity.setCredentials(apiToken);
            Ubiquity.setDefaultLogLevel(Ubiquity.LOG_OFF);
            int statusCode;
            ClientHandle? clientHandle = null;

            try
            {
                statusCode = Ubiquity.openClient(clusterId);
                if (statusCode != Ubiquity.SUCCESS)
                {
                    throw new InvalidOperationException($"Error during connection: {statusCode}");
                }

                clientHandle = Ubiquity.getClientHandle();

                float maxCpm = 2.0f;
                float adversaryPrice = maxCpm / 2.0f;
                long creativeId = 1L;

                Random random = new Random(123);
                List<BidRequest> requests = BuildRequests();
                List<Feedback> feedbacks = BuildFeedbacks();
                Dictionary<long, Feedback> feedbackByQueryId = new Dictionary<long, Feedback>();

                long paceNanos = 1_000_000_000L / ThroughputQps;
                long nextSendNanoTime = GetTimeNanos();

                int count = 0;
                const int dumpPeriod = 1000;

                while (count < numQueries)
                {
                    if (GetTimeNanos() < nextSendNanoTime)
                    {
                        continue;
                    }

                    nextSendNanoTime += paceNanos;

                    long queryId = random.NextInt64();
                    BidRequest request = requests[count % requests.Count];
                    Feedback feedback = feedbacks[count % feedbacks.Count];
                    feedbackByQueryId[queryId] = feedback;

                    clientHandle.startLineItem(maxCpm);
                    clientHandle.putCreative(creativeId);
                    FillFeatures(clientHandle, request);

                    if (clientHandle.predict(modelId, queryId) == Ubiquity.SUCCESS)
                    {
                        count++;
                    }

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

                    float conversionProbability = clientHandle.getProbability();
                    long selectedCreativeId = clientHandle.getCreativeId();
                    float price = clientHandle.getPrice();

                    if (price > adversaryPrice)
                    {
                        long completedQueryId = clientHandle.getQueryId();
                        Feedback? completedFeedback;
                        if (feedbackByQueryId.TryGetValue(completedQueryId, out completedFeedback))
                        {
                            clientHandle.train(modelId, completedQueryId, 0);
                            if (completedFeedback.Level1)
                            {
                                clientHandle.train(modelId, completedQueryId, 1);
                            }
                            if (completedFeedback.Level2)
                            {
                                clientHandle.train(modelId, completedQueryId, 2);
                            }
                        }
                    }

                    if (count % dumpPeriod == 0)
                    {
                        Console.WriteLine(
                            $"Total queries = {count}, last response: predicted price = {price}, conversion probability = {conversionProbability:F7}, selected creative id = {selectedCreativeId}");
                    }
                }
            }
            finally
            {
                if (clientHandle is not null)
                {
                    clientHandle.close();
                }
                Ubiquity.closeClient();

                Thread.Sleep(TimeSpan.FromSeconds(196));
                apiUtil.DeleteModel(modelId);
            }
        }

        private static string BuildLineItemModelSchema(string clusterId)
        {
            return $$"""
                        {
                            "cluster_id": "{{clusterId}}",
                            "name": "lineitem/Example1",
                            "feedbacks": [
                                {
                                    "name": "imp",
                                    "max_delay": 65
                                },
                                {
                                    "name": "view",
                                    "max_delay": 130
                                },
                                {
                                    "name": "click",
                                    "max_delay": 196
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
                            ],
                            "creatives": {
                                "features": [
                                    {
                                        "name": "a1",
                                        "type": "categorical"
                                    }
                                ],
                                "values": [
                                    {
                                        "id": 1,
                                        "features": [
                                            "0"
                                        ]
                                    }
                                ]
                            }
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

        private static List<Feedback> BuildFeedbacks()
        {
            return new List<Feedback>
            {
                new Feedback { Level1 = true, Level2 = false },
                new Feedback { Level1 = false, Level2 = true }
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
        }

    }
}
