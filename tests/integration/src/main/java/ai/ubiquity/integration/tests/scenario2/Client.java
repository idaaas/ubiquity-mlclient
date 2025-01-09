package ai.ubiquity.integration.tests.scenario2;

import ai.ubiquity.*;
import ai.ubiquity.integration.tests.common.MockConfig;

import com.typesafe.config.*;

import java.util.ArrayList;
import java.util.List;

public class Client {

  public static void main(String[] args) {

    
    /**
     * Scenario 2: Finding the singleton when a contact point is unreachable
     * Setup:
     *     node_3 hosts the singleton actor
     *     node_1 is unreachable/dead and the driver starts the connection process with outdated list of contact points.
     *     after a while, the entire cluster shut down gracefully
     *     the driver is finally closed
     * Expected behavior:
     *     the driver sends a topology request to node_2 then node_3 after it failed to contact the node_1
     *     the driver should try to call the API regularly to try to reconnect
     *     after the call to close, the driver should stop trying to reconnect and close
     */

    String apiToken = "totoken";
    String clusterId = "MyClusterId";

    List<String> ips = new ArrayList<>();
    ips.add("10.20.0.1");
    ips.add("10.20.0.5");
    ips.add("10.20.0.9");
    ips.add("10.20.0.13");

    String clusterData = "{ \"contact_points\": [";
    for (String ip : ips) {
      clusterData += "\"" + ip + "\",";
    }
    clusterData = clusterData.substring(0, clusterData.length() - 1);
    clusterData += "] }";

    Ubiquity.setResourceStringInCache(clusterId, clusterData);

    System.out.println("[General] start");

    Ubiquity.setCredential(apiToken);
    System.out.println("[General] openclient");
    int statusCode = Ubiquity.openClient(clusterId);
    System.out.println("[General] client opened");
    if (Ubiquity.SUCCESS != statusCode)
      throw new RuntimeException("[General] Error during connection : " + statusCode);
    ClientHandle clientHandle = Ubiquity.getClientHandle();

    //Set resource again to reconnect after disconnection
    Ubiquity.setResourceStringInCache(clusterId, clusterData);

    try {
      Thread.sleep(30000);
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