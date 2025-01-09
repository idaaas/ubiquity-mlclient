package ai.ubiquity.integration.tests.common.serialization;

// Registration Response V1
// 0               1               2               3               4
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    V(=0x01)   |  Type(=0x02)  |            Client ID          |
// 4---------------+---------------+------------------------------20
// |                                                               |
// |                        Correlation id                         |
// |                                                               | 
// 20------------------------------22-----------------------------24
// |        Control Port           |     Assigned Client ID        |                        
// +-------------------------------+-------------------------------+

public class RegistrationResponse extends AbstractMessage{
    
    int TYPE = 0x02;

    public static final int VERSION = 0x01;
    public static final short UNSET_CLIENTID = (short)0xffff;
    public static final int SIZE = 24;

    @Override
    public int messageSize() {
        return SIZE;
    }

    public void setHeader() {
        buffer.putByte(offset, (byte) VERSION);
        buffer.putByte(offset + 1, (byte) TYPE);
        buffer.putShort(offset + 2, (short)UNSET_CLIENTID);
    }

    public void setCorrelationId(long correlationIdHigh, long correlationIdLow) {
        buffer.putLong(offset + 4, correlationIdHigh);
        buffer.putLong(offset + 12, correlationIdLow);
    }

    public void setControlPort(short value) {
        buffer.putShort(offset + 20, value);
    }

    public void setClientId(short clientId) {
        buffer.putShort(offset + 22, clientId);
    }
}
