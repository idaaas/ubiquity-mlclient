package ai.ubiquity.integration.tests.common.utils;

import ai.ubiquity.integration.tests.common.types.SendProtocolMessageType;

public class Types {
    public static int parseIntOrDefault(String str, int defaultValue) {
        try {
            return Integer.parseInt(str);
        } catch (Exception e) {
            return defaultValue;
        }
    }
}
