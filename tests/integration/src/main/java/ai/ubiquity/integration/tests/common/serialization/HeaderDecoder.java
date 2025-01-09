package ai.ubiquity.integration.tests.common.serialization;

import org.agrona.DirectBuffer;

/**
 * This helped class allows access to the header information without prior
 * knowledge of the message type
 */
public final class HeaderDecoder {
  private DirectBuffer buffer;
  private int offset;

  public void wrap(DirectBuffer buffer, int offset) {
    if (this.buffer != buffer)
      this.buffer = buffer;
    this.offset = offset;
  }

  public byte getVersion() {
    return buffer.getByte(offset);
  }

  public byte getType() {
    return buffer.getByte(offset + 1);
  }
}