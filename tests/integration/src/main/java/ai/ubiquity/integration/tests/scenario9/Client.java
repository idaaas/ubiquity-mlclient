package ai.ubiquity.integration.tests.scenario9;

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
              "contactPoints": ["10.20.0.5"],
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
                }
              ]
            }""";
  }

  public static void main(String[] args) throws InterruptedException {


    /*
     * Scenario 8: Poll predictions
     * Setup:
     *    modelId X is served by node#1 (only node in the cluster)
     *    the driver establishes the connection with the cluster and receives a node update message from node#1
     *    the driver sends 200 queries for model x
     *    the driver cancel the last query

     Expected behavior:
     *     the server should receive all 200 queries and reply with their predictions
     *     the server should receive one cancel
     */

    String apiToken = "totoken";
    String clusterId = "2c099MyN3lsVu";

    List<String> ips = new ArrayList<>();
    ips.add("10.20.0.5");

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

    Thread.sleep(1500);

    Ubiquity.setResourceStringInCache(clusterId, clusterData);

    String modelLineItem = "0000000000Z81"; // 1000
    String modelActionRate = "0000000000Z81"; // 2000
    String modelProbabilityEstimator = "0000000000Z81"; // 3000
    String modelBidShading = "0000000000Z81"; // 4000

    long queryId1 = 1L;
    long queryId2 = 2L;
    // ----------------- QUERY#1 ---------------------------
    clientHandle.startLineItem(modelLineItem, queryId1, 2);
    clientHandle.putCreative(1);
    clientHandle.putNumeric(0.45f);
    clientHandle.putNumeric(130.f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putBoolean((byte)0x00);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.printf("[General] query %d sent%n", queryId2);
    else System.out.printf("[General] query %d failed to be sent%n", queryId2);

    long deadline = System.nanoTime() + POLL_TIMEOUT;
    do {
      statusCode = clientHandle.poll();
      if (statusCode == Ubiquity.SUCCESS) {
        System.out.println("[General] prediction line-item received");
        break;
      }
      Thread.yield();
    } while (System.nanoTime() < deadline);

    clientHandle.startActionRate(modelActionRate, queryId1, 0.12f);
    clientHandle.putCreative(1);
    clientHandle.putNumeric(0.45f);
    clientHandle.putNumeric(130.f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putBoolean((byte)0x00);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.printf("[General] query %d sent%n", queryId2);
    else System.out.printf("[General] query %d failed to be sent%n", queryId2);

    do {
      statusCode = clientHandle.poll();
      if (statusCode == Ubiquity.SUCCESS) {
        System.out.println("[General] prediction action-rate received");
        break;
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