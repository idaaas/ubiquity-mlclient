package ai.ubiquity.integration.tests.common.serialization;

// Registration Error Message
// 0               1               2               3               4
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    V(=0x00)   |  Type(=0x03)  |          Client Id            |
// 4--------------------------------------------------------------20
// |                                                               |
// |                        Correlation id                         |
// |                                                               |
// 20-------------------------------------------------------------24
// |                         Error code                            |
// +---------------------------------------------------------------+

public class RegistrationErrorMessage extends AbstractMessage{

    int TYPE = 0x03;

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

    public void setErrorCode(int value) {
        buffer.putInt(offset + 20, value);
    }
}
