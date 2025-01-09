package ai.ubiquity.integration.tests.common.serialization;

import org.agrona.MutableDirectBuffer;

/**
 * This class helps implement outgoing message serializers. It
 * partiallysimplements the OutgoingMessage interface
 */
public class Encoder {
  protected MutableDirectBuffer buffer;
  protected int offset;

  public void wrap(MutableDirectBuffer buffer, int offset) {
    if (this.buffer != buffer)
      this.buffer = buffer;
    this.offset = offset;
  }
}