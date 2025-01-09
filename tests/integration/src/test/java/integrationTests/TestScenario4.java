package integrationTests;

import assertion.AssertionRule;
import assertion.IntegrationAssert;
import common.CommonAssertions;
import common.ResourceRetriever;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;

public class TestScenario4 {
  IntegrationAssert server, client;

  @BeforeEach
  void setup() {
    String serverPath = ResourceRetriever.getResource("it-scenario4.log");
    String clientPath = ResourceRetriever.getResource("client-scenario4.log");

    server = new IntegrationAssert(serverPath);
    client = new IntegrationAssert(clientPath);
  }

  @Test
  void ClusterNode4() {
    // should respond properly to handshake protocol
    AssertionRule rule = CommonAssertions.assertServerRegularHandshake(server.node("node_4").shouldLog("Starts polling"));
    rule.then.shouldLogAtLeast("apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => NONE", 1)
            // should send model 1000 and NOT 2000
            .and.shouldLog("apply rule[8.0] TIME_MS.NONE => MODEL_SCHEMA_UPDATE")
            .then.shouldLog("Sending model Id 1000")
            .and.shouldNotLog("Sending model Id 2000")
            // should receive query function of load balancing
            .then.shouldLog("prediction query with id 1002 received with type 05 and application 02")
            .then.shouldLog("apply rule[3.0] CONNECTION_REQUEST.DISCONNECTING_REQUEST => CONNECTION_RESPONSE.DISCONNECTION_CONFIRMATION_RESPONSE");
  }

  @Test
  void ClusterNode3() {
    // should respond properly to handshake protocol
    AssertionRule rule = CommonAssertions.assertServerRegularHandshake(server.node("node_3").shouldLog("Starts polling"));
    rule.then.shouldLogAtLeast("apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => NONE", 1)
            // should send model 1000 and NOT 2000
            .and.shouldLog("apply rule[8.0] TIME_MS.NONE => MODEL_SCHEMA_UPDATE")
            .then.shouldLog("Sending model Id 1000")
            .and.shouldNotLog("Sending model Id 2000")
            // should receive query function of load balancing
            .then.shouldLog("prediction query with id 1001 received with type 05 and application 02")
            .then.shouldLog("apply rule[3.0] CONNECTION_REQUEST.DISCONNECTING_REQUEST => CONNECTION_RESPONSE.DISCONNECTION_CONFIRMATION_RESPONSE");
  }

  @Test
  void ClusterNode2() {
    // should respond properly to handshake protocol
    AssertionRule rule = CommonAssertions.assertServerRegularHandshake(server.node("node_2").shouldLog("Starts polling"));
    rule.then.shouldLogAtLeast("apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => NONE", 1)
            // should send model 1000 and 2000
            .and.shouldLog("apply rule[8.0] TIME_MS.NONE => MODEL_SCHEMA_UPDATE")
            .then.shouldLog("Sending model Id 1000")
            .and.shouldLog("Sending model Id 2000")
            // should receive query with load balancing
            .then.shouldLog("prediction query with id 2003 received with type 05 and application 02")
            .then.shouldLog("apply rule[3.0] CONNECTION_REQUEST.DISCONNECTING_REQUEST => CONNECTION_RESPONSE.DISCONNECTION_CONFIRMATION_RESPONSE");
  }

  @ParameterizedTest
  @ValueSource(strings = {"(10.20.0.9)", "(10.20.0.13)"})
  void nodeClients(String ip) {
    // should respond properly to handshake protocol
    AssertionRule rule = CommonAssertions.assertClientHandshake(client.nodeClient(ip))
            // should receive one model
            .then.shouldLog("[0001] [CLIENT_READY] : Received MODEL_ADD (34 bytes)");
  }

  @Test
  void nodeClientWith2Models() {
    // should respond properly to handshake protocol
    AssertionRule rule = CommonAssertions.assertClientHandshake(client.nodeClient("(10.20.0.5)"))
            // should receive both models
            .then.shouldLog("[0001] [CLIENT_READY] : Received MODEL_ADD (34 bytes)", 2);
  }
}
