#pragma once

#include <cstdint>
#include <limits>

namespace Ubiquity {

#ifndef SWIG
        using StatusCode = int32_t;
        using ApplicationType = int16_t;

        static constexpr StatusCode SETUP_ERROR = 0x20000;

        static constexpr StatusCode UNDEFINED = 0;

        static constexpr StatusCode SUCCESS = 1;
        static constexpr StatusCode NO_DATA = 2;
        static constexpr StatusCode BACK_PRESSURED = 3;
        static constexpr StatusCode NO_COMPATIBLE_NODE = 4;

        static constexpr StatusCode ARCHITECTURE_NOT_SUPPORTED      = SETUP_ERROR | 1;
        static constexpr StatusCode AERON_INITIALIZATION_FAILED     = SETUP_ERROR | 2;

        static constexpr StatusCode INVALID_RESOURCE_ID             = SETUP_ERROR | 3;
        static constexpr StatusCode INVALID_APP_FOR_MODEL           = SETUP_ERROR | 4;
        static constexpr StatusCode MISMATCHED_FEATURE_TYPE         = SETUP_ERROR | 5;
        static constexpr StatusCode INVALID_FEEDBACK_LEVEL          = SETUP_ERROR | 6;
        static constexpr StatusCode MISSING_CREATIVES               = SETUP_ERROR | 7;
        static constexpr StatusCode TOO_MANY_CREATIVES              = SETUP_ERROR | 8;
        static constexpr StatusCode MISPLACED_CREATIVE_ID           = SETUP_ERROR | 9;

        static constexpr StatusCode NO_API_RESPONSE                 = SETUP_ERROR | 10;
        static constexpr StatusCode FORBIDDEN                       = SETUP_ERROR | 11;
        static constexpr StatusCode UNKNOWN_RESOURCE                = SETUP_ERROR | 12;
        static constexpr StatusCode OTHER_FETCH_ERROR               = SETUP_ERROR | 13;
        static constexpr StatusCode INVALID_CLUSTER_DESCRIPTION     = SETUP_ERROR | 14;
        static constexpr StatusCode SCHEMA_UNAVAILABLE              = SETUP_ERROR | 15;
        static constexpr StatusCode INVALID_HANDLE                  = SETUP_ERROR | 16;
        static constexpr StatusCode UNAUTHORIZED                    = SETUP_ERROR | 17;
        static constexpr StatusCode INTERNAL_API_ERROR              = SETUP_ERROR | 18;

        static constexpr ApplicationType APP_BID_SHADING = 0;
        static constexpr ApplicationType APP_PROBABILITY = 1;
        static constexpr ApplicationType APP_LINE_ITEM = 2;
        static constexpr ApplicationType APP_UNDEFINED = 3;

        static constexpr int8_t MISSING_BOOLEAN = -1;
        static constexpr int8_t MISSING_CATEGORICAL = -1;
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
                int32_t poll(void) noexcept;
                int32_t predict(void) noexcept;
                void putBoolean(const int8_t value) noexcept;
                void putCategorical(const int8_t value) noexcept;
                void putCreative(const int64_t value) noexcept;
                void putNominal(const char *value) noexcept;
                void putNumeric(const float value) noexcept;
                void startBidShading(const char *modelId, const int64_t queryId, const float bidPrice, const float bidFloor) noexcept;
                void startLineItem(const char *modelId, const int64_t queryId, const float maxCPM) noexcept;
                void startProbability(const char *modelId, const int64_t queryId) noexcept;
                int32_t train(const char *modelId, int64_t queryId, int32_t level) noexcept;
                int32_t cancel(const char *modelId, int64_t queryId) noexcept;

                class Impl;
            private:
                friend ClientHandle getClientHandle(void) noexcept;
                Impl *pimpl;
        };
        const char *getStatusString(
                int32_t statusCode
                ) noexcept;

        void setAeronDirectory(
                const char *const directoryPath
                ) noexcept;

        void setCredential(
                const char *const tenantId
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

}
