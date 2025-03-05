package integrationTests;

import assertion.IntegrationAssert;
import common.CommonAssertions;
import common.ResourceRetriever;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;

public class TestScenario7 {
  IntegrationAssert server, client;

  @BeforeEach
  void setup() {
    String serverPath = ResourceRetriever.getResource("it-scenario7.log");
    String clientPath = ResourceRetriever.getResource("client-scenario7.log");

    server = new IntegrationAssert(serverPath);
    client = new IntegrationAssert(clientPath);
  }

  @ParameterizedTest
  @ValueSource(strings = {"node_1", "node_2", "node_3"})
  void Cluster(String id) {
    // should respond properly to handshake protocol
    CommonAssertions.assertServerRegularHandshake(server.node(id).shouldLog("Starts polling"));
  }

  @Test
  void ClusterConsistentHashing() {
    // should respond properly to handshake protocol
    server.anyNode()
            .shouldLog("[Server node_2] apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => MODEL_SCHEMA_UPDATE  1 0")
            .and.shouldLog("[Server node_1] apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => MODEL_SCHEMA_UPDATE  1 0")
            .and.shouldLog("[Server node_3] apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => MODEL_SCHEMA_UPDATE  1 0")
            // all node should send their models
            .and.shouldLog("[Server node_2] Sending model Id 1000")
            .and.shouldLog("[Server node_1] Sending model Id 1000")
            .and.shouldLog("[Server node_3] Sending model Id 1000")
            .and.shouldLog("[Server node_1] Sending model Id 2000")
            .and.shouldLog("[Server node_2] Sending model Id 2000")
            // node_2 & node_1 should receive query with id 2001 multiple times until backpressure
            .then.shouldLogAtLeast("[Server node_2] prediction query with id 2001 received with type 05 and application 02", 10)
            .and.shouldLogAtLeast("[Server node_1] prediction query with id 2001 received with type 05 and application 02", 10)
            // node_3 should receive query with id 1003 because of backpressure
            .and.shouldLog("[Server node_3] prediction query with id 1003 received with type 05 and application 02");
  }

  @ParameterizedTest
  @ValueSource(strings = {"(10.20.0.5)", "(10.20.0.9)", "(10.20.0.13)"})
  void Client(String ip) {
    CommonAssertions.assertClientHandshake(client.nodeClient(ip));
  }

  @Test
  void ClientModelScenario() {
    client.anyNodeClient().shouldLog("NodeClient 0 (10.20.0.5) [0001] [CLIENT_READY] : Received MODEL_ADD (34 bytes)", 2)
            .and.shouldLog("NodeClient 1 (10.20.0.9) [0001] [CLIENT_READY] : Received MODEL_ADD (34 bytes)", 2)
            .and.shouldLog("NodeClient 2 (10.20.0.13) [0001] [CLIENT_READY] : Received MODEL_ADD (34 bytes)")
            .and.shouldLog("Inserting new schema(1000) in cache")
            .then.shouldLog("Inserting new schema(2000) in cache")
            // the second query with id 2 should fail because of backpressure
            .then.shouldLog("[General] query 2 failed to be sent")
            // the third query should be sent to node_3 instead of node_1 because of consistent hashing
            .then.shouldLog("[General] query 3 sent");
  }
}
