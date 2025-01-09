package ai.ubiquity.integration.tests.common.serialization;

// Server Opened
// 0               1               2               3               4
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    V(=0x00)   |  Type(=0x0c)  |           Client ID           |
// +---------------+---------------+-------------------------------+

public class ServerOpenedResponse extends AbstractMessage {
  int TYPE = 0x0c;

  public static final int VERSION = 0x00;
  public static final int SIZE = 4;

  @Override
  public int messageSize() {
    return SIZE;
  }

  public void setHeader() {
    buffer.putByte(offset, (byte) VERSION);
    buffer.putByte(offset + 1, (byte) TYPE);
  }

  public void setClientId(short clientId) {
    buffer.putShort(offset + 2, clientId);
  }
}
