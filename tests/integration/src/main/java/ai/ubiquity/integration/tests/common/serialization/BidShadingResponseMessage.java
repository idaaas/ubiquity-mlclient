package ai.ubiquity.integration.tests.common.serialization;

public class BidShadingResponseMessage extends AbstractMessage {
  int TYPE = 0x06;

  public static final int VERSION = 0x00;
  public static final int SIZE = 32;


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

  public void setPrice(float price) {
    buffer.putFloat(offset + 24, price);
  }

  public void setWinningProbability(float probability) {
    buffer.putFloat(offset + 28, probability);
  }
}
