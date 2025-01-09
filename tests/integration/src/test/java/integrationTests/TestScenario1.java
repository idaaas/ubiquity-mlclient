package integrationTests;

import assertion.IntegrationAssert;
import common.CommonAssertions;
import common.ResourceRetriever;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;

public class TestScenario1 {

  IntegrationAssert server, client;

  @BeforeEach
  void setup() {
    String serverPath = ResourceRetriever.getResource("it-scenario1.log");
    String clientPath = ResourceRetriever.getResource("client-scenario1.log");

    server = new IntegrationAssert(serverPath);
    client = new IntegrationAssert(clientPath);
  }

  @ParameterizedTest
  @ValueSource(strings = {"node_1", "node_2", "node_3"})
  void ServerTests(String nodeId) {
    CommonAssertions.assertServerRegularHandshake(server.node(nodeId).shouldLog("Starts polling"))
            .then.shouldLog("apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => NONE")
            .then.shouldLog("apply rule[3.0] CONNECTION_REQUEST.DISCONNECTING_REQUEST => CONNECTION_RESPONSE.DISCONNECTION_CONFIRMATION_RESPONSE");
  }

  //node 4 should not receive topology request, as it is discovered by the TOPOLOGY_UPDATE message received by other node clients
  @Test
   void Node4Test() {
    IntegrationAssert a = new IntegrationAssert("/home/christophe/GIT/ubiquity-mlclient-private/tests/test-output/it-scenario1.log");
    server.node("node_4").shouldLog("Starts polling")
            .then.shouldNotLog("apply rule[1.0] TOPOLOGY_REQUEST.NONE => TOPOLOGY_UPDATE")
            .then.shouldLog("apply rule[2.0] CONNECTION_REQUEST.REGISTRATION_REQUEST => CONNECTION_RESPONSE.REGISTRATION_RESPONSE")
            .then.shouldLog("apply rule[4.0] CONNECTION_REQUEST.SERVER_OPEN_REQUEST => CONNECTION_RESPONSE.SERVER_OPENED_RESPONSE")
            .then.shouldLog("apply rule[5.0] CONNECTION_REQUEST.SERVER_READY_REQUEST => CONNECTION_RESPONSE.SERVER_READY_RESPONSE")
            .then.shouldLog("apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => NONE")
            .then.shouldLog("apply rule[3.0] CONNECTION_REQUEST.DISCONNECTING_REQUEST => CONNECTION_RESPONSE.DISCONNECTION_CONFIRMATION_RESPONSE");
  }

  @ParameterizedTest
  @ValueSource(strings = {"(10.20.0.1)", "(10.20.0.9)"})
  void ClientTests(String ip) {
    CommonAssertions.assertClientHandshake(client.nodeClient(ip));
  }

  //Should be the same as previous ones when broadcast is implemented
  @Test
  void SingletonNodeClient() {
    CommonAssertions.assertClientSingletonHandshake(client.nodeClient("10.20.0.5"));
  }

  // New connected not should not request topology
  @Test
  void DiscoveredNode() {
    client.nodeClient("(10.20.0.13)").shouldLog("[ffff] [UNBOUND] : Switched to state DISCONNECTED")
            .then.shouldNotLog("[ffff] [DISCONNECTED] : Switched to state REQUESTING_TOPOLOGY")
            .then.shouldNotLog("[ffff] [REQUESTING_TOPOLOGY] : Sending TOPOLOGY_REQUEST")
            .then.shouldNotLog("[ffff] [REQUESTING_TOPOLOGY] : Received TOPOLOGY_UPDATE (12 bytes)")
            .then.shouldLog("[ffff] [DISCONNECTED] : Switched to state CONNECTING_PREDICTION")
            .then.shouldLog("[ffff] [CONNECTING_PREDICTION] : Sending REGISTRATION_REQUEST")
            .then.shouldLog("[ffff] [CONNECTING_PREDICTION] : Received REGISTRATION_CONFIRMATION (24 bytes)")
            .then.shouldLog("[0001] [CONNECTING_PREDICTION] : Switched to state REGISTERED")
            .then.shouldLog("[0001] [REGISTERED] : Sending SERVER_OPEN_REQUEST")
            .then.shouldLog("[0001] [REGISTERED] : Received SERVER_OPENED (4 bytes)")
            .then.shouldLog("[0001] [REGISTERED] : Switched to state AWAITING_PRED_CONNECTION")
            .then.shouldLog("[0001] [AWAITING_PRED_CONNECTION] : Switched to state PRED_CONNECTED")
            .then.shouldLog("[0001] [PRED_CONNECTED] : Sending SERVER_READY_REQUEST")
            .then.shouldLog("[0001] [PRED_CONNECTED] : Received SERVER_READY (4 bytes)");
  }
}
