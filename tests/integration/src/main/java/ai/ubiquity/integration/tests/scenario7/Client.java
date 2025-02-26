package ai.ubiquity.integration.tests.scenario7;

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
     * Scenario 7: backpressure management
     * Setup:
     *     modelId X is served by nodes node_1, node_2 and node_3, modelId Y is served by nodes node_1 and node_2
     *     start three nodes
     *     the driver establishes the connection with the cluster and receives node update messages from the three nodes
     *     the driver fill the predict log buffer of node_1 and node_2 by continuously sending the same query for model Y (served by node_1 and node_2)
     *     the driver sends a query for model X (with a specific id such that consistent_hashing(query_id) = node_1)
     * Excepted behavior:
     *     the second pred query should be received by node_3 (since both node_1 and node_2 reply with a BACKPRESSURE)
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
    String modelId2 = "0000000001YG2"; //2000

    long queryId1 = 1L; //Sends to node_2
    long queryId2 = 13L; //Sends to node_2
    long queryId3 = 3L; //Sends to node_1
    // ----------------- QUERY#1 ---------------------------
    do {
      clientHandle.startLineItem(modelId2, queryId1, 2);
      clientHandle.putCreative(1);
      clientHandle.putNominal("test3");
      clientHandle.putNumeric(0.46f);
      clientHandle.putBoolean((byte) 0x01);
      clientHandle.putCategorical((byte)0x02);
    } while(clientHandle.predict() == Ubiquity.SUCCESS);

    // ----------------- QUERY#2 ---------------------------
    clientHandle.startLineItem(modelId2, queryId2, 2);
    clientHandle.putCreative(1);
    clientHandle.putNominal("test3");
    clientHandle.putNumeric(0.46f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putCategorical((byte)0x02);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.println("[General] query 2 sent");
    else System.out.println("[General] query 2 failed to be sent");


    // ----------------- QUERY#3 ---------------------------
    clientHandle.startLineItem(modelId1, queryId3, 2);
    clientHandle.putCreative(1);
    clientHandle.putNumeric(0.45f);
    clientHandle.putNumeric(130.f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putBoolean((byte)0x00);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.println("[General] query 3 sent");
    else System.out.println("[General] query 3 failed to be sent");

    Thread.sleep(1000);

    System.out.println("[General] closing client");

    clientHandle.close();
    Ubiquity.closeClient();

    Thread.sleep(500); // Wait for driver thread to shutdown propertly
  }
}