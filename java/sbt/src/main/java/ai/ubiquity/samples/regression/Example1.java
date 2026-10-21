package ai.ubiquity.samples.regression;

import ai.ubiquity.ClientHandle;
import ai.ubiquity.Ubiquity;
import ai.ubiquity.samples.utils.ApiUtil;
import ai.ubiquity.samples.utils.BidRequest;
import ai.ubiquity.samples.utils.CycleIterator;
import ai.ubiquity.samples.utils.Feedback;
import java.time.Duration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Random;
import java.io.IOException;

public class Example1 {
  private static final long POLL_TIMEOUT = Duration.ofMillis(200).toNanos();
  private static final int THROUGHPUT_IN_QPS = 300;
  private static final int DEFAULT_NUM_QUERIES = 500_000;
  private static final int DUMP_PERIOD = 1000;

  // Single cleanup routine used on every exit path.
  private static void cleanup(boolean clientOpened, ClientHandle clientHandle, boolean modelCreated, String modelId, ApiUtil apiUtil) {
    if (clientOpened) {
      clientHandle.close();
      Ubiquity.closeClient();
    }
    if (modelCreated) {
      System.out.println("Waiting 10 seconds for the last feedback to be processed...");
      try {
        Thread.sleep(10_000);
        apiUtil.deleteModel(modelId);
      } catch (InterruptedException ie) {
        Thread.currentThread().interrupt();
      } catch (IOException ioe) {
        System.err.println("Couldn't delete created model : " + ioe.getMessage());
      }
    }
  }

  public static void main(String[] args) throws Exception {
    if (args.length < 2){
      System.err.println("You need to provide API token and cluster ID to run the example");
      System.exit(2);
    }
    final String apiToken;apiToken = args[0];
    final String clusterId = args[1];
    final int numQueries = args.length >= 3 ? Integer.parseInt(args[2]) : DEFAULT_NUM_QUERIES;

    ApiUtil apiUtil = new ApiUtil(apiToken);
    ClientHandle clientHandle = null;
    String modelId = null;
    boolean clientOpened = false;
    boolean modelCreated = false;
    int exitCode = 0;

    try {
        // 1. Create your models via API
        modelId = apiUtil.createModel("regression", MODEL_SCHEMA.formatted(clusterId));
        if (modelId.isEmpty() || modelId == null) {
            throw new RuntimeException("Failed to create model");
        }
        modelCreated = true;

        // 2. Create and connect a ubiquity-client
        Ubiquity.setCredentials(apiToken);
        int statusCode = Ubiquity.openClient(clusterId);
        if (Ubiquity.SUCCESS != statusCode)
          throw new RuntimeException("Error during connection : " + statusCode);

        clientHandle = Ubiquity.getClientHandle();
        clientOpened = true;

        // 3. Prepare data and feedback streams
        Random rand = new Random(123); // to generate query ids
        // use cycle iterators in order to loop infinitely over the data
        Iterator<BidRequest> dataIterator = CycleIterator.bidRequestIterator("/client/datasets/features.csv.gz");
        Iterator<Feedback> feedbackIterator = CycleIterator.feedbackIterator("/client/datasets/feedback.csv.gz");
        HashMap<Long, Feedback> feedbackHashMap = new HashMap<>();

        // The next lines of code are useful for pacing the traffic sent to the ml
        // cluster based on a fixed throughput
        final long paceInNanos = Duration.ofSeconds(1).toNanos() / THROUGHPUT_IN_QPS;
        long nextSendNanoTime = System.nanoTime();

        // 4. Loop and send prediction queries with their associated feedbacks
        int count = 0;
        while (count < numQueries) {
          if (System.nanoTime() < nextSendNanoTime)
            continue;
          nextSendNanoTime = nextSendNanoTime + paceInNanos;

          // 4.1 prepare a regression query
          long queryId = rand.nextLong();
          // read a request with its feedback from the dataset file
          BidRequest request = dataIterator.next();
          Feedback feedback = feedbackIterator.next();
          // read the feedback for all the levels and store it in a temp map
          feedbackHashMap.put(queryId, feedback);
          // prepare a query using this request
          clientHandle.startRegression();
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
          clientHandle.putBoolean(request.getImpBannerTopFrame() ? (byte) 1 : (byte) 0);
          clientHandle.putCategorical(request.getDeviceType());
          clientHandle.putBoolean(request.getImpInstl() ? (byte) 1 : (byte) 0);
          clientHandle.putNominal(request.getAppPageCat());

          // 4.2. send the prediction request
          if (clientHandle.predict(modelId, queryId) == Ubiquity.SUCCESS)
            count++;

          // 4.3. wait to poll prediction response and validate its QueryId
          long deadline = System.nanoTime() + POLL_TIMEOUT;
          do {
            statusCode = clientHandle.poll();
            Thread.yield();
          } while (statusCode == Ubiquity.NO_DATA && System.nanoTime() < deadline);
          if (statusCode != Ubiquity.SUCCESS)
            continue;

          // 4.4 read the response data
          double predictionResponse = clientHandle.getPrediction();

          // 4.5 send a positive feedback of level_0 to the cluster.
          // After calling the feedback level 0, no cancellation is allowed
          clientHandle.train(modelId, queryId, "level_0");

          if (count % DUMP_PERIOD == 0) {
            System.out.printf(
                "Total queries = %d, last response: prediction response = %d\n", count, predictionResponse);
          }
        }
    } 
    catch (Exception e) {
      System.err.println("Fatal error: " + e.getMessage());
      exitCode = 1;
    }
    finally {
      //  close the opened resources
      cleanup(clientOpened, clientHandle, modelCreated, modelId, apiUtil);
    }

    // System.exit is called only after all resources are closed.
    System.exit(exitCode);
  }

  private final static String MODEL_SCHEMA = """
      {
        "cluster_id": "%s",
        "name": "regression/Example1",
        "feedbacks": [
          {
            "name": "level_0",
            "max_delay": 65
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
