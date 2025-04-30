#pragma once

#include "ubiquity_client/Ubiquity.hpp"
#include "ClusterClient.hpp"

#include <cstddef>

namespace HandleState {
    enum Value : uint8_t {
        READY = 0,
        PREDICTING,
        PREDICT_SUCCEEDED,
        POLLED,
        TRAINED,
        CANCELED,
    };
    static constexpr const char *originFunctionName[]{
        nullptr,
        nullptr,
        nullptr,
        "poll",
        "train",
        "cancel",
    };
}

namespace ErrorOrigin {
    enum Value : uint8_t {
        PREDICT = 0,
        POLL,
        TRAIN,
        CANCEL,
    };
    static constexpr const char *functionName[]{
        "predict",
        "poll",
        "train",
        "cancel",
    };
}

class Ubiquity::ClientHandle::Impl {
    public:
        Impl(void);
        Impl(const Ubiquity::StatusCode);
        ~Impl(void);
        Ubiquity::StatusCode getStatusCode(void) noexcept;
        const char *getStatusString(void) noexcept;
        const char *getModelId(void) noexcept;
        int64_t getQueryId(void) noexcept;
        int64_t getCreativeId(void) noexcept;
        float getPrice(void) noexcept;
        float getProbability(void) noexcept;
        int32_t getPredictionType(void) noexcept;
        int8_t getBidStatus(void) noexcept;
        Ubiquity::StatusCode poll(void) noexcept;
        Ubiquity::StatusCode predict(void) noexcept;

        void putBoolean(
                const int8_t value
                ) noexcept;

        void putCategorical(
                const int8_t value
                ) noexcept;

        void putCreative(
                const int64_t creativeId
                ) noexcept;

        void putNominal(
                const char *value
                ) noexcept;

        void putNumeric(
                const float value
                ) noexcept;

        void startBidShading(
                const char *const encodedModelId,
                const int64_t queryId,
                const float bidPrice,
                const float bidFloor
                ) noexcept;

        void startProbability(
                const char *const encodedModelId,
                const int64_t queryId
                ) noexcept;

        void startLineItem(
                const char *const encodedModelId,
                const int64_t queryId,
                const float maxCPM,
                const float bidFloor,
                const int32_t exposureCount
                ) noexcept;

        void startActionRate(
                const char *const encodedModelId,
                const int64_t queryId,
                const float bidPrice
                ) noexcept;

        Ubiquity::StatusCode train(
                const char *const encodedModelId,
                int64_t queryId,
                int level
                ) noexcept;

        Ubiquity::StatusCode cancel(
                const char *const encodedModelId,
                int64_t queryId
                ) noexcept;

        const char *getDiagnosticString() noexcept;

        Ubiquity::StatusCode statusCode = Ubiquity::SUCCESS;
        HandleState::Value state = HandleState::READY;
        ErrorOrigin::Value errorOrigin;
        std::string diagnosticString;

        uint64_t featureTypeList[8];
        char encodedModelId[14];
        int64_t queryId;
        Ubiquity::ApplicationType applicationId = Ubiquity::APP_UNDEFINED;
        int32_t numCreative = 0;
        size_t featureOrdinal = 0;
        size_t offset = 0;

        bool misplacedCreativeId = 0;
        bool tooManyCreatives = 0;

        ClusterClient::Schema schema;

        std::shared_ptr<ClusterClient> clusterClient;

        aeron::concurrent::AtomicBuffer atomicBuffer;

        inline void putFeature8(
                const uint8_t value,
                const ClusterClient::Schema::FeatureType featureType
                ) noexcept;

        inline void putFeature32(
                const uint32_t value,
                const ClusterClient::Schema::FeatureType featureType
                ) noexcept;

        enum class MessageType : uint8_t;

        inline void prepareCommonHeader(
                const char *const encodedModelId,
                const int64_t queryId,
                const MessageType messageType
                ) noexcept;

        inline void prepareQueryHeader(
                const char *const encodedModelId,
                const int64_t queryId,
                const Ubiquity::ApplicationType applicationId
                ) noexcept;

