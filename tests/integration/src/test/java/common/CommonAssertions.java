package common;

import assertion.AssertionRule;

public class CommonAssertions {
  public static AssertionRule assertClientHandshake(AssertionRule rule) {
    return assertClientHandshake(rule, "0001");
  }
  public static AssertionRule assertClientHandshake(AssertionRule rule, String clientId) {
    return rule.then.shouldLog("[ffff] [UNBOUND] : Switched to state DISCONNECTED")
            .then.shouldLog("[ffff] [DISCONNECTED] : Switched to state REQUESTING_TOPOLOGY")
            .then.shouldLog("[ffff] [REQUESTING_TOPOLOGY] : Sending TOPOLOGY_REQUEST")
            .then.shouldLog("[ffff] [REQUESTING_TOPOLOGY] : Received TOPOLOGY_UPDATE")
            .then.shouldLog("[ffff] [REQUESTING_TOPOLOGY] : Switched to state CONNECTING_PREDICTION")
            .then.shouldLog("[ffff] [CONNECTING_PREDICTION] : Sending REGISTRATION_REQUEST")
            .then.shouldLog("[ffff] [CONNECTING_PREDICTION] : Received REGISTRATION_CONFIRMATION (24 bytes)")
            .then.shouldLog("["+clientId+"] [CONNECTING_PREDICTION] : Switched to state REGISTERED")
            .then.shouldLog("["+clientId+"] [REGISTERED] : Sending SERVER_OPEN_REQUEST")
            .then.shouldLog("["+clientId+"] [REGISTERED] : Received SERVER_OPENED (4 bytes)")
            .then.shouldLog("["+clientId+"] [REGISTERED] : Switched to state AWAITING_PRED_CONNECTION")
            .then.shouldLog("["+clientId+"] [AWAITING_PRED_CONNECTION] : Switched to state PRED_CONNECTED")
            .then.shouldLog("["+clientId+"] [PRED_CONNECTED] : Sending SERVER_READY_REQUEST")
            .then.shouldLog("["+clientId+"] [PRED_CONNECTED] : Received SERVER_READY (4 bytes)");
  }

  public static AssertionRule assertClientSingletonHandshake(AssertionRule rule) {
    return rule.shouldLog("Binding new NodeClient")
            .then.shouldLog("[ffff] [UNBOUND] : Switched to state DISCONNECTED")
            .then.shouldLog("[ffff] [DISCONNECTED] : Switched to state REQUESTING_TOPOLOGY")
            .then.shouldLog("[ffff] [REQUESTING_TOPOLOGY] : Sending TOPOLOGY_REQUEST")
            .then.shouldLog("[ffff] [REQUESTING_TOPOLOGY] : Received TOPOLOGY_UPDATE (24 bytes)")
            .then.shouldLog("[ffff] [REQUESTING_TOPOLOGY] : Switched to state CONNECTING_PREDICTION")
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

  public static AssertionRule assertServerRegularHandshake(AssertionRule rule) {
    return rule.then.shouldLog("apply rule[1.0] TOPOLOGY_REQUEST.NONE => TOPOLOGY_UPDATE")
            .then.shouldLog("apply rule[2.0] CONNECTION_REQUEST.REGISTRATION_REQUEST => CONNECTION_RESPONSE.REGISTRATION_RESPONSE")
            .then.shouldLog("apply rule[4.0] CONNECTION_REQUEST.SERVER_OPEN_REQUEST => CONNECTION_RESPONSE.SERVER_OPENED_RESPONSE")
            .then.shouldLog("apply rule[5.0] CONNECTION_REQUEST.SERVER_READY_REQUEST => CONNECTION_RESPONSE.SERVER_READY_RESPONSE");
  }
}
