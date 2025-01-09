package ai.ubiquity.integration.tests.common.serialization;

public class ModelRemoveResponse extends AbstractMessage {
  int TYPE = 0x15;

  public static final int VERSION = 0x00;
  public static final short BROADCAST = Short.MAX_VALUE;
  public static final int SIZE = 16;

  @Override
  public int messageSize() {
    return SIZE;
  }

  public void setHeader() {
    buffer.putByte(offset, (byte) VERSION);
    buffer.putByte(offset + 1, (byte) TYPE);
    buffer.putShort(offset + 2, BROADCAST);
  }

  public void setNodeIp(String nodeIp) {
    String[] ips = nodeIp.split("\\.");
    buffer.putByte(offset + 4, (byte) Integer.parseInt(ips[0]));
    buffer.putByte(offset + 5, (byte) Integer.parseInt(ips[1]));
    buffer.putByte(offset + 6, (byte) Integer.parseInt(ips[2]));
    buffer.putByte(offset + 7, (byte) Integer.parseInt(ips[3]));
  }

  public void setModelId(long modelId) {
    buffer.putLong(offset + 8, modelId);
  }
}
