package ai.ubiquity.integration.tests.common.types;

public enum ApplicationIdType {
  LINE_ITEM("line-item", (short)0x02),
  BID_SHADING("bid-shading", (short)0x00),
  PROBABILITY_ESTIMATOR("probability-estimator", (short)0x01),
  UNDEFINED("undefined", (short)0x03);

  public short value;
  public String name;
  ApplicationIdType(String name, short value) {
    this.name = name;
    this.value = value;
  }

  public static ApplicationIdType fromName(String name) {
    for (ApplicationIdType type : ApplicationIdType.values()) {
      if (type.name.equals(name)) {
        return type;
      }
    }
    return ApplicationIdType.UNDEFINED;
  }
}