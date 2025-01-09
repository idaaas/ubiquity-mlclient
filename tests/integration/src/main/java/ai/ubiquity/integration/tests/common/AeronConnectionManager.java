package ai.ubiquity.integration.tests.common;

import java.time.Duration;

import io.aeron.Aeron;
import io.aeron.ChannelUriStringBuilder;
import io.aeron.logbuffer.BufferClaim;
import io.aeron.Publication;
import io.aeron.Subscription;

import org.agrona.DirectBuffer;

import static java.lang.Thread.sleep;

public class AeronConnectionManager {
  private final static int CONNECTION_PORT = 2000;
  private final static int CONTROL_PORT = 2001;

  private final static int CONNECTION_STREAM_ID = 1;
  private final static int CONTROL_STREAM_ID = 1;

  private Subscription connectionSub;
  private Publication controlPub;

  private Subscription feedbackSub;
  private Subscription predSub;
  private Publication predPub;

  private final static int PREDICTION_STREAM_MDC = 1;
  private final static int PREDICTION_QUERY_STREAM_ID = 2;
  private final static int FEEDBACK_STREAM_ID = 3;

  private boolean isServerOpened = false;

  private String serverIP;

  private Aeron aeron;

  AeronConnectionManager(String serverIP, Aeron aeron) {
    this.serverIP = serverIP;
    this.aeron = aeron;
  }

  void connectProtocolStreams() {
    this.startProtocolStreams();
  }

  void closeProtocolStreams() {
    this.connectionSub.close();
    this.connectionSub = null;
    this.controlPub.close();
    this.controlPub = null;
  }

  boolean areProtocolStreamsOpened() {
    return this.connectionSub.isConnected() && this.controlPub.isConnected();
  }

  void startProtocolStreams() {
    String connectionChannel = new ChannelUriStringBuilder()
        .media("udp")
        .endpoint(this.serverIP + ":" + CONNECTION_PORT)
        .build();

    String controlChannel = new ChannelUriStringBuilder()
        .media("udp")
        .controlEndpoint(this.serverIP + ":" + CONTROL_PORT)
        .controlMode("dynamic")
        .build();

    this.connectionSub = aeron.addSubscription(connectionChannel, CONNECTION_STREAM_ID);
    this.controlPub = aeron.addPublication(controlChannel, CONTROL_STREAM_ID);

    System.out.printf("connection id %d%n", this.connectionSub.registrationId());
  }

  Subscription getHandshakeSub() {
    return this.connectionSub;
  }

  void openPred(int controlPort) {
    String predQueryChannelSub = new ChannelUriStringBuilder()
        .media("udp")
        .endpoint(this.serverIP + ":" + CONNECTION_PORT)
        .build();

    String feedbackChannelSub = new ChannelUriStringBuilder()
        .media("udp")
        .endpoint(this.serverIP + ":" + CONNECTION_PORT)
        .build();

    String predChannelControlPub = new ChannelUriStringBuilder()
        .media("udp")
        .controlEndpoint(this.serverIP + ":" + controlPort)
        .controlMode("dynamic")
        .build();

    this.predSub = aeron.addSubscription(predQueryChannelSub, PREDICTION_QUERY_STREAM_ID);
    this.feedbackSub = aeron.addSubscription(feedbackChannelSub, FEEDBACK_STREAM_ID);
    this.predPub = aeron.addPublication(predChannelControlPub, PREDICTION_STREAM_MDC);

    isServerOpened = true;
  }

  Subscription getPredSub() {
    return this.predSub;
  }

  void closePred() {
    this.predSub.close();
    this.feedbackSub.close();
    this.predPub.close();
  }

  void restartPred() {
    String subChannel = this.predSub.channel();
    String feedbackChannel = this.feedbackSub.channel();
    String predChannel = this.predPub.channel();

    this.predSub.close();
    this.feedbackSub.close();
    this.predPub.close();

    try {
      sleep(1000);
    } catch (InterruptedException e) {
      throw new RuntimeException(e);
    }

    this.predSub = aeron.addSubscription(subChannel, PREDICTION_QUERY_STREAM_ID);
    this.feedbackSub = aeron.addSubscription(feedbackChannel, FEEDBACK_STREAM_ID);
    this.predPub = aeron.addPublication(predChannel, PREDICTION_STREAM_MDC);

  }

  boolean isPredOpened() {
    if (isServerOpened) {
      return this.predSub.isConnected() && this.feedbackSub.isConnected() && this.predPub.isConnected();
    }

    return false;
  }



  protected boolean sendQueryResponse(DirectBuffer buffer, int length) {
    if (!this.predPub.isConnected()) {
      System.out.println("[WARN] query response polling is not connected");
      return false;
    }
    final BufferClaim bufferClaim = new BufferClaim();
    long deadline = System.nanoTime() + Duration.ofSeconds(5).getNano();
    long result;
    do {
      result = this.predPub.tryClaim(length, bufferClaim);
    } while (result <= 0L && System.nanoTime() <= deadline);
    if (result > 0) {
      bufferClaim.putBytes(buffer, 0, length).commit();
      return true;
    }
    return false;
  }

  protected boolean sendMessageMulticast(DirectBuffer buffer, int length) {
    final BufferClaim bufferClaim = new BufferClaim();
    long deadline = System.nanoTime() + Duration.ofSeconds(5).getNano();
    long result;
    do {
      result = this.controlPub.tryClaim(length, bufferClaim);
    } while (result <= 0L && System.nanoTime() <= deadline);
    if (result > 0) {
      bufferClaim.putBytes(buffer, 0, length).commit();
      return true;
    }
    return false;
  }

  public Publication getPredPub() {
    return this.predPub;
  }
}
