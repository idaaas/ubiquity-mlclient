package ai.ubiquity.integration.tests.scenario4;

import ai.ubiquity.*;
import ai.ubiquity.integration.tests.common.MockConfig;

import com.typesafe.config.*;

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
              "contactPoints": ["10.20.0.1", "10.20.0.5", "10.20.0.9", "10.20.0.13"],
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
     * Scenario 4: Full cluster visibility and node selection, then losing connection ungracefully
     * Setup:
     *     all nodes node#1, node#2, node#3 serve modelId X, only node#2 serves modelId Y
     *     the driver establishes the connection with the cluster and receives node update messages from the three nodes
     *     the driver sends a query#1 for model X with a specific id such that consistent_hashing(query_id) = node#3
     *     the driver sends a query#2 for model X with a specific id such that consistent_hashing(query_id) = node#4
     *     the driver sends a query#3 with a random id for model Y
     * Expected behavior:
     *     the pred query#1 should be received by node#3
     *     the pred query#2 should be received by node#4
     *     the pred query#3 should be received by node#2
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

    Thread.sleep(2000);

    Ubiquity.setResourceStringInCache(clusterId, clusterData);

    String modelId1 = "0000000000Z81"; //1000
    String modelId2 = "0000000001YG2"; //2000

    long queryId1 = 1L;
    long queryId2 = 2L;
    long queryId3 = 3L;
    // ----------------- QUERY#1 ---------------------------
    clientHandle.startLineItem(modelId1, queryId1, 2);
    clientHandle.putCreative(1);
    clientHandle.putNumeric(0.45f);
    clientHandle.putNumeric(0.46f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putBoolean((byte)0x00);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.println("[General] query 1 sent");
    else System.out.println("[General] query 1 failed to be sent");

    // ----------------- QUERY#2 ---------------------------
    clientHandle.startLineItem(modelId1, queryId2, 2);
    clientHandle.putCreative(1);
    clientHandle.putNumeric(0.45f);
    clientHandle.putNumeric(0.46f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putBoolean((byte)0x00);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.println("[General] query 2 sent");
    else System.out.println("[General] query 2 failed to be sent");


    // ----------------- QUERY#3 ---------------------------
    clientHandle.startLineItem(modelId2, queryId3, 2);
    clientHandle.putCreative(1);
    clientHandle.putNominal("test3");
    clientHandle.putNumeric(130.f);
    clientHandle.putBoolean((byte)0x01);
    clientHandle.putCategorical((byte)0x02);
    if (clientHandle.predict() == Ubiquity.SUCCESS) System.out.println("[General] query 3 sent");
    else System.out.println("[General] query 3 failed to be sent");


    Thread.sleep(1000);

    System.out.println("[General] closing client");

    clientHandle.close();
    Ubiquity.closeClient();

    Thread.sleep(500); // Wait for driver thread to shutdown propertly
  }
}