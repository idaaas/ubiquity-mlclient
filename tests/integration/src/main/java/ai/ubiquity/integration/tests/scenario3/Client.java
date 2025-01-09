package ai.ubiquity.integration.tests.scenario3;

import ai.ubiquity.*;
import ai.ubiquity.integration.tests.common.MockConfig;

import com.typesafe.config.*;

import java.util.ArrayList;
import java.util.List;

public class Client {

  public static void main(String[] args) {

    
    /**
     * Scenario 3: Full connection to a node
     * Setup:
     *     the cluster contains two node, list of contact points [node_1, node_2]
     *     node_2 hosts the singleton actor
     *     The client starts the connection process then calls driver.close()
     *
     * Expected behavior:
     *     the driver sends a topology request to node_2. node_2 containing the singleton replies with the cluster topology.
     *     the driver should send a registration_request to node_2. node_2 sends a correlation conflict.
     *     the driver should resend a registration_request to node_2 with a different correlation id. node_2 replies with a registration_response.
     *     the driver should send a server_open_request. node_2 ignores this request.
     *     the driver should send another server_open_request. node_2 replies with server_open_response.
     *     the driver should send a server_ready_request. node_2 ignores this request twice.
     *     the driver should resend the same request. node_2 sends a server_ready_response.
     *     the driver should send a heartbeat every 1s.
     *     the driver should follow the same steps to connect to node_1
     *     the driver should send two disconnection messages to the two nodes in the cluster. node_1 replies with a disconnection response. node_2 ignores the messages
     *     the driver should close all its aeron connections (publications and subscriptions) related to node_2
     *     the driver should also close all its aeron connections (publications and subscriptions) related to node_1 after the disconnection timeout
     */

    String apiToken = "totoken";
    String clusterId = "MyClusterId";

    List<String> ips = new ArrayList<>();
    ips.add("10.20.0.1");
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

    try {
      Thread.sleep(3000);
    } catch (InterruptedException e) {
      e.printStackTrace(); // Handle the exception
    }

    System.out.println("[General] closing client");

    clientHandle.close();
    Ubiquity.closeClient();

    try {
      Thread.sleep(1500); // Wait for driver thread to shutdown propertly
    } catch (InterruptedException e) {
      e.printStackTrace(); // Handle the exception
    }
  }
}