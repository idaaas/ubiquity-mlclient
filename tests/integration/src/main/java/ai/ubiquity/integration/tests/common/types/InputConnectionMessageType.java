package ai.ubiquity.integration.tests.common.types;

import ai.ubiquity.integration.tests.common.MockProtocol;

public enum InputConnectionMessageType {
    REGISTRATION_REQUEST("REGISTRATION_REQUEST", (short)0x01),
    DISCONNECTING_REQUEST("DISCONNECTING_REQUEST", (short)0x05),
    HEARTBEAT("HEARTBEAT", (short)0x06),
    SERVER_OPEN_REQUEST("SERVER_OPEN_REQUEST", (short)0x07),
    SERVER_READY_REQUEST("SERVER_READY_REQUEST", (short)0x08),
    ERROR("ERROR", (short)0xff),
    NONE("NONE", (short)0xfe);

    public final String name;
    public final Short value;
    InputConnectionMessageType(String name, Short value) {
        this.value = value;
        this.name = name;
    }

    public static InputConnectionMessageType fromShort(Short value) {
        for (InputConnectionMessageType type : InputConnectionMessageType.values()) {
            if (type.value.equals(value)) {
                return type;
            }
        }
        return InputConnectionMessageType.NONE;
    }

    public static InputConnectionMessageType fromName(String name) {
        for (InputConnectionMessageType type : InputConnectionMessageType.values()) {
            if (type.name.equals(name)) {
                return type;
            }
        }
        return InputConnectionMessageType.NONE;
    }
}
