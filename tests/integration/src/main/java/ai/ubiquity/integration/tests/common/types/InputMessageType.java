package ai.ubiquity.integration.tests.common.types;

public enum InputMessageType {
    TOPOLOGY_REQUEST("TOPOLOGY_REQUEST", (short)0x0f),
    CONNECTION_REQUEST("CONNECTION_REQUEST", (short)0x01),
    TIME_MS("TIME_MS", (short)0x1f),
    NONE("NONE", (short)0xff);

    public final String name;
    public final Short value;
    InputMessageType(String name, Short value) {
        this.value = value;
        this.name = name;
    }

    public static InputMessageType fromShort(Short value) {
        for (InputMessageType type : InputMessageType.values()) {
            if (type.value.equals(value)) {
                return type;
            }
        }
        return InputMessageType.NONE;
    }

    public static InputMessageType fromName(String name) {
        for (InputMessageType type : InputMessageType.values()) {
            if (type.name.equals(name)) {
                return type;
            }
        }
        return InputMessageType.NONE;
    }
}
