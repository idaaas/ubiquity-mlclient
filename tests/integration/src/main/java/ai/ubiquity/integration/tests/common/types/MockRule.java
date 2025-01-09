package ai.ubiquity.integration.tests.common.types;

public record MockRule(String id,
                       InputMessageType on,
                       InputConnectionMessageType subType,
                       SendProtocolMessageType send,
                       String who,
                       String what,
                       int when,
                       int waitMs) {

    public String toString() {
        return "rule["+id+"] "+on.name+"."+subType.name+" => "+send.name+" "+who+" "+when+" "+ waitMs;
    }
}
