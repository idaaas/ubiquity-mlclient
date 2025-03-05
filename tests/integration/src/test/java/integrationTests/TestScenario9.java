package integrationTests;

import assertion.AssertionRule;
import assertion.IntegrationAssert;
import common.CommonAssertions;
import common.ResourceRetriever;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public class TestScenario9 {
  IntegrationAssert server, client;

  @BeforeEach
  void setup() {
    String serverPath = ResourceRetriever.getResource("it-scenario9.log");
    String clientPath = ResourceRetriever.getResource("client-scenario9.log");

    server = new IntegrationAssert(serverPath);
    client = new IntegrationAssert(clientPath);
  }

  @Test
  void Cluster() {
    // should respond properly to handshake protocol
    AssertionRule rule = CommonAssertions.assertServerRegularHandshake(server.anyNode().shouldLog("Starts polling"));
    rule.then.shouldLog("apply rule[6.0] CONNECTION_REQUEST.HEARTBEAT => MODEL_SCHEMA_UPDATE  1 0")
            .then.shouldLog("Sending model Id 1000")
            .then.shouldLog("Sending model Id 2000")
            .then.shouldLog("Sending model Id 3000")
            .then.shouldLog("Sending model Id 4000")
            .then.shouldLog("prediction query with id 1001 received with type 05 and application 02")
            .then.shouldLog("prediction query with id 2002 received with type 05 and application 03")
            .then.shouldLog("prediction query with id 3003 received with type 05 and application 01")
            .then.shouldLog("prediction query with id 4004 received with type 05 and application 00");
  }

  @Test
  void Client() {
    AssertionRule rule = CommonAssertions.assertClientHandshake(client.anyNodeClient());
    rule.then.shouldLog("[0001] [CLIENT_READY] : Received MODEL_ADD (34 bytes)", 4)
            .and.shouldLog("Inserting new schema(1000) in cache")
            .then.shouldLog("Inserting new schema(2000) in cache")
            .then.shouldLog("Inserting new schema(3000) in cache")
            .then.shouldLog("Inserting new schema(4000) in cache")
            .then.shouldLog("query 1 sent")
            .then.shouldLog("prediction line-item received with price 0.125000 and probability 0.200000")
            .then.shouldLog("query 2 sent")
            .then.shouldLog("prediction action-rate received with price 0.398000")
            .then.shouldLog("query 3 sent")
            .then.shouldLog("prediction probability-estimator received with probability 0.300000")
            .then.shouldLog("query 4 sent")
            .then.shouldLog("prediction bid-shading received with price 0.125000 and probability 0.850000");
  }
}
