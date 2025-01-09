package ai.ubiquity.integration.tests.common.serialization;

import org.agrona.DirectBuffer;

import java.nio.ByteOrder;

public class QueryHeaderMessage {

  int TYPE = 0x05;

  public static final int VERSION = 0x00;
  public static final int SIZE = 28;

  public int messageSize() {
    return SIZE;
  }

  private DirectBuffer buffer;
  private int offset;

  public void wrap(DirectBuffer buffer, int offset) {
    if (this.buffer != buffer)
      this.buffer = buffer;
    this.offset = offset;
  }

  public byte getVersion() {
    return this.buffer.getByte(offset);
  }

  public byte getType() {
    return this.buffer.getByte(offset + 1);
  }

  public short getClientId() {
    return buffer.getShort(offset + 2, ByteOrder.LITTLE_ENDIAN);
  }

  public short getApplicationId() {
    return buffer.getShort(offset + 4, ByteOrder.LITTLE_ENDIAN);
  }

  public short getSchemaVersion() {
    return buffer.getShort(offset + 6, ByteOrder.LITTLE_ENDIAN);
  }

  public long getModelId() {
    return buffer.getLong(offset + 8, ByteOrder.LITTLE_ENDIAN);
  }

  public long getQueryId() {
    return buffer.getLong(offset + 16, ByteOrder.LITTLE_ENDIAN);
  }

  public int getFeedbackDelay() {
    return buffer.getInt(offset + 24, ByteOrder.LITTLE_ENDIAN);
  }
}
