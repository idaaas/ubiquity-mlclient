package integrationTests;

import assertion.AssertionRule;
import assertion.IntegrationAssert;
import common.CommonAssertions;
import common.ResourceRetriever;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;

public class TestScenario2 {
  IntegrationAssert server, client;

  @BeforeEach
  void setup() {
    String serverPath = ResourceRetriever.getResource("it-scenario2.log");
    String clientPath = ResourceRetriever.getResource("client-scenario2.log");

    server = new IntegrationAssert(serverPath);
    client = new IntegrationAssert(clientPath);
  }

  @ParameterizedTest
  @ValueSource(strings = {"node_2", "node_3", "node_4"})
  void ServerTests(String nodeId) {
    // should start a handshake protocol
    AssertionRule rule = CommonAssertions.assertServerRegularHandshake(server.node(nodeId).shouldLog("Starts polling"))
            // the server sends a graceful disconnection request
            .then.shouldLog("CONNECTION_REQUEST.HEARTBEAT => TOPOLOGY_UPDATE.REMOVE")
            // the server should receive a disconnection request from the client
            .then.shouldLog("apply rule[3.0] CONNECTION_REQUEST.DISCONNECTING_REQUEST => CONNECTION_RESPONSE.DISCONNECTION_CONFIRMATION_RESPONSE");
    // should start a new handshake protocol after some time
    CommonAssertions.assertServerRegularHandshake(rule);
  }

  @ParameterizedTest
  @ValueSource(strings = {"(10.20.0.5)", "(10.20.0.9)", "(10.20.0.13)"})
  void ClientTests(String ip) {
    // should start a handshake protocol
    AssertionRule rule = CommonAssertions.assertClientHandshake(client.nodeClient(ip))
            // the node client should receive a topology update (node remove) to disconnect gracefully
            .then.shouldLog("[0001] [CLIENT_READY] : Received TOPOLOGY_UPDATE (12 bytes)")
            // the client should ask to disconnect
            .then.shouldLog("[0001] [CLIENT_READY] : Switched to state DISCONNECTING")
            .then.shouldLog("[0001] [DISCONNECTING] : Sending DISCONNECTION_REQUEST")
            // the client should receive a disconnection confirmation
            .then.shouldLog("[0001] [DISCONNECTING] : Received DISCONNECTION_CONFIRMATION (4 bytes)")
            // the client should move to an unbound state
            .then.shouldLog("[ffff] [DISCONNECTING] : Switched to state UNBOUND");
    // After a while, it should start a handshake protocol
    CommonAssertions.assertClientHandshake(rule, "0002");
  }

  @Test
  void UnboundNodeClient() {
    // this node does not exist and should not be able to connect
    client.nodeClient("(10.20.0.1)")
            .then.shouldLog("[ffff] [UNBOUND] : Switched to state DISCONNECTED")
            .then.shouldLog("[ffff] [DISCONNECTED] : Switched to state UNBOUND");
  }
}
