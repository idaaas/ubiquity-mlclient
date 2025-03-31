#pragma once

#include "ubiquity_client/Ubiquity.hpp"
#include "NodeClient.hpp"
#include "LoadBalancer.hpp"

#include <nlohmann/json.hpp>
#include <Aeron.h>
#include <aeronmd.h>

#include <cstddef>
#include <atomic>
#include <vector>
#include <unordered_map>


class ClusterClient {
public:
    static constexpr size_t MAXIMUM_TRANSMISSION_UNIT = 1280;

    using ModelId = int64_t;
    struct Schema {
        enum class FeatureType : int64_t {
            NUMERIC = 0,
            NOMINAL = 1,
            BOOLEAN = 2,
            CATEGORICAL = 3,
            RESERVED = 4,
        };
        static constexpr const char *featureTypeStrings[]{
            "NUMERIC",
            "NOMINAL",
            "BOOLEAN",
            "CATEGORICAL",
        };
        static constexpr FeatureType featureTypeTranslation[8] {
            FeatureType::NUMERIC,
            FeatureType::RESERVED,
            FeatureType::NOMINAL,
            FeatureType::BOOLEAN,
            FeatureType::CATEGORICAL,
        };
        ModelId modelId = 0;
        uint64_t featureTypeList[8];
        uint16_t featureCount;
        uint16_t feedbackDelayCount;
        int32_t feedbackDelayList[16] = {-1};
        Ubiquity::ApplicationType applicationId = Ubiquity::APP_UNDEFINED;
    };
    using SchemaCache = std::unordered_map<ModelId, Schema>;
    static std::shared_ptr<SchemaCache> s_schemaCache;

    std::string clusterId;
    std::shared_ptr<LoadBalancer> loadBalancer;
    std::atomic<Ubiquity::StatusCode> statusCode = Ubiquity::NO_DATA;

    static std::shared_ptr<aeron::Aeron> s_aeronClient;
    static std::string s_appToken;

    ClusterClient(void) noexcept;

//shared with NodeClient.cpp:
    int nonDisconnectedNcCount = 0;
    size_t nextUnboundNcIndex = 0;
    uint8_t inputBuffer[MAXIMUM_TRANSMISSION_UNIT];
    bool topologyAlreadyReceived = false;
    using PredSubList = std::vector<std::shared_ptr<aeron::Subscription>>;
    std::shared_ptr<PredSubList> predSubList;

    void removeNodesFromTopology(
            const uint32_t *const ipList,
            const uint16_t ipCount
            ) noexcept;

    void addNodesToTopology(
            const uint32_t *const ipList,
            const uint16_t ipCount
            ) noexcept;

    void refreshTopology(
            const uint32_t *const ipList,
            const uint16_t ipCount
            ) noexcept;

    void ensureSchemaForOneModelIsInCache(
            const int64_t modelId,
            const int16_t applicationId,
            const int32_t *const feedbackDelays,
            const uint16_t feedbackDelaysCount,
            const int8_t *const features
            ) noexcept;

//shared with Ubiquity.cpp:
    std::atomic<bool> canTerminate = false;

    static std::map<std::string, std::string> s_resourceCache;
    static std::string s_apiUrlRoot;

    static void clusterThreadFunction(
            std::shared_ptr<ClusterClient> cc
            ) noexcept;

private:
    static constexpr size_t NODE_CLIENT_POOL_SIZE = 256;

    NodeClient ncArray[NODE_CLIENT_POOL_SIZE];

    bool shouldDisconnect = false;
    bool isTerminating = false;

    inline bool loadConfiguration(void) noexcept;
    inline void terminate(void) noexcept;

    inline void unbindOneClient(
            const uint32_t ip
            ) noexcept;

    inline void unbindSomeClients(
            const uint32_t *const ipList,
            uint16_t ipCount
            ) noexcept;

    inline void bindAndOrRefreshOneClient(
            const uint32_t ip
            ) noexcept;

    inline void bindAndOrRefreshSomeClients(
            const uint32_t *const ipList,
            uint16_t ipCount
            ) noexcept;

    inline void markAllClientsAsUnrefreshed(void) noexcept;

    inline void unbindAllUnrefreshedClients(void) noexcept;

    inline bool loadIpListFromDescription(
            const nlohmann::json &ipStringArray
            ) noexcept;

    inline bool configureFromString(
            const char *configurationString
            ) noexcept;

    static inline const std::string consumeResourceStringFromCache(
            const std::string &resourceId
            ) noexcept;

    static inline Ubiquity::StatusCode fetchResourceFromRestApi(
            std::string *const destination,
            const std::string &url
            ) noexcept;

};
