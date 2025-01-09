package ai.ubiquity.integration.tests.common.serialization;

import ai.ubiquity.integration.tests.common.types.FeatureType;
import ai.ubiquity.integration.tests.common.types.ApplicationIdType;

import java.util.List;

public class ModelSchemaUpdateReponse extends AbstractMessage {

  int TYPE = 0x14;

  public static final int VERSION = 0x00;
  public static final short BROADCAST = Short.MAX_VALUE;
  public static final int SIZE = 24;
  public int size = 0; //pas propre

  @Override
  public int messageSize() {
    return size;
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

  public void setApplicationId(ApplicationIdType appId) {
    buffer.putShort(offset + 16, appId.value);
  }

  public void setFeedbackDelays(List<Integer> feedbacks, List<FeatureType> features) {
    buffer.putShort(offset + 18, (short)feedbacks.size());
    for (int i = 0 ; i < feedbacks.size() ; i++) {
      buffer.putInt(offset + 20 + i * 4, feedbacks.get(i));
    }
    int featureOffset = 20 + feedbacks.size()*4;

    buffer.putShort(offset + featureOffset, (short)features.size());
    for (int i = 0 ; i < features.size() ; i++) {
      buffer.putByte(offset + featureOffset + 2 + i, features.get(i).value);
    }

    size = featureOffset + 2 + features.size();
  }
}
