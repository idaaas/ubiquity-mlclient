package ai.ubiquity.integration.tests.common.types;

public enum FeatureType {
  NUMERIC("numeric", (byte) 0x0),
  UNDEFINED("undefined", (byte) 0x1),
  NOMINAL("nominal", (byte) 0x2),
  BOOLEAN("boolean", (byte) 0x3),
  CATEGORICAL("categorical", (byte) 0x4);

  public byte value;
  public String name;
  FeatureType(String name, byte value) {
    this.name = name;
    this.value = value;
  }

  public static FeatureType fromName(String name) {
    for (FeatureType type : FeatureType.values()) {
      if (type.name.equals(name)) {
        return type;
      }
    }
    return FeatureType.UNDEFINED;
  }
}