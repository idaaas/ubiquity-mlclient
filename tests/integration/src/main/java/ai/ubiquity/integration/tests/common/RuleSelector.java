package ai.ubiquity.integration.tests.common;

import java.util.*;

import ai.ubiquity.integration.tests.common.types.InputConnectionMessageType;
import ai.ubiquity.integration.tests.common.types.InputMessageType;
import ai.ubiquity.integration.tests.common.types.MockRule;

public class RuleSelector {
    private final Map<String, List<MockRule>> mockMap;
    private final Map<String, Integer> nbOfEvent = new HashMap<>();

    public RuleSelector(Map<String, List<MockRule>> mockMap) {
      this.mockMap = mockMap;
    }

    public MockRule getAppliedProtocol(InputMessageType type, InputConnectionMessageType subType, String serverId) {
        if (!mockMap.containsKey(type.name)) return null;
        
        List<MockRule> list = mockMap.get(type.name);
        Integer nbEvent = getNbEventAndIncreaseByOne(type, subType);

        if (list == null) return null;

        List<MockRule> whoList = getServerSpecificBehaviour(list, type, subType, serverId);
        if (!whoList.isEmpty()) return applyWhen(whoList, nbEvent);

        List<MockRule> generalList = getServerGeneralBehaviour(list, type, subType);
        return applyWhen(generalList, nbEvent);
    }

    private Integer getNbEventAndIncreaseByOne(InputMessageType type, InputConnectionMessageType subType) {
        if (!nbOfEvent.containsKey(type.name+ subType.name)) {
          nbOfEvent.put(type.name+ subType.name, 0);
        }

        Integer nbEvent = nbOfEvent.get(type.name+ subType.name);
        nbOfEvent.put(type.name+ subType.name, nbEvent+1);
        return nbEvent;
    }

    MockRule applyWhen(List<MockRule> mockRules, Integer nbOfReceivedEvent) {
      int sum = 0;
      for (MockRule rule: mockRules) {
          if (rule.when() <= 0) return rule;// if rule has no when clause => pass
          if (nbOfReceivedEvent-sum < rule.when()) return rule;// if nb of execution in when clause of current rule

          sum += rule.when(); // else pass rule
      }

      return mockRules.get(0);
    }



    List<MockRule> getServerSpecificBehaviour(List<MockRule> list, InputMessageType type, InputConnectionMessageType subType, String id) {
        return list.stream().filter(p -> p.who().equals(id) && subType.equals(p.subType())).toList();
    }

    List<MockRule> getServerGeneralBehaviour(List<MockRule> list, InputMessageType type, InputConnectionMessageType subType) {
        return list.stream().filter(p -> p.who().isEmpty() && subType.equals(p.subType())).toList();
    }

    public MockRule getAppliedProtocolWithTimeouts(InputMessageType inputMessageType, long elapsedTime, String id) {
        if (!mockMap.containsKey(inputMessageType.name)) return null;

        List<MockRule> list = mockMap.get(inputMessageType.name);
        list.sort(Comparator.comparingInt(MockRule::waitMs));
        list = list.stream().filter((r) -> Objects.equals(r.who(), id) || Objects.equals(r.who(), "")).toList();
        if (!nbOfEvent.containsKey(inputMessageType.name)) {
            nbOfEvent.put(inputMessageType.name, 0);
        }

        Integer nbEvent = nbOfEvent.get(inputMessageType.name);
        if (nbEvent >= list.size()) return null;

        MockRule rule = list.get(nbEvent);

        if (rule.waitMs() < elapsedTime) {
            nbOfEvent.put(inputMessageType.name, nbEvent+1);
            return rule;
        }

        return null;
    }
}
