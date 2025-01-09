package integrationTests;

import assertion.IntegrationAssert;
import common.ResourceRetriever;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public class TestScenario3 {
  IntegrationAssert server, client;

  @BeforeEach
  void setup() {
    String serverPath = ResourceRetriever.getResource("it-scenario3.log");
    String clientPath = ResourceRetriever.getResource("client-scenario3.log");

    server = new IntegrationAssert(serverPath);
    client = new IntegrationAssert(clientPath);
  }

  @Test
  void ServerNode1() {
    server.node("node_1").shouldLog("Starts polling")
            // the server sends the topology
            .then.shouldLog("apply rule[1.0] TOPOLOGY_REQUEST.NONE => TOPOLOGY_UPDATE")
            // the server sends a correlation conflict
            .then.shouldLog("apply rule[2.0] CONNECTION_REQUEST.REGISTRATION_REQUEST => REGISTRATION_ERROR_RESPONSE.CORRELATION_CONFLICT")
            // the server send a connection response registration with the new correlationId
            .then.shouldLog("apply rule[2.1] CONNECTION_REQUEST.REGISTRATION_REQUEST => CONNECTION_RESPONSE.REGISTRATION_RESPONSE")
            // the sever does not respond to the first two server open query
            .then.shouldLog("apply rule[4.0] CONNECTION_REQUEST.SERVER_OPEN_REQUEST => NONE", 2)
            // the server open the feedback streams and follow the rest of protocol
            .then.shouldLog("apply rule[4.1] CONNECTION_REQUEST.SERVER_OPEN_REQUEST => CONNECTION_RESPONSE.SERVER_OPENED_RESPONSE")
            .then.shouldLog("apply rule[5.0] CONNECTION_REQUEST.SERVER_READY_REQUEST => CONNECTION_RESPONSE.SERVER_READY_RESPONSE")
            .then.shouldLogAtLeast("apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => NONE", 1)
            // the server respond to disconnection request
            .then.shouldLog("apply rule[3.0] CONNECTION_REQUEST.DISCONNECTING_REQUEST => CONNECTION_RESPONSE.DISCONNECTION_CONFIRMATION_RESPONSE");
  }

  @Test
  void ServerNode2() {
    server.node("node_2").shouldLog("Starts polling")
            // the server sends the topology
            .then.shouldLog("apply rule[1.0] TOPOLOGY_REQUEST.NONE => TOPOLOGY_UPDATE")
            // the server sends a correlation conflict
            .then.shouldLog("apply rule[2.0] CONNECTION_REQUEST.REGISTRATION_REQUEST => REGISTRATION_ERROR_RESPONSE.CORRELATION_CONFLICT")
            // the server send a connection response registration with the new correlationId
            .then.shouldLog("apply rule[2.1] CONNECTION_REQUEST.REGISTRATION_REQUEST => CONNECTION_RESPONSE.REGISTRATION_RESPONSE")
            // the sever does not respond to the first two server open query
            .then.shouldLog("apply rule[4.0] CONNECTION_REQUEST.SERVER_OPEN_REQUEST => NONE", 2)
            // the server open the feedback streams and follow the rest of protocol
            .then.shouldLog("apply rule[4.1] CONNECTION_REQUEST.SERVER_OPEN_REQUEST => CONNECTION_RESPONSE.SERVER_OPENED_RESPONSE")
            .then.shouldLog("apply rule[5.0] CONNECTION_REQUEST.SERVER_READY_REQUEST => CONNECTION_RESPONSE.SERVER_READY_RESPONSE")
            .then.shouldLogAtLeast("apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => NONE", 1)
            // the server does not respond to the disconnection request
            .then.shouldLog("apply rule[3.1] CONNECTION_REQUEST.DISCONNECTING_REQUEST => NONE node_2",5);
  }

  @Test
  void NodeClient1() {
    // start regular protocol
    client.nodeClient("(10.20.0.1)").shouldLog("[ffff] [UNBOUND] : Switched to state DISCONNECTED")
            .then.shouldLog("[ffff] [DISCONNECTED] : Switched to state REQUESTING_TOPOLOGY")
            .then.shouldLog("[ffff] [REQUESTING_TOPOLOGY] : Sending TOPOLOGY_REQUEST")
            .then.shouldLog("[ffff] [REQUESTING_TOPOLOGY] : Received TOPOLOGY_UPDATE")
            .then.shouldLog("[ffff] [REQUESTING_TOPOLOGY] : Switched to state CONNECTING_PREDICTION")
            .then.shouldLog("[ffff] [CONNECTING_PREDICTION] : Sending REGISTRATION_REQUEST")
            // first registration fails with correlation conflict
            .then.shouldLog("[ffff] [CONNECTING_PREDICTION] : Received REGISTRATION_ERROR (24 bytes)")
            // resend registration request with new correlationId
            .then.shouldLog("[ffff] [CONNECTING_PREDICTION] : Sending REGISTRATION_REQUEST")
            // registration succeed
            .then.shouldLog("[ffff] [CONNECTING_PREDICTION] : Received REGISTRATION_CONFIRMATION (24 bytes)")
            .then.shouldLog("[0001] [CONNECTING_PREDICTION] : Switched to state REGISTERED")
            // send server open request 3 times, since the server does not respond twice
            .then.shouldLog("[0001] [REGISTERED] : Sending SERVER_OPEN_REQUEST", 3)
            .then.shouldLog("[0001] [REGISTERED] : Received SERVER_OPENED (4 bytes)")
            .then.shouldLog("[0001] [REGISTERED] : Switched to state AWAITING_PRED_CONNECTION")
            .then.shouldLog("[0001] [AWAITING_PRED_CONNECTION] : Switched to state PRED_CONNECTED")
            .then.shouldLog("[0001] [PRED_CONNECTED] : Sending SERVER_READY_REQUEST")
            .then.shouldLog("[0001] [PRED_CONNECTED] : Received SERVER_READY (4 bytes)")
            .then.shouldLog("[0001] [PRED_CONNECTED] : Switched to state CLIENT_READY")
            .then.shouldLog("[0001] [CLIENT_READY] : Switched to state DISCONNECTING")
            .then.shouldLog("[0001] [DISCONNECTING] : Sending DISCONNECTION_REQUEST")
            .then.shouldLog("[0001] [DISCONNECTING] : Received DISCONNECTION_CONFIRMATION")
            .then.shouldLog("[ffff] [DISCONNECTING] : Switched to state UNBOUND");
  }

  @Test
  void NodeClient2() {
    // start regular protocol
    client.nodeClient("(10.20.0.5)").shouldLog("[ffff] [UNBOUND] : Switched to state DISCONNECTED")
            .then.shouldLog("[ffff] [DISCONNECTED] : Switched to state REQUESTING_TOPOLOGY")
            .then.shouldLog("[ffff] [REQUESTING_TOPOLOGY] : Sending TOPOLOGY_REQUEST")
            .then.shouldLog("[ffff] [REQUESTING_TOPOLOGY] : Received TOPOLOGY_UPDATE")
            .then.shouldLog("[ffff] [REQUESTING_TOPOLOGY] : Switched to state CONNECTING_PREDICTION")
            .then.shouldLog("[ffff] [CONNECTING_PREDICTION] : Sending REGISTRATION_REQUEST")
            // first registration fails with correlation conflict
            .then.shouldLog("[ffff] [CONNECTING_PREDICTION] : Received REGISTRATION_ERROR (24 bytes)")
            // resend registration request with new correlationId
            .then.shouldLog("[ffff] [CONNECTING_PREDICTION] : Sending REGISTRATION_REQUEST")
            // registration succeed
            .then.shouldLog("[ffff] [CONNECTING_PREDICTION] : Received REGISTRATION_CONFIRMATION (24 bytes)")
            .then.shouldLog("[0001] [CONNECTING_PREDICTION] : Switched to state REGISTERED")
            // send server open request 3 times, since the server does not respond twice
            .then.shouldLog("[0001] [REGISTERED] : Sending SERVER_OPEN_REQUEST", 3)
            .then.shouldLog("[0001] [REGISTERED] : Received SERVER_OPENED (4 bytes)")
            .then.shouldLog("[0001] [REGISTERED] : Switched to state AWAITING_PRED_CONNECTION")
            .then.shouldLog("[0001] [AWAITING_PRED_CONNECTION] : Switched to state PRED_CONNECTED")
            .then.shouldLog("[0001] [PRED_CONNECTED] : Sending SERVER_READY_REQUEST")
            .then.shouldLog("[0001] [PRED_CONNECTED] : Received SERVER_READY (4 bytes)")
            .then.shouldLog("[0001] [PRED_CONNECTED] : Switched to state CLIENT_READY")
            .then.shouldLog("[0001] [CLIENT_READY] : Switched to state DISCONNECTING")
            .then.shouldLog("[0001] [DISCONNECTING] : Sending DISCONNECTION_REQUEST", 5)
            // the server does not respond
            .and.shouldNotLog("[0001] [DISCONNECTING] : Received DISCONNECTION_CONFIRMATION")
            .then.shouldLog("[ffff] [DISCONNECTING] : Switched to state UNBOUND");
  }
}
