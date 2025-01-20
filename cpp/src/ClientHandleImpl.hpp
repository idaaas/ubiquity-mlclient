#pragma once

#include "ubiquity_client/Ubiquity.hpp"
#include "ClusterClient.hpp"

#include <cstddef>

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
                const float maxCPM
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

        ClusterClient::Schema schema;
        std::shared_ptr<ClusterClient> clusterClient;
        Ubiquity::StatusCode statusCode = Ubiquity::SUCCESS;
        aeron::concurrent::AtomicBuffer atomicBuffer;
        size_t featureOrdinal = 0;
        size_t offset = 0;
        char encodedModelId[14];

        inline bool nextFeatureTypeMatches(
                const ClusterClient::Schema::FeatureType featureType
                ) noexcept;

        inline void putFeature8(
                const uint8_t value,
                const ClusterClient::Schema::FeatureType featureType
                ) noexcept;

        inline void putFeature32(
                const uint32_t value,
                const ClusterClient::Schema::FeatureType featureType
                ) noexcept;

        enum class MessageType : uint8_t;

        inline Ubiquity::StatusCode getSchema(
                const int64_t modelId
                ) noexcept;

        inline void prepareCommonHeader(
                const int64_t modelId,
                const int64_t queryId,
                const Ubiquity::ApplicationType applicationId,
                const MessageType messageType
                ) noexcept;

        inline void prepareFeedbackHeader(
                const uint64_t modelId,
                const int64_t queryId,
                const Ubiquity::ApplicationType applicationId,
                const uint16_t feedbackLevel
                ) noexcept;

        inline void prepareQueryHeader(
                const char *const encodedModelId,
                const int64_t queryId,
                const Ubiquity::ApplicationType applicationId
                ) noexcept;

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

        struct FeedbackNextLevelsMessage {
            FeedbackMessage _;
            uint32_t eventOccurred;
        } __attribute__((packed));

        struct CancelMessage {
          ApplicationMessage _;
        };

        union {
            uint8_t buffer[ClusterClient::MAXIMUM_TRANSMISSION_UNIT];
            ApplicationMessage common;
            BidShadingPrediction bidShadingPrediction;
            ProbabilityPrediction probabilityPrediction;
            LineItemPrediction lineItemPrediction;
            QueryMessage query;
            BidShadingQueryMessage bidShadingQuery;
            LineItemQueryMessage lineItemQuery;
            FeedbackMessage feedback;
            BidShadingFeedbackMessage bidShadingFeedback;
            ProbabilityFeedbackMessage probabilityFeedback;
            LineItemFeedbackMessage lineItemFeedback;
            FeedbackNextLevelsMessage feedbackNextLevels;
            CancelMessage cancelMessage;
        } message;

};
