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
  protected final static int PREDICTION_PORT = 2004;
  protected final static int PREDICTION_STREAM_ID = 1;
  protected final IdleStrategy POLLING_IDLE_STRATEGY = new BusySpinIdleStrategy();
  protected final long POLLING_TIMEOUT = Duration.ofSeconds(20).toMillis();

  protected final String serverIP;
  protected final String serverId;

  protected EpochClock clock = SystemEpochClock.INSTANCE;
  protected final HeaderDecoder headerReader = new HeaderDecoder();
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
    //System.out.printf("send message with size %d%n", message.messageSize());
    if (message == null) return;

    boolean result = this.aeronMngr.sendMessageMulticast(protocolWriteBuffer, message.messageSize());
    if (!result) {
      System.out.printf("[Server %s] couldn't send message response to client while in %n", serverId);
      // currentState = State.ERROR;
    }
  }

  private void handlePredictionQuery(DirectBuffer buffer, int offset, int length, Header header) {
    QueryHeaderMessage h = new QueryHeaderMessage();
    h.wrap(buffer, offset);
    System.out.printf("[Server %s] prediction query with id %d received with type %02X and application %02X%n", serverId,h.getQueryId(), h.getType(), h.getApplicationId());
    if (h.getType() != 0x05) {
      System.out.printf("[Server %s] wrong type received : %02X%n", serverId, h.getType());
    }

    responseWriteBuffer.putByte(0, (byte)0x00);
    responseWriteBuffer.putByte(1, (byte)0x06);
    responseWriteBuffer.putShort(2, h.getClientId());
    responseWriteBuffer.putShort(4, h.getApplicationId());
    responseWriteBuffer.putLong(8, h.getModelId());
    responseWriteBuffer.putLong(16, h.getQueryId());
    responseWriteBuffer.putLong(24, buffer.getLong(offset+36));
    responseWriteBuffer.putInt(32, 125); //price
    responseWriteBuffer.putInt(36, 100); //conversion proba

    boolean result = this.aeronMngr.sendQueryResponse(responseWriteBuffer, 40);
    if (!result) {
      System.out.printf("[Server %s] couldn't send query response to client %n", serverId);
      // currentState = State.ERROR;
    }
  }

  private void handleTimeouts(long elapsedTime) throws NameNotFoundException{
    AbstractMessage message = this.protocol.handleTimeouts(protocolWriteBuffer, elapsedTime);
    if (message == null) return;

    boolean result = this.aeronMngr.sendMessageMulticast(protocolWriteBuffer, message.messageSize());
    if (!result) {
      System.out.printf("[Server %s] couldn't send message response to client while in %n", serverId);
    }
  }

  private void handleDequeue() {
    AbstractMessage message = this.protocol.handleBackPressureQueue(protocolWriteBuffer);
    if (message == null) return;

    boolean result = this.aeronMngr.sendMessageMulticast(protocolWriteBuffer, message.messageSize());
    if (!result) {
      System.out.printf("[Server %s] couldn't send message response to client while in %n", serverId);
    }
  }

};