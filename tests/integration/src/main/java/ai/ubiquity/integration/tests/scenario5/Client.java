package ai.ubiquity.integration.tests.scenario5;

import ai.ubiquity.ClientHandle;
import ai.ubiquity.Ubiquity;

import java.util.ArrayList;
import java.util.List;

public class Client {

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
                }
              ]
            }""";
  }

  public static void main(String[] args) throws InterruptedException {


    /*
     * Scenario 5: A node is serving a new model
     * Setup:
     *     node_2 serves modelId X
     *     the driver establishes the connection with the cluster and receives node update messages from the three nodes
     *     the query#1 is selected such as:
     *         consistent_hashing(query_id) = node_2 if the list of nodes serving model X contain only node_2
     *         consistent_hashing(query_id) = node_1 if the list of nodes serving model X contain node_1 and node_2
     *     the driver tries to send a query#1 for model X
     *     node_1 sends a node update (modelId X is now served by node_1 and node_2)
     *     the driver sends a query#2 with the same id1 for model X
     *     the node_1 sends a node update removing modelId X
     *     the driver sends a query#3 with the same id1 for model X
     *
     * Expected behavior:
     *     The pred query#1 is received by node_2
     *     The pred query#2 is received by node_1
     *     The pred query#3 is received by node_2
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

    Thread.sleep(1500);

    Ubiquity.setResourceStringInCache(clusterId, clusterData);

    String modelId1 = "0000000000Z81"; //1000

    long queryId1 = 1L;

    Thread.sleep(1000);
    // ----------------- QUERY#1 ---------------------------
    clientHandle.startLineItem(modelId1, queryId1, 2);
    clientHandle.putCreative(1);
    clientHandle.putNumeric(0.45f);
    clientHandle.putNumeric(0.46f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putBoolean((byte)0x00);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.printf("[General] query with id %d sent%n", queryId1);
    else System.out.printf("[General] query with id %d failed to be sent%n", queryId1);

    Thread.sleep(1000);
    // ----------------- QUERY#2 ---------------------------
    clientHandle.startLineItem(modelId1, queryId1, 2);
    clientHandle.putCreative(1);
    clientHandle.putNumeric(0.45f);
    clientHandle.putNumeric(0.46f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putBoolean((byte)0x00);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.printf("[General] query with id %d sent%n", queryId1);
    else System.out.printf("[General] query with id %d failed to be sent%n", queryId1);

    Thread.sleep(2000);
    // ----------------- QUERY#2 ---------------------------
    clientHandle.startLineItem(modelId1, queryId1, 2);
    clientHandle.putCreative(1);
    clientHandle.putNumeric(0.45f);
    clientHandle.putNumeric(0.46f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putBoolean((byte)0x00);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.printf("[General] query with id %d sent%n", queryId1);
    else System.out.printf("[General] query with id %d failed to be sent%n", queryId1);

    Thread.sleep(500);

    System.out.println("[General] closing client");

    clientHandle.close();
    Ubiquity.closeClient();

    Thread.sleep(500); // Wait for driver thread to shutdown propertly
  }
}