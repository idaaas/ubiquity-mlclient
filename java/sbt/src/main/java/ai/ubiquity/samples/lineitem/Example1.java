package ai.ubiquity.samples.lineitem;

import ai.ubiquity.Ubiquity;
import ai.ubiquity.samples.utils.ApiUtil;
import ai.ubiquity.samples.utils.BidRequest;
import ai.ubiquity.samples.utils.CycleIterator;
import ai.ubiquity.samples.utils.Feedback;

import java.time.Duration;
import java.util.*;

public class Example1 {
    private static final long PREDICT_TIMEOUT = Duration.ofSeconds(10).toNanos();
    private static final long POLL_TIMEOUT = Duration.ofSeconds(1).toNanos();

    public static void main(String[] args) throws Exception {
        if(args.length < 2) throw new RuntimeException("You need to provide clusterId and API token to run the LineItemExample");
        final String apiToken = args[0];
        final String clusterId = args[1];

        // 1. Create your models via API
        ApiUtil apiUtil = new ApiUtil(clusterId, apiToken);
        String modelId = apiUtil.createLineItemModel();

        // 2. Create and connect a ubiquity-client
        Ubiquity.setCredential(apiToken);
        int errorCode = Ubiquity.openClient(clusterId);
        if (Ubiquity.ErrorCode.SUCCESS != errorCode) throw new RuntimeException("Error during connection : " + errorCode);
        Ubiquity.ClientHandle clientHandle = Ubiquity.getClientHandle();
        if (Ubiquity.ErrorCode.SUCCESS != clientHandle.getLastError()) throw new RuntimeException("Error during creation of clientHandle : " + errorCode);

        // 3. Prepare data and feedback streams
        int numQueries = 250000;
        float maxCPM = 2.0F;
        Random rand = new Random(123); // to generate query ids
        List<Long> creativeIds = Arrays.asList(1L, 2L, 3L, 4L, 5L); // a list of all creatives ids used when creating the model
        // use cycle iterators in order to loop infinitely over the data
        String feedbackPath = "./src/main/resources/feedback.csv.gz";
        String dataPath = "./src/main/resources/features.csv.gz";
        Iterator<BidRequest> dataIterator = CycleIterator.bidRequestIterator(dataPath);
        Iterator<Feedback> feedbackIterator = CycleIterator.feedbackIterator(feedbackPath);

        // 4. Loop and send prediction queries with their associated feedbacks
        final int dumpPeriod = 1000;
        for (int count = 1; count <= numQueries; count ++) {
            // 5.1. prepare and send a line item query
            long queryId = rand.nextLong();
            clientHandle.startLineItem(modelId, queryId, maxCPM);
            // 5.1.1. fill in a random subset of eligible creatives
            Collections.shuffle(creativeIds);
            int numEligibleCreatives = 1 + rand.nextInt(creativeIds.size()); // at least one creative
            for (int i = 0; i < numEligibleCreatives; i++) clientHandle.putCreative(creativeIds.get(i));

            // 5.1.2. fill in the features as specified in the model schema in config.json
            BidRequest request = dataIterator.next();
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
            clientHandle.putBoolean(request.getImpBannerTopFrame());
            clientHandle.putCategorical(request.getDeviceType());
            clientHandle.putBoolean(request.getImpInstl());
            clientHandle.putNominal(request.getAppPageCat());

            // 4.1.3. send the prediction request
            errorCode = clientHandle.predict();
            long deadline = System.nanoTime() + PREDICT_TIMEOUT;
            while (errorCode != Ubiquity.ErrorCode.SUCCESS && System.nanoTime() < deadline) {
                Thread.yield();
                errorCode = clientHandle.predict();
            }
            if (errorCode != Ubiquity.ErrorCode.SUCCESS) throw new RuntimeException("Could not send query " + clientHandle.getLastErrorString());

            // 4.2. wait to poll prediction response and validate its QueryId
            errorCode = clientHandle.poll();
            deadline = System.nanoTime() + POLL_TIMEOUT;
            while (errorCode == Ubiquity.ErrorCode.PENDING && System.nanoTime() < deadline) {
                Thread.yield();
                errorCode = clientHandle.poll();
            }
            if (errorCode != Ubiquity.ErrorCode.SUCCESS) throw new RuntimeException("Poll timeout");
            if(clientHandle.getQueryId() != queryId) throw new RuntimeException("Did not poll the excepted queryId");
            if (count % dumpPeriod == 0) {
                System.out.printf("Total queries = %d, last response: predicted price = %f, conversion probability = %f, selected creative id = %d\n", count, clientHandle.getPrice(), clientHandle.getProbability(), clientHandle.getCreativeId());
            }

            // 4.3 Sending back conversion feedback (if any)
            // 4.3.1. send a notification of execution
            // notifies the cluster that the predicted pair (price, creative) have entered the auction
            // here we suppose that all predictions actually enter the auction
            // if the cluster is not notified of the execution, no feedback will be considered for the given query
            errorCode = clientHandle.train(modelId, queryId, 0);
            while(errorCode == Ubiquity.ErrorCode.BACK_PRESSURED) {
                Thread.yield();
                errorCode = clientHandle.train(modelId, queryId, 0);
            }

            // 4.3.2. read the feedback for all the levels and send positive feedback only if conversion occurred
            Feedback feedbackVector = feedbackIterator.next();
            // Try to send feedback of level 1
            if (feedbackVector.getLevel1()) {
                errorCode = clientHandle.train(modelId, queryId, 1);
                while (errorCode == Ubiquity.ErrorCode.BACK_PRESSURED) {
                    Thread.yield();
                    errorCode = clientHandle.train(modelId, queryId, 1);
                }
            }

            //  Try to send feedback of level 2
            if (feedbackVector.getLevel2()) {
                errorCode = clientHandle.train(modelId, queryId, 2);
                while (errorCode == Ubiquity.ErrorCode.BACK_PRESSURED) {
                    Thread.yield();
                    errorCode = clientHandle.train(modelId, queryId, 2);
                }
            }
        }
        clientHandle.close();
        Ubiquity.closeClient(clusterId);

        // 5. wait for the last feedbacks sent to be processed by the cluster then delete the models
        Thread.sleep(10000);
        apiUtil.deleteModel(modelId);
    }
}
