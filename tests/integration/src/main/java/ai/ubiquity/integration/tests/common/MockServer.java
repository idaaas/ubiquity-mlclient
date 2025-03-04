package ai.ubiquity.integration.tests.common;

import ai.ubiquity.integration.tests.common.serialization.*;

import io.aeron.Aeron;
import io.aeron.logbuffer.Header;

import org.agrona.DirectBuffer;
import org.agrona.MutableDirectBuffer;
import org.agrona.concurrent.*;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.time.Duration;

import javax.naming.NameNotFoundException;

import com.typesafe.config.Config;

public class MockServer extends Thread {
  protected final IdleStrategy POLLING_IDLE_STRATEGY = new BusySpinIdleStrategy();
  protected final long POLLING_TIMEOUT = Duration.ofSeconds(20).toMillis();

  protected final String serverIP;
  protected final String serverId;

  protected EpochClock clock = SystemEpochClock.INSTANCE;
  protected MutableDirectBuffer protocolWriteBuffer = new UnsafeBuffer(
      ByteBuffer.allocateDirect(64).order(ByteOrder.LITTLE_ENDIAN));
  protected MutableDirectBuffer responseWriteBuffer = new UnsafeBuffer(
          ByteBuffer.allocateDirect(64).order(ByteOrder.LITTLE_ENDIAN));

  private final MockProtocol protocol;
  private final AeronConnectionManager aeronMngr;

  public MockServer(Config nodeConf, Aeron aeron) throws NameNotFoundException {

    String id = nodeConf.getString("id");
    this.serverId = id;
    String ip = MockConfig.getServerConfFromId(id).ip;

    System.out.printf("[Server %s] Create new server %s%n", this.serverId, ip);

    this.serverIP = ip;

    aeronMngr = new AeronConnectionManager(ip, aeron);
    protocol = new MockProtocol(id, aeronMngr);

    aeronMngr.connectProtocolStreams();
  }

  public void run() {
    long deadline = clock.time() + POLLING_TIMEOUT;
    long startTime = clock.time();
    POLLING_IDLE_STRATEGY.reset();
    System.out.printf("[Server %s] Starts polling%n", this.serverId);
    while (clock.time() < deadline) {
      this.aeronMngr.getHandshakeSub().poll(this::handleMessage, 1);
      if (this.aeronMngr.isPredOpened()) this.aeronMngr.getPredSub().poll(this::handlePredictionQuery, 1);
      if (this.aeronMngr.isPredOpened()) this.aeronMngr.getFeedbackSub().poll(this::handleFeedbackQuery, 1);
      try {
        this.handleTimeouts(clock.time() - startTime);
      } catch (NameNotFoundException e) {
        System.out.println("error in handling timeout");
      }
      try {
        this.handleDequeue();
      } catch (Exception e) {
        System.out.println("error in message dequeue");
      }
      POLLING_IDLE_STRATEGY.idle();
    }
    System.out.println("end polling");

    this.interrupt();
  }

  private void handleFeedbackQuery(DirectBuffer buffer, int offset, int length, Header header) {
    QueryHeaderMessage h = new QueryHeaderMessage();
    h.wrap(buffer, offset);
    System.out.printf("[Server %s] feedbacks query with id %d received with type %02X and application %02X%n", serverId,h.getQueryId(), h.getType(), h.getApplicationId());
  }

  public void close() {
    System.out.printf("[Server %s] Closing signal%n", serverId);
    this.aeronMngr.closeProtocolStreams();
  }

  private void handleMessage(DirectBuffer buffer, int offset, int length, Header header) {
    AbstractMessage message = this.protocol.handleMessage(protocolWriteBuffer, buffer, offset);
    if (message == null) return;

    boolean result = this.aeronMngr.sendMessageMulticast(protocolWriteBuffer, message.messageSize());
    if (!result) System.out.printf("[Server %s] couldn't send message response to client while in %n", serverId);

  }

  private void handlePredictionQuery(DirectBuffer buffer, int offset, int length, Header header) {
    AbstractMessage message = this.protocol.handlePredictionQuery(responseWriteBuffer, buffer, offset, length, header);
    if (message == null) return;

    boolean result = this.aeronMngr.sendQueryResponse(responseWriteBuffer, message.messageSize());
    if (!result) System.out.printf("[Server %s] couldn't send query response to client %n", serverId);

  }

  private void handleTimeouts(long elapsedTime) throws NameNotFoundException{
    AbstractMessage message = this.protocol.handleTimeouts(protocolWriteBuffer, elapsedTime);
    if (message == null) return;

    boolean result = this.aeronMngr.sendMessageMulticast(protocolWriteBuffer, message.messageSize());
    if (!result) System.out.printf("[Server %s] couldn't send message response to client while in %n", serverId);
  }

  private void handleDequeue() {
    AbstractMessage message = this.protocol.handleBackPressureQueue(protocolWriteBuffer);
    if (message == null) return;

    boolean result = this.aeronMngr.sendMessageMulticast(protocolWriteBuffer, message.messageSize());
    if (!result) System.out.printf("[Server %s] couldn't send message response to client while in %n", serverId);
  }
}