        inline Ubiquity::StatusCode checkPredictSetupErrors();
        inline Ubiquity::StatusCode checkFeedbackSetupErrors();

        struct ApplicationMessage {
            uint8_t version;
            uint8_t type;
            uint16_t clientId;
            Ubiquity::ApplicationType applicationId;
            union {
                uint16_t schemaVersion;
                uint16_t feedbackLevel;
            } reserved;
            uint64_t modelId;
            int64_t queryId;
        } __attribute__((packed));

        struct BidShadingPrediction {
            ApplicationMessage _;
            float price;
            float probability;
        } __attribute__((packed));

        struct ProbabilityPrediction {
            ApplicationMessage _;
            float probability;
        } __attribute__((packed));

        struct LineItemPrediction {
            ApplicationMessage _;
            int64_t creativeId;
            float price;
            float probability;
        } __attribute__((packed));

        struct ActionRatePrediction {
            ApplicationMessage _;
            int64_t creativeId;
            float price;
        } __attribute__((packed));

        struct QueryMessage {
            ApplicationMessage _;
            uint32_t feedbackDelay;
        } __attribute__((packed));

        struct BidShadingQueryMessage {
            QueryMessage _;
            float bidFloor;
            float bidPrice;
        } __attribute__((packed));

        struct LineItemQueryMessage {
            QueryMessage _;
            float maxCPM;
            float bidFloor;
            int32_t exposureCount;
            int32_t numCreative;
        } __attribute__((packed));

        struct ActionRateQueryMessage {
            QueryMessage _;
            float bidPrice;
            int32_t numCreative;
        } __attribute__((packed));

        struct FeedbackMessage {
            ApplicationMessage _;
            uint32_t feedbackDelay;
            uint32_t nextFeedbackDelay;
        } __attribute__((packed));

        struct BidShadingFeedbackMessage {
            FeedbackMessage _;
            double weight;
            float proba; // deprecated
            float bidPrice; // deprectated
            uint32_t executed;
        } __attribute__((packed));

        struct ProbabilityFeedbackMessage {
            FeedbackMessage _;
            double weight;
            uint32_t executed;
        } __attribute__((packed));

        struct LineItemFeedbackMessage {
            FeedbackMessage _;
            double weight;
            int64_t creativeId; // deprecated
            float conversionProba; // deprecated
            uint32_t executed;
        } __attribute__((packed));

        struct ActionRateFeedbackMessage {
            FeedbackMessage _;
            double weight;
            int64_t creativeId; // deprecated
            uint32_t executed;
        } __attribute__((packed));

        struct FeedbackNextLevelsMessage {
            FeedbackMessage _;
            uint32_t eventOccurred;
        } __attribute__((packed));

        struct CancelMessage {
          ApplicationMessage _;
          uint32_t feedbackDelay;
        };

        union {
            uint8_t buffer[ClusterClient::MAXIMUM_TRANSMISSION_UNIT];
            ApplicationMessage common;
            BidShadingPrediction bidShadingPrediction;
            ProbabilityPrediction probabilityPrediction;
            LineItemPrediction lineItemPrediction;
            ActionRatePrediction actionRatePrediction;
            QueryMessage query;
            BidShadingQueryMessage bidShadingQuery;
            LineItemQueryMessage lineItemQuery;
            ActionRateQueryMessage actionRateQuery;
            FeedbackMessage feedback;
            BidShadingFeedbackMessage bidShadingFeedback;
            ProbabilityFeedbackMessage probabilityFeedback;
            LineItemFeedbackMessage lineItemFeedback;
            ActionRateFeedbackMessage actionRateFeedback;
            FeedbackNextLevelsMessage feedbackNextLevels;
            CancelMessage cancelMessage;
        } message;
    private:
      void putCreativeLineItem(
        const int64_t creativeId
        ) noexcept;
      void putCreativeActionRate(
        const int64_t creativeId
        ) noexcept;
  


};
