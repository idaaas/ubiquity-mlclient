package ai.ubiquity.integration.tests.common.serialization;

public class ActionRateResponseMessage extends AbstractMessage {
  int TYPE = 0x06;

  public static final int VERSION = 0x00;
  public static final int SIZE = 36;


  @Override
  public int messageSize() {
    return SIZE;
  }

  public void setHeader(short clientId, short applicationId, long modelId, long queryId) {
    buffer.putByte(offset, (byte) VERSION);
    buffer.putByte(offset + 1, (byte) TYPE);
    buffer.putShort(offset + 2, clientId);
    buffer.putShort(offset + 4, applicationId);
    buffer.putLong(offset + 8, modelId);
    buffer.putLong(offset + 16, queryId);
  }

  public void setCreativeId(long creativeId) {
    buffer.putLong(offset + 24, creativeId);
  }

  public void setPrice(float price) {
    buffer.putFloat(offset + 32, price);
  }
}