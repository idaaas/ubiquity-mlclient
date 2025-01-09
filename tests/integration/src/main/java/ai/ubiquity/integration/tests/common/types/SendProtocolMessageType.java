package ai.ubiquity.integration.tests.common.types;

public enum SendProtocolMessageType {
    TOPOLOGY_UPDATE("TOPOLOGY_UPDATE", 0x0f00),
    TOPOLOGY_UPDATE__REMOVE("TOPOLOGY_UPDATE.REMOVE", 0x1f00),
    CONNECTION_RESPONSE__REGISTRATION_RESPONSE("CONNECTION_RESPONSE.REGISTRATION_RESPONSE", 0x01),
    CONNECTION_RESPONSE__DISCONNECTION_CONFIRMATION_RESPONSE("CONNECTION_RESPONSE.DISCONNECTION_CONFIRMATION_RESPONSE", 0x02),
    CONNECTION_RESPONSE__SERVER_OPENED_RESPONSE("CONNECTION_RESPONSE.SERVER_OPENED_RESPONSE", 0x03),
    CONNECTION_RESPONSE__SERVER_READY_RESPONSE("CONNECTION_RESPONSE.SERVER_READY_RESPONSE", 0x04),
    REGISTRATION_ERROR_RESPONSE__CORRELATION_CONFLICT("REGISTRATION_ERROR_RESPONSE.CORRELATION_CONFLICT", 0x05),
    REGISTRATION_ERROR_RESPONSE__NO_NEW_CONNECTION("REGISTRATION_ERROR_RESPONSE.NO_NEW_CONNECTION", 0x06),
    MODEL_SCHEMA_UPDATE("MODEL_SCHEMA_UPDATE", 0x14),
    NONE("NONE", 0xffff),
    SHUTDOWN__UNGRACEFUL("SHUTDOWN.UNGRACEFUL", 0xfffe),
    START("START", 0xfffd),
    DISCONNECT("DISCONNECT", 0xfffc),
    ADD_MODEL("ADD_MODEL", 0xfffb),
    REMOVE_MODEL("REMOVE_MODEL", 0xfffa);

    public final String name;
    public final Integer value;
    SendProtocolMessageType(String name, Integer value) {
        this.value = value;
        this.name = name;
    }

    public static SendProtocolMessageType fromShort(Integer value) {
        for (SendProtocolMessageType type : SendProtocolMessageType.values()) {
            if (type.value.equals(value)) {
                return type;
            }
        }
        return SendProtocolMessageType.NONE;
    }

    public static SendProtocolMessageType fromName(String name) {
        for (SendProtocolMessageType type : SendProtocolMessageType.values()) {
            if (type.name.equals(name)) {
                return type;
            }
        }
        return SendProtocolMessageType.NONE;
    }
}
