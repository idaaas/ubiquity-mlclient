package ai.ubiquity.samples.lineitem;

import ai.ubiquity.*;
import ai.ubiquity.samples.utils.ApiUtil;
import ai.ubiquity.samples.utils.BidRequest;
import ai.ubiquity.samples.utils.CycleIterator;
import ai.ubiquity.samples.utils.Feedback;

import java.time.Duration;
import java.util.*;

public class Example1 {
    private static final long POLL_TIMEOUT = Duration.ofMillis(200).toNanos();
    private static final int THROUGHPUT_IN_QPS = 300;

    public static void main(String[] args) throws Exception {
        if(args.length < 2) throw new RuntimeException("You need to provide API token and cluster ID to run the LineItemExample");
        final String apiToken = args[0];
        final String clusterId = args[1];

        // 1. Create your models via API
        ApiUtil apiUtil = new ApiUtil(apiToken);
        String modelId = apiUtil.createLineItemModel(modelSchema.formatted(clusterId));

        // 2. Create and connect a ubiquity-client
        Ubiquity.setCredential(apiToken);
        int statusCode = Ubiquity.openClient(clusterId);
        if (Ubiquity.SUCCESS != statusCode) throw new RuntimeException("Error during connection : " + statusCode);
        ClientHandle clientHandle = Ubiquity.getClientHandle();

        // 3. Prepare data and feedback streams
        int numQueries = 250000;
        float maxCPM = 2.0F;
        Random rand = new Random(123); // to generate query ids
        List<Long> creativeIds = Arrays.asList(1L, 2L, 3L, 4L, 5L); // a list of all creatives ids used when creating the model
        // use cycle iterators in order to loop infinitely over the data
        Iterator<BidRequest> dataIterator = CycleIterator.bidRequestIterator("/client/datasets/features.csv.gz");
        Iterator<Feedback> feedbackIterator = CycleIterator.feedbackIterator("/client/datasets/feedback.csv.gz");
        HashMap<Long, Feedback> feedbackHashMap = new HashMap<>();

        // The next lines of code are useful for pacing the traffic sent to the ml cluster based on a fixed throughput
        final long paceInNanos = Duration.ofSeconds(1).toNanos() / THROUGHPUT_IN_QPS;
        long nextSendNanoTime = System.nanoTime();

        // 4. Loop and send prediction queries with their associated feedbacks
        final int dumpPeriod = 1000;
        int count = 0;
        while (count < numQueries) {
            if(System.nanoTime() < nextSendNanoTime) continue;
            nextSendNanoTime = nextSendNanoTime + paceInNanos;

            // 4.1 prepare a line item query
            long queryId = rand.nextLong();
            // read a bid request with its feedback from the dataset file
            BidRequest request = dataIterator.next();
            Feedback feedback = feedbackIterator.next();
            // read the feedback for all the levels and store it in a temp map
            feedbackHashMap.put(queryId, feedback);
            // prepare a line item query using this bid request
            clientHandle.startLineItem(modelId, queryId, maxCPM);
            // fill in a random subset of eligible creatives
            Collections.shuffle(creativeIds);
            int numEligibleCreatives = 1 + rand.nextInt(creativeIds.size()); // at least one creative
            for (int i = 0; i < numEligibleCreatives; i++) clientHandle.putCreative(creativeIds.get(i));
            // fill in the features as specified in the model schema in config.json
            clientHandle.putNominal(request.getAppBundle());
            clientHandle.putCategorical(request.getImpBannerPos());
            clientHandle.putCategorical(request.getLocalTime());
            clientHandle.putNominal(request.getDeviceOs());
            clientHandle.putNominal(request.getDeviceGeoCity());
            clientHandle.putNominal(request.getDeviceModel());
            clientHandle.putNominal(request.getBcat());
            clientHandle.putNominal(request.getDeviceGeoZip());
            clientHandle.putNominal(request.getUserYob());
            clientHandle.putNominal(request.getAppPublisherName());
            clientHandle.putCategorical(request.getDeviceConnectionType());
            clientHandle.putNominal(request.getDeviceGeoRegion());
            clientHandle.putNominal(request.getAppCat());
            clientHandle.putNominal(request.getDeviceMake());
            clientHandle.putNominal(request.getDeviceHxW());
            clientHandle.putCategorical(request.getUserGender());
            clientHandle.putNominal(request.getDeviceLanguage());
            clientHandle.putNominal(request.getImpDisplayManager());
            clientHandle.putNominal(request.getImpBannerBattr());
            clientHandle.putNominal(request.getDeviceGeoCountry());
            clientHandle.putNominal(request.getAppContentLanguage());
            clientHandle.putCategorical(request.getAppContentContext());
            clientHandle.putBoolean(request.getImpBannerTopFrame() ? (byte)1 : (byte)0);
            clientHandle.putCategorical(request.getDeviceType());
            clientHandle.putBoolean(request.getImpInstl() ? (byte)1 : (byte)0);
            clientHandle.putNominal(request.getAppPageCat());

            // 4.2. send the prediction request
            if(clientHandle.predict() == Ubiquity.SUCCESS)
                count ++;

            // 4.3. wait to poll prediction response and validate its QueryId
            long deadline = System.nanoTime() + POLL_TIMEOUT;
            do {
                statusCode = clientHandle.poll();
                Thread.yield();
            } while (statusCode == Ubiquity.NO_DATA && System.nanoTime() < deadline);
            if (statusCode != Ubiquity.SUCCESS) continue;

            float conversionProbability = clientHandle.getProbability();
            long creativeId = clientHandle.getCreativeId();
            float price = clientHandle.getPrice();
            queryId = clientHandle.getQueryId();
            feedback = feedbackHashMap.get(queryId);
            // send positive feedback only if conversion occurred
            // 4.5 Sending back conversion feedback (if any)
            // 4.5.1. send a notification of execution
            // notifies the cluster that the predicted pair (price, creative) have entered the auction
            // here we suppose that all predictions actually enter the auction
            // if the cluster is not notified of the execution, no feedback will be considered for the given query
            clientHandle.train(modelId, queryId, 0);
            // 4.5.2. Try to send feedback of level 1
            if (feedback.getLevel1()) clientHandle.train(modelId, queryId, 1);
            // 4.5.3. Try to send feedback of level 2
            if (feedback.getLevel2()) clientHandle.train(modelId, queryId, 2);

            if (count % dumpPeriod == 0) {
                System.out.printf("Total queries = %d, last response: predicted price = %f, conversion probability = %f, selected creative id = %d\n", count, price, conversionProbability, creativeId);
            }
        }
        clientHandle.close();
        Ubiquity.closeClient();

        // 5. wait for the last feedbacks sent to be processed by the cluster then delete the models
        Thread.sleep(10000);
        apiUtil.deleteModel(modelId);
    }

    private final static String modelSchema = """
        {
          "cluster_id": "%s",
          "name": "lineitem/Example1",
          "feedback_delays": [
            20,
            30,
            40
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
              "name": "a0",
              "type": "numeric"
            },
            {
              "name": "a1",
              "type": "nominal"
            },
            {
              "name": "a2",
              "type": "boolean"
            },
            {
              "name": "a3",
              "type": "categorical"
            }
          ],
          "values": [
            {
              "id": 5,
              "features": [
                "0.79626095",
                "Arabic",
                "true",
                "6"
              ]
            },
            {
              "id": 1,
              "features": [
                "0.72317415",
                "French",
                "true",
                "9"
              ]
            },
            {
              "id": 2,
              "features": [
                "0.2532931",
                "German",
                "true",
                "7"
              ]
            },
            {
              "id": 3,
              "features": [
                "0.80586946",
                "German",
                "true",
                "6"
              ]
            },
            {
              "id": 4,
              "features": [
                "0.7160485",
                "French",
                "false",
                "9"
              ]
            }
          ]
        }
      }""";
}
