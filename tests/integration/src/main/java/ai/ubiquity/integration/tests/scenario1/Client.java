package ai.ubiquity.integration.tests.scenario1;

import ai.ubiquity.*;

import java.util.ArrayList;
import java.util.List;

public class Client {

  public static void main(String[] args) {

    /*
     * Scenario 1: Finding the singleton actor and establishing connection to all
     *  nodes in a healthy cluster
     * Setup:
     *  node_2 hosts the singleton actor
     *  node_2 has a delay in topology update so every connected node must see every connected node sending a topology request
     *  node_4 is not in the list of contact points
     *  The client starts the connection process
     * Expected behavior:
     *  the driver finds the missing node and connects to every node in the cluster
     *  including node_4
     *  every connected node sending a topology request except node client created after the topology update of the singleton
     */

    String apiToken = "totoken";
    String clusterId = "MyClusterId";

    List<String> ips = new ArrayList<>();
    ips.add("10.20.0.1");
    ips.add("10.20.0.5");
    ips.add("10.20.0.9");

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
      Thread.sleep(2000);
    } catch (InterruptedException e) {
      e.printStackTrace(); // Handle the exception
    }

    System.out.println("[General] closing client");

    clientHandle.close();
    Ubiquity.closeClient();

    try {
      Thread.sleep(500); // Wait for driver thread to shutdown propertly
    } catch (InterruptedException e) {
      e.printStackTrace(); // Handle the exception
    }
  }
}