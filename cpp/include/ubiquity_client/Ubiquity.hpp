#pragma once

#include <cstdint>
#include <limits>

namespace Ubiquity {

#ifndef SWIG
        using StatusCode = int32_t;
        using ApplicationType = int16_t;

        static constexpr StatusCode UNDEFINED = 0;

        static constexpr StatusCode SUCCESS = 1;
        static constexpr StatusCode NO_DATA = 2;
        static constexpr StatusCode BACK_PRESSURED = 3;
        static constexpr StatusCode NO_COMPATIBLE_NODE = 4;

        static constexpr StatusCode ARCHITECTURE_NOT_SUPPORTED      = NO_COMPATIBLE_NODE + 1;
        static constexpr StatusCode AERON_INITIALIZATION_FAILED     = NO_COMPATIBLE_NODE + 2;

        static constexpr StatusCode INVALID_RESOURCE_ID             = NO_COMPATIBLE_NODE + 3;
        static constexpr StatusCode INVALID_APP_FOR_MODEL           = NO_COMPATIBLE_NODE + 4;
        static constexpr StatusCode MISMATCHED_FEATURE_TYPE         = NO_COMPATIBLE_NODE + 5;
        static constexpr StatusCode INVALID_FEEDBACK_LEVEL          = NO_COMPATIBLE_NODE + 6;
        static constexpr StatusCode MISSING_CREATIVES               = NO_COMPATIBLE_NODE + 7;
        static constexpr StatusCode TOO_MANY_CREATIVES              = NO_COMPATIBLE_NODE + 8;
        static constexpr StatusCode MISPLACED_CREATIVE_ID           = NO_COMPATIBLE_NODE + 9;

        static constexpr StatusCode NO_API_RESPONSE                 = NO_COMPATIBLE_NODE + 10;
        static constexpr StatusCode FORBIDDEN                       = NO_COMPATIBLE_NODE + 11;
        static constexpr StatusCode UNKNOWN_RESOURCE                = NO_COMPATIBLE_NODE + 12;
        static constexpr StatusCode OTHER_FETCH_ERROR               = NO_COMPATIBLE_NODE + 13;
        static constexpr StatusCode INVALID_CLUSTER_DESCRIPTION     = NO_COMPATIBLE_NODE + 14;
        static constexpr StatusCode NO_MODEL_METADATA               = NO_COMPATIBLE_NODE + 15;
        static constexpr StatusCode INVALID_HANDLE                  = NO_COMPATIBLE_NODE + 16;
        static constexpr StatusCode UNAUTHORIZED                    = NO_COMPATIBLE_NODE + 17;
        static constexpr StatusCode INTERNAL_API_ERROR              = NO_COMPATIBLE_NODE + 18;
        static constexpr StatusCode NO_MEMORY                       = NO_COMPATIBLE_NODE + 19;
        static constexpr StatusCode CONNECTION_TIMEOUT              = NO_COMPATIBLE_NODE + 20;

        static constexpr ApplicationType APP_BID_SHADING = 0;
        static constexpr ApplicationType APP_PROBABILITY = 1;
        static constexpr ApplicationType APP_LINE_ITEM = 2;
        static constexpr ApplicationType APP_ACTION_RATE = 3;
        static constexpr ApplicationType NUMBER_OF_APP_TYPES = 4;
        static constexpr ApplicationType APP_UNDEFINED = 9999;

        static constexpr int32_t LOG_OFF = 0;

        static constexpr int32_t LOG_TRACE = 0x01;
        static constexpr int32_t LOG_DEBUG = 0x02;
        static constexpr int32_t LOG_INFO = 0x04;
        static constexpr int32_t LOG_WARNING = 0x08;
        static constexpr int32_t LOG_ERROR = 0x10;
        static constexpr int32_t LOG_CRITICAL = 0x20;

        static constexpr int32_t MIN_LOG_TRACE = 0x3f;
        static constexpr int32_t MIN_LOG_DEBUG = 0x3e;
        static constexpr int32_t MIN_LOG_INFO = 0x3c;
        static constexpr int32_t MIN_LOG_WARNING = 0x38;
        static constexpr int32_t MIN_LOG_ERROR = 0x30;
        static constexpr int32_t MIN_LOG_CRITICAL = 0x20;

