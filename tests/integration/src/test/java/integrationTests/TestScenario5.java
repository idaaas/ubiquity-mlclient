package integrationTests;

import assertion.AssertionRule;
import assertion.IntegrationAssert;
import common.CommonAssertions;
import common.ResourceRetriever;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;

public class TestScenario5 {
  IntegrationAssert server, client;

  @BeforeEach
  void setup() {
    String serverPath = ResourceRetriever.getResource("it-scenario5.log");
    String clientPath = ResourceRetriever.getResource("client-scenario5.log");

    server = new IntegrationAssert(serverPath);
    client = new IntegrationAssert(clientPath);
  }

  @ParameterizedTest
  @ValueSource(strings = {"node_1", "node_2", "node_3"})
  void Cluster(String id) {
    // should respond properly to handshake protocol
    AssertionRule rule = CommonAssertions.assertServerRegularHandshake(server.node(id).shouldLog("Starts polling"));
  }

  @Test
  void Node1And2() {
    // should respond properly to handshake protocol
    server.anyNode()
            .shouldLog("[Server node_2] apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => MODEL_SCHEMA_UPDATE  1 0")
            .and.shouldLog("[Server node_3] apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => MODEL_SCHEMA_UPDATE  1 0")
            // node_2 send it's model, node_1 does not have a model yet
            .and.shouldLog("[Server node_2] Sending model Id 1000")
            // node_2 should receive query with id 1001 first
            .then.shouldLog("[Server node_2] prediction query with id 1001 received with type 05 and application 02")
            // node_1 should send it's model
            .then.shouldLog("[Server node_1] apply rule[7.0] CONNECTION_REQUEST.HEARTBEAT => ADD_MODEL node_1 1 0")
            .then.shouldLog("[Server node_1] Sending model Id 1000")
            // should receive query with id 1001 first
            .then.shouldLog("[Server node_1] prediction query with id 1001 received with type 05 and application 02")
            // should remove model for node_1
            .then.shouldLog("[Server node_1] apply rule[7.2] CONNECTION_REQUEST.HEARTBEAT => REMOVE_MODEL node_1 1 0")
            .then.shouldLog("[Server node_1] Removing model Id 1000")
            // node_2 should receive the second request
            .then.shouldLog("[Server node_2] prediction query with id 1001 received with type 05 and application 02");
  }

  @Test
  void Node2() {
    // should respond properly to handshake protocol
    AssertionRule rule = CommonAssertions.assertServerRegularHandshake(server.node("node_2").shouldLog("Starts polling"));
    // should send it's model
    rule.then.shouldLog("apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => MODEL_SCHEMA_UPDATE  1 0")
            // should send a model before node_1
            .then.shouldLog("Sending model Id 1000")
            // should receive query with id 1001 first
            .then.shouldLog("prediction query with id 1001 received with type 05 and application 02");
  }
  @Test
  void Node3() {
    // should respond properly to handshake protocol
    AssertionRule rule = CommonAssertions.assertServerRegularHandshake(server.node("node_3").shouldLog("Starts polling"));
    // should send it's model (this node has none)
    rule.then.shouldLog("apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => MODEL_SCHEMA_UPDATE  1 0")
            // should not send any model
            .then.shouldNotLog("Sending model Id 1000")
            .then.shouldNotLog("prediction query with id 1001 received with type 05 and application 02");
  }

  @ParameterizedTest
  @ValueSource(strings = {"(10.20.0.5)", "(10.20.0.9)", "(10.20.0.13)"})
  void Client(String ip) {
    CommonAssertions.assertClientHandshake(client.nodeClient(ip));
  }

  @Test
  void ClientModelScenario() {
    client.anyNodeClient().shouldLog("NodeClient 1 (10.20.0.9) [0001] [CLIENT_READY] : Received MODEL_ADD (34 bytes)")
            .and.shouldNotLog("NodeClient 1 (10.20.0.13) [0001] [CLIENT_READY] : Received MODEL_ADD (34 bytes)")
            .then.shouldLog("Inserting new schema(1000) in cache")
            .then.shouldLog("[General] query with id 1 sent")
            .then.shouldLog("NodeClient 0 (10.20.0.5) [0001] [CLIENT_READY] : Received MODEL_ADD (34 bytes)")
            .then.shouldLog("[General] query with id 1 sent")
            .then.shouldLog("NodeClient 0 (10.20.0.5) [0001] [CLIENT_READY] : Received MODEL_REMOVE (16 bytes)")
            .then.shouldLog("[General] query with id 1 sent");
  }
}
