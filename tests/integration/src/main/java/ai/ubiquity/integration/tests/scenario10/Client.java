package ai.ubiquity.integration.tests.scenario10;

import ai.ubiquity.ClientHandle;
import ai.ubiquity.Ubiquity;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;

public class Client {
  private static final long POLL_TIMEOUT = Duration.ofMillis(200).toNanos();

  public static String config() {
    return """
            {
              "config": {
                "termBufferSize": 65536,
                "aeronDir": "/dev/shm/ubiquity-driver"
              },
              "contactPoints": ["10.20.0.5", "10.20.0.9", "10.20.0.13"],
              "models": [
                {
                  "model_id": 1000,
                  "cluster_id": "2c099MyN3lsVu",
                  "tenant_id": "2c0A9GOwyZxlP",
                  "name": "Campaign Title",
                  "feedback_delays": [
                    90,
                    360
                  ],
                  "features": [
                    {"type": "numeric", "name": "a1"},
                    {"type": "numeric", "name": "a2"},
                    {"type": "boolean", "name": "a3"},
                    {"type": "boolean", "name": "a4"}
                  ]
                },
                {
                  "model_id": 2000,
                  "cluster_id": "2c099MyN3lsVu",
                  "tenant_id": "2c0A9GOwyZxlP",
                  "name": "Campaign Title",
                  "min_bid_price": 0.01,
                  "max_bid_price": 30,
                  "feedback_delays": [
                    90,
                    360
                  ],
                  "features": [
                    {"type": "nominal", "name": "a1"},
                    {"type": "numeric", "name": "a2"},
                    {"type": "boolean", "name": "a3"},
                    {"type": "categorical", "name": "a4"}
                  ]
                }
              ]
            }""";
  }

  public static void main(String[] args) throws InterruptedException {


    /*
     * Scenario 6: query should not be usable twice with same query id (convert this as TU when available)
     * Setup:
     *     node_2 serves modelId X
     *     the driver establishes a connection and send one query
     *     the call to predict is repeated without the setup of a query with a new query Id
     *
     * Current behavior:
     *     the Query can be sent for ever.
     *     when the first poll is received, the buffer of the query is replaced by the response
     *     the new overlap buffer is sent with the predict call instead of the query
     * Expected behavior:
     *     The pred query#1 is received by node_2
     *     The other queries should return errors
     */

    String apiToken = "totoken";
    String clusterId = "2c099MyN3lsVu";

    List<String> ips = new ArrayList<>();
    ips.add("10.20.0.5");
    ips.add("10.20.0.9");
    ips.add("10.20.0.13");

    String clusterData = "{ \"contact_points\": [";
    for (String ip : ips) {
      clusterData += "\"" + ip + "\",";
    }
    clusterData = clusterData.substring(0, clusterData.length() - 1);
    clusterData += "] }";
    System.out.printf("[General] Cluster data : %s%n", clusterData);

    Ubiquity.setResourceStringInCache(clusterId, clusterData);

    System.out.println("[General] start");

    Ubiquity.setCredential(apiToken);
    System.out.println("[General] openclient");
    int statusCode = Ubiquity.openClient(clusterId);
    System.out.println("[General] client opened");
    if (Ubiquity.SUCCESS != statusCode)
      throw new RuntimeException("[General] Error during connection : " + statusCode);
    ClientHandle clientHandle = Ubiquity.getClientHandle();

    Thread.sleep(3500);

    Ubiquity.setResourceStringInCache(clusterId, clusterData);

    String modelId1 = "0000000000Z81"; //1000

    long queryId1 = 1L;
    // ----------------- QUERY#1 ---------------------------
    clientHandle.startLineItem(modelId1, queryId1, 2);
    clientHandle.putCreative(1);
    clientHandle.putNumeric(0.45f);
    clientHandle.putNumeric(0.46f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putBoolean((byte)0x00);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.println("[General] query 1 sent");
    else System.out.println("[General] query 1 failed to be sent");

    long deadline = System.nanoTime() + POLL_TIMEOUT;
    int receivedFeedback = 0;
    do {
      statusCode = clientHandle.poll();
      if (statusCode == Ubiquity.SUCCESS) {
        receivedFeedback++;
        if (receivedFeedback >= 10) break;
        clientHandle.predict();
      }
      Thread.yield();
    } while (System.nanoTime() < deadline);


    Thread.sleep(1000);

    System.out.println("[General] closing client");

    clientHandle.close();
    Ubiquity.closeClient();

    Thread.sleep(500); // Wait for driver thread to shutdown propertly
  }
}