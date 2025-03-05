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
              "contactPoints": ["10.20.0.5"]
            }""";
  }

  public static void main(String[] args) throws InterruptedException {


    /*
     * Scenario 9: Models integration
     * Setup:
     *    Each type of model sends a query to node_1 and expect to poll a response
     *
     Expected behavior:
     *     all model predict query and response are successful
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
    String modelActionRate = "0000000001YG2"; // 2000
    String modelProbabilityEstimator = "0000000002XR3"; // 3000
    String modelBidShading = "0000000003X04"; // 4000

    Thread.sleep(1000);

    long queryId1 = 1L;
    long queryId2 = 2L;
    long queryId3 = 3L;
    long queryId4 = 4L;
    // ----------------- QUERY#1 ---------------------------
    clientHandle.startLineItem(modelLineItem, queryId1, 2);
    clientHandle.putCreative(1);
    clientHandle.putNumeric(0.45f);
    clientHandle.putNumeric(130.f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putBoolean((byte)0x00);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.printf("[General] query %d sent%n", queryId1);
    else System.out.printf("[General] query %d failed to be sent%n", queryId1);

    long deadline = System.nanoTime() + POLL_TIMEOUT;
    do {
      statusCode = clientHandle.poll();
      if (statusCode == Ubiquity.SUCCESS) {
        System.out.printf("[General] prediction line-item received with price %f and probability %f%n", clientHandle.getPrice(), clientHandle.getProbability());
        break;
      }
      Thread.yield();
    } while (System.nanoTime() < deadline);

    clientHandle.startActionRate(modelActionRate, queryId2, 0.12f);
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
        System.out.printf("[General] prediction action-rate received with price %f%n", clientHandle.getPrice());
        break;
      }
      Thread.yield();
    } while (System.nanoTime() < deadline);

    clientHandle.startProbability(modelProbabilityEstimator, queryId3);
    clientHandle.putNumeric(0.45f);
    clientHandle.putNumeric(130.f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putBoolean((byte)0x00);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.printf("[General] query %d sent%n", queryId3);
    else System.out.printf("[General] query %d failed to be sent%n", queryId3);

    do {
      statusCode = clientHandle.poll();
      if (statusCode == Ubiquity.SUCCESS) {
        System.out.printf("[General] prediction probability-estimator received with probability %f%n", clientHandle.getProbability());
        break;
      }
      Thread.yield();
    } while (System.nanoTime() < deadline);

    clientHandle.startBidShading(modelBidShading, queryId4, 0.2f, 0.5f);
    clientHandle.putNumeric(0.45f);
    clientHandle.putNumeric(130.f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putBoolean((byte)0x00);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.printf("[General] query %d sent%n", queryId4);
    else System.out.printf("[General] query %d failed to be sent%n", queryId4);

    do {
      statusCode = clientHandle.poll();
      if (statusCode == Ubiquity.SUCCESS) {
        System.out.printf("[General] prediction bid-shading received with price %f and probability %f%n", clientHandle.getPrice(), clientHandle.getProbability());
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