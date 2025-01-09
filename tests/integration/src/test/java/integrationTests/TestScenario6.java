package integrationTests;

import assertion.AssertionRule;
import assertion.IntegrationAssert;
import common.CommonAssertions;
import common.ResourceRetriever;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;

public class TestScenario6 {
  IntegrationAssert server, client;

  @BeforeEach
  void setup() {
    String serverPath = ResourceRetriever.getResource("it-scenario6.log");
    String clientPath = ResourceRetriever.getResource("client-scenario6.log");

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
            .shouldLog("[Server node_2] apply rule[8.0] TIME_MS.NONE => MODEL_SCHEMA_UPDATE  0 1000")
            .and.shouldLog("[Server node_1] apply rule[8.0] TIME_MS.NONE => MODEL_SCHEMA_UPDATE  0 1000")
            // node_1 and node_2 should send their models
            .then.shouldLog("[Server node_2] Sending model Id 1000")
            .and.shouldLog("[Server node_1] Sending model Id 1000")
            // node_2 should receive query with id 994 first
            .then.shouldLog("[Server node_2] prediction query with id 994 received with type 05 and application 02")
            // node_2 should send a node remove request
            .then.shouldLog("[Server node_2] apply rule[9.0] TIME_MS.NONE => TOPOLOGY_UPDATE.REMOVE node_2")
            // node_1 should receive the second request
            .then.shouldLog("[Server node_1] prediction query with id 994 received with type 05 and application 02");
  }

  @Test
  void Node2() {
    // should respond properly to handshake protocol
    AssertionRule rule = CommonAssertions.assertServerRegularHandshake(server.node("node_2").shouldLog("Starts polling"));
    // should send it's model
    rule.then.shouldLog("apply rule[8.0] TIME_MS.NONE => MODEL_SCHEMA_UPDATE  0 1000")
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
    rule.then.shouldLog("apply rule[8.0] TIME_MS.NONE => MODEL_SCHEMA_UPDATE  0 1000")
            // should not send any model
            .then.shouldNotLog("Sending model Id 1000")
            .then.shouldNotLog("prediction query");
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
            .and.shouldLog("NodeClient 0 (10.20.0.5) [0001] [CLIENT_READY] : Received MODEL_ADD (34 bytes)")
            .then.shouldLog("Inserting new schema(1000) in cache")
            // send one query
            .then.shouldLog("[General] query with id 10 sent")
            // client 1 should receive a disconnection request
            .then.shouldLog("NodeClient 1 (10.20.0.9) [0001] [CLIENT_READY] : Received TOPOLOGY_UPDATE")
            // send an other query with the same Id
            .then.shouldLog("[General] query with id 10 sent");
  }
}
