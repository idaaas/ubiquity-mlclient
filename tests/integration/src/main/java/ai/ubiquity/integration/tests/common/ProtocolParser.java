package ai.ubiquity.integration.tests.common;

import ai.ubiquity.integration.tests.common.types.*;
import ai.ubiquity.integration.tests.common.utils.Types;
import com.typesafe.config.Config;
import com.typesafe.config.ConfigFactory;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class ProtocolParser {

    public static RuleSelector parseMessageHandlerConf(String serverId) {
        List<? extends Config> nodes = ConfigFactory.load().getConfigList("mocks");

        HashMap<String, List<MockRule>> mockMap = new HashMap<>();

        System.out.println("-------------Configure rules-------------");

        for (Config rule : nodes) {
            String id = rule.hasPath("id") ? rule.getString("id") : "undefinedId";
            MockRule parsedOn = parseOnStatement(rule);
            SendProtocolMessageType send = rule.hasPath("send") ? SendProtocolMessageType.fromName(rule.getString("send")) : SendProtocolMessageType.NONE;
            String who = rule.hasPath("who") ? rule.getString("who") : "";
            String what = rule.hasPath("what") ? rule.getString("what") : "";
            int when =  rule.hasPath("when") ? Types.parseIntOrDefault(rule.getString("when"), 0) : 0;
            int delayMs = rule.hasPath("waitMs") ? Types.parseIntOrDefault(rule.getString("waitMs"), 0) : 0;

            MockRule conf = new MockRule(id, parsedOn.on(), parsedOn.subType(), send, who, what, when, delayMs);
            System.out.println(conf);

            if (!mockMap.containsKey(conf.on().name)) mockMap.put(conf.on().name, new ArrayList<>());
            mockMap.get(conf.on().name).add(conf);
        }
        System.out.println("-----------------------------------------");

        return new RuleSelector(mockMap);
    }

    static MockRule parseOnStatement(Config rule) {
        String value = rule.hasPath("on") ? rule.getString("on") : "NONE";
        String id = rule.hasPath("id") ? rule.getString("id") : "undefinedId";
        String[] types = value.split("\\.");
        if (types.length == 1) {
            InputMessageType type =  InputMessageType.fromName(types[0]);
            return new MockRule(id, type, InputConnectionMessageType.NONE, SendProtocolMessageType.NONE, "", "", 0, 0);
        }

        InputMessageType type =  InputMessageType.fromName(types[0]);
        InputConnectionMessageType subType = InputConnectionMessageType.fromName(types[1]);

        return new MockRule(id, type, subType, SendProtocolMessageType.NONE, "", "", 0, 0);
    }
}
