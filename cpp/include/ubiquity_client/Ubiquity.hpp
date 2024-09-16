#pragma once

#include <cstdint>
#include <limits>
#include <memory>

class Ubiquity {
    public:
        struct ErrorCode {
            using Type = int32_t;
            static constexpr Type SUCCESS = 1;
            static constexpr Type PENDING = 2;
            static constexpr Type TIMEOUT = 3;
            static constexpr Type ARCHITECTURE_NOT_SUPPORTED = 4;
            static constexpr Type AERON_INITIALIZATION_FAILED = 5;
            static constexpr Type FORBIDDEN = 6;
            static constexpr Type INVALID_RESOURCE_ID = 7;
            static constexpr Type UNKNOWN_RESOURCE = 8;
            static constexpr Type OTHER_FETCH_ERROR = 9;
            static constexpr Type INVALID_CLUSTER_DESCRIPTION = 10;
            static constexpr Type INVALID_SCHEMA_DESCRIPTION = 11;
            static constexpr Type CLIENT_NOT_CONNECTED = 12;
            static constexpr Type SCHEMA_UNAVAILABLE = 13;
            static constexpr Type INVALID_APPLICATION_FOR_MODEL = 14;
            static constexpr Type MISPLACED_CREATIVE_ID = 15;
            static constexpr Type TOO_FEW_CREATIVES = 16;
            static constexpr Type TOO_MANY_CREATIVES = 17;
            static constexpr Type MISMATCHED_FEATURE_TYPE = 18;
            static constexpr Type INVALID_FEEDBACK_LEVEL = 19;
            static constexpr Type MTU_EXCEEDED = 20;
            static constexpr Type BACK_PRESSURED = 21;
            static constexpr Type NO_COMPATIBLE_NODE = 22;
            static constexpr Type INVALID_PREDICTION_RESPONSE = 23;
        };
        struct Application {
            using Type = int16_t;
            static constexpr Type BID_SHADING = 0;
            static constexpr Type PROBABILITY = 1;
            static constexpr Type LINE_ITEM = 2;
            static constexpr Type UNDEFINED = 3;
        };
        struct Missing {
            static constexpr bool BOOLEAN = static_cast<bool>(-1);
            static constexpr int8_t CATEGORICAL = -1;
            static constexpr const char *NOMINAL = "_MISSING_";
            static constexpr float NUMERIC = std::numeric_limits<float>::quiet_NaN();
        };
        class ClientHandle {
            public:
                void close(void) noexcept;
                int64_t getCreativeId(void) noexcept;
                int64_t getQueryId(void) noexcept;
                const char *getModelId(void) noexcept;
                const char *getClusterId(void) noexcept;
                ErrorCode::Type getLastError(void) noexcept;
                const char *getLastErrorString(void) noexcept;
                float getPrice(void) noexcept;
                Application::Type getPredictionType(void) noexcept;
                float getProbability(void) noexcept;
                const char *getStatus(void) noexcept;
                ErrorCode::Type poll(void) noexcept;
                ErrorCode::Type predict(void) noexcept;
                void putBoolean(const bool value) noexcept;
                void putCategorical(const int8_t value) noexcept;
                void putCreative(const int64_t value) noexcept;
                void putNominal(const char *value) noexcept;
                void putNumeric(const float value) noexcept;
                void setBidPrice(const float bidPrice) noexcept;
                void setCreativeId(const int64_t creativeId) noexcept;
                void setConversionProbability(const float conversionProbability) noexcept;
                void setWinningProbability(const float winningProbability) noexcept;
                void startBidShading(const char *modelId, const int64_t queryId, const float bidPrice, const float bidFloor) noexcept;
                void startLineItem(const char *modelId, const int64_t queryId, const float maxCPM) noexcept;
                void startProbabilityEstimation(const char *modelId, const int64_t queryId) noexcept;
                ErrorCode::Type train(const char *modelId, int64_t queryId, int level) noexcept;

                class Impl;
                ClientHandle(std::shared_ptr<Impl> &&pimpl) noexcept;
            private:
                std::shared_ptr<Impl> pimpl;
        };
        static const char *getErrorString(
                ErrorCode::Type errorCode
                ) noexcept;

        static void setAeronDirectory(
                const char *const directoryPath
                ) noexcept;

        static void setCredential(
                const char *const tenantId
                ) noexcept;

        static void setApiAddress(
                const char *const root
                ) noexcept;

        static void setResourceStringInCache(
                const char *const id,
                const char *const configString
                ) noexcept;

        static ClientHandle getClientHandle(void) noexcept;

        static ClientHandle getClientHandleById(
                const char *const clusterId
                ) noexcept;

        static ErrorCode::Type openClient(
                const char *const clusterId
                ) noexcept;

        static void closeClient(
                const char *const clusterId
                ) noexcept;

        static int64_t xxh3_64bits(
                const char *source
                ) noexcept;

        ~Ubiquity(void);

};