        static constexpr int32_t MAX_LOG_TRACE = 0x01;
        static constexpr int32_t MAX_LOG_DEBUG = 0x03;
        static constexpr int32_t MAX_LOG_INFO = 0x07;
        static constexpr int32_t MAX_LOG_WARNING = 0x0f;
        static constexpr int32_t MAX_LOG_ERROR = 0x1f;
        static constexpr int32_t MAX_LOG_CRITICAL = 0x3f;

        static constexpr int8_t MISSING_BOOLEAN = -1;
        static constexpr int8_t MISSING_CATEGORICAL = -1;

        static constexpr int8_t BID = 1;
        static constexpr int8_t NO_BID = 0;

        static constexpr int32_t WIN_RATE_PRESERVATION = 0;
        static constexpr int32_t EXPECTED_SURPLUS_MAXIMIZATION = 1;

        static constexpr const char *MISSING_NOMINAL = "_MISSING_";
        static constexpr float MISSING_NUMERIC = std::numeric_limits<float>::quiet_NaN();
#endif

        class ClientHandle {

            public:
                ClientHandle(void) noexcept;

                void close(void) noexcept;
                int64_t getCreativeId(void) noexcept;
                int64_t getQueryId(void) noexcept;
                const char *getModelId(void) noexcept;
                int32_t getStatusCode(void) noexcept;
                const char *getStatusString(void) noexcept;
                float getPrice(void) noexcept;
                int32_t getPredictionType(void) noexcept;
                float getProbability(void) noexcept;
                int8_t getBidStatus(void) noexcept;
                int32_t poll(void) noexcept;
                void idle(void) noexcept;
                int32_t predict(const char *modelId, const int64_t queryId) noexcept;
                void putBoolean(const int8_t value) noexcept;
                void putCategorical(const int8_t value) noexcept;
                void putCreative(const int64_t value) noexcept;
                void putNominal(const char *value) noexcept;
                void putNumeric(const float value) noexcept;
                void startBidShading(const float bidPrice, const float bidFloor, int32_t optimizationMode=0) noexcept;
                void startLineItem(const float maxCPM, const float bidFloor=0.01f, const int32_t exposureCount=0) noexcept;
                void startActionRate(const float bidPrice, const float bidFloor=0.01F, const int32_t exposureCount=0) noexcept;
                void startProbability(void) noexcept;
                int32_t trainWithLevel(const char *modelId, int64_t queryId, const uint16_t feedback_level) noexcept;
                int32_t train(const char *modelId, int64_t queryId, const char *const feedback_name) noexcept;
                int32_t cancel(const char *modelId, int64_t queryId) noexcept;
                const char *getDiagnosticString() noexcept;
                void setLogLevel(const int32_t level) noexcept;

            private:
                friend ClientHandle getClientHandle(void) noexcept;
                void *pimpl;
        };
        const char *getStatusString(
                int32_t statusCode
                ) noexcept;

        void setAeronDirectory(
                const char *const directoryPath
                ) noexcept;

        // Deprecated.
        void setCredential(
                const char *const appToken
                ) noexcept;

        void setCredentials(
                const char *const appToken
                ) noexcept;

        void setApiAddress(
                const char *const root
                ) noexcept;

        void setResourceStringInCache(
                const char *const id,
                const char *const configString
                ) noexcept;

        ClientHandle getClientHandle(void) noexcept;

        int32_t openClient(
                const char *const clusterId
                ) noexcept;

        void closeClient(void) noexcept;

        int64_t xxh3_64bits(
                const char *source
                ) noexcept;

        void setDefaultLogLevel(
                const int32_t level
                ) noexcept;

        void setProtocolLogLevel(
                const int32_t level
                ) noexcept;

        void setLogFileLevel(
                const char *const path,
                const int32_t level
                ) noexcept;

        void setLogFileDailyRotation(
                const char *const path,
                const int32_t rotationHour,
                const int32_t rotationMinute
                ) noexcept;

        void setLogFileMaxSize(
                const char *const path,
                const int64_t maxSize
                ) noexcept;

        void setLogFileMaxArchives(
                const char *const path,
                const int64_t maxArchives
                ) noexcept;

        void setStdoutLevel(
                const int32_t level
                ) noexcept;

        void setStderrLevel(
                const int32_t level
                ) noexcept;

        void setSyslogLevel(
                const int32_t level
                ) noexcept;
}
