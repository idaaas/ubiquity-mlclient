package ai.ubiquity.integration.tests.common.serialization;

import org.agrona.DirectBuffer;

public class RegistrationRequestDecoder {

  private DirectBuffer buffer;
  private int offset;

  public void wrap(DirectBuffer buffer, int offset) {
    if (this.buffer != buffer)
      this.buffer = buffer;
    this.offset = offset;
  }

  public short getClientId() {
    return buffer.getShort(offset + 2);
  }

  public long getCorrelationIdHigh() {
    return buffer.getLong(offset + 4);
  }

  public long getCorrelationIdLow() {
    return buffer.getLong(offset + 12);
  }

  public short getOutput() {
    return buffer.getShort(offset + 20);
  }
}
