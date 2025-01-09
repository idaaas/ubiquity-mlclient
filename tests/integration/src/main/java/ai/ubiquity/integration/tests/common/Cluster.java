package ai.ubiquity.integration.tests.common;

import com.typesafe.config.ConfigFactory;
import io.aeron.Aeron;
import io.aeron.driver.MediaDriver;

import org.agrona.CloseHelper;

import com.typesafe.config.*;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import javax.naming.NameNotFoundException;

public class Cluster {
  public static void main(String[] args) throws InterruptedException, IOException, NameNotFoundException {
    String scenarioId = ConfigFactory.load().getString("name");
    System.out.printf("------------ Starting test %s ------------%n", scenarioId);

    MediaDriver mediaDriver = Utils.launchEmbeddedMediaDriver(scenarioId);
    final Aeron aeron = Utils.initAeron(mediaDriver);

    List<? extends Config> nodes = ConfigFactory.load().getConfigList("cluster");
    ArrayList<MockServer> servers = new ArrayList<>();

    try {
      for (Config node : nodes) {
        servers.add(new MockServer(node, aeron));
      }
      for (MockServer server : servers)
        server.start();

      
    } catch (Exception e) {
      System.out.printf("------------ Cluster failed ------------%n");
      System.out.printf("%s%n", e);
    } finally {
      for (MockServer server : servers) {
        server.join();
        server.close();
      }

      aeron.close();
      CloseHelper.close(mediaDriver);

      System.out.printf("------------ Test %s finished ------------%n", scenarioId);
    }

  }
}