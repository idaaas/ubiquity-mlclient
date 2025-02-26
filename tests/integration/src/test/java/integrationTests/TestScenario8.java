package integrationTests;

import assertion.AssertionRule;
import assertion.IntegrationAssert;
import common.CommonAssertions;
import common.ResourceRetriever;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public class TestScenario8 {
  IntegrationAssert server, client;

  @BeforeEach
  void setup() {
    String serverPath = ResourceRetriever.getResource("it-scenario8.log");
    String clientPath = ResourceRetriever.getResource("client-scenario8.log");

    server = new IntegrationAssert(serverPath);
    client = new IntegrationAssert(clientPath);
  }

  @Test
  void Cluster() {
    // should respond properly to handshake protocol
    AssertionRule rule = CommonAssertions.assertServerRegularHandshake(server.node("node_1").shouldLog("Starts polling"));
    rule.then.shouldLog("apply rule[8.0] TIME_MS.NONE => MODEL_SCHEMA_UPDATE")
            .then.shouldLog("Sending model Id 1000")
            // Should receive 200 prediction queries
            .then.shouldLog("prediction query with id 1001 received with type 05 and application 02", 200)
            // Should receive cancel query
            .then.shouldLog("feedbacks query with id 1001 received with type 08 and application 02");
  }

  @Test
  void Client() {
    AssertionRule rule = CommonAssertions.assertClientHandshake(client.anyNodeClient());
    rule.then.shouldLog("[0001] [CLIENT_READY] : Received MODEL_ADD (34 bytes)")
            .then.shouldLog("Inserting new schema(1000) in cache")
            // Should send 200 query
            .then.shouldLog("[General] query sent", 200)
            // Should poll 200 prediction
            .and.shouldLog("[General] prediction received", 200)
            // Should send one cancellation query
            .and.shouldLog("[General] cancel sent");
  }
}
