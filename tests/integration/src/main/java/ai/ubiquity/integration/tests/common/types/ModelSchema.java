package ai.ubiquity.integration.tests.common.types;

import java.util.List;

public record ModelSchema(long modelId, ApplicationIdType appId, List<Integer> feedbackDelays, List<FeatureType> featureTypes) {
}
