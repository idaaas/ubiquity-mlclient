package ai.ubiquity.integration.tests.common.serialization;

// Topology Update Message
// 0               1               2               3               4
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    V(=0x00)   |  Type(=0x0d)  |            Client ID          |
// 4---------------+---------------6-------------------------------8
// |            Node Status        |         Number of Nodes       |                       
// 8-------------------------------+------------------------------12 
// |                            Node IP                            |                       
// 12--------------------------------------------------------------+ 
// ...                                                             |
// +---------------------------------------------------------------+

public class TopologyUpdateResponse extends AbstractMessage {
  public static int TYPE = 0x0D;

  public static final int VERSION = 0;
  public static final int SIZE = 8;
  public static final short CLIENT_ID_BROADCAST = Short.MAX_VALUE;
  public static final int MAX_NUM_NODES = 100;

  public short numNodes = 0;

  @Override
  public int messageSize() {
    return SIZE + numNodes * 4;
  }

  public static int maxMessageSize() {
    return SIZE + MAX_NUM_NODES * 4;
  }

  public void setHeader() {
    buffer.putByte(offset, (byte) VERSION);
    buffer.putByte(offset + 1, (byte) TYPE);
  }

  public void setBroadcast() {
    buffer.putShort(offset + 2, CLIENT_ID_BROADCAST);
  }

  public void setNodesStatus(NodeStatus status) {
    buffer.putShort(offset + 4, (short) status.ordinal());
  }

  public void setNodesStatus(short status) {
    buffer.putShort(offset + 4, status);
  }

  public void setNumNodes(short value) {
    numNodes = value;
    buffer.putShort(offset + 6, value);
  }

  public void setNodeIp(int i, String ip) {
    String[] ips = ip.split("\\.");
    buffer.putByte(offset + SIZE + i * 4, (byte) Integer.parseInt(ips[0]));
    buffer.putByte(offset + SIZE + 1 + i * 4, (byte) Integer.parseInt(ips[1]));
    buffer.putByte(offset + SIZE + 2 + i * 4, (byte) Integer.parseInt(ips[2]));
    buffer.putByte(offset + SIZE + 3 + i * 4, (byte) Integer.parseInt(ips[3]));
  }

  public enum NodeStatus {
    REMOVED((short)0x00), 
    ADDED((short)0x01),
    FULL_STATE((short)0x02);

    short hexvalue;

    NodeStatus(short hexvalue) {
      this.hexvalue = hexvalue;
    }
    
  }
}
