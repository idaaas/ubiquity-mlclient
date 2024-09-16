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

    struct Schema {
        int64_t modelId = 0;
        Ubiquity::Application::Type applicationId;
        std::vector<uint32_t> feedbackDelays = {static_cast<uint32_t>(-1)};
        enum class FeatureType : uint8_t {
            UNDEFINED = 0,
            BOOLEAN,
            NUMERIC,
            NOMINAL,
            CATEGORICAL,
        };
        std::vector<FeatureType> features;
    };
    using SchemaCache = std::unordered_map<size_t, Schema>;
    static std::shared_ptr<SchemaCache> s_schemaCache;

    std::string clusterId;
    std::shared_ptr<LoadBalancer> loadBalancer;
    std::atomic<Ubiquity::ErrorCode::Type> errorCode = Ubiquity::ErrorCode::PENDING;

    static std::shared_ptr<aeron::Aeron> s_aeronClient;
    static std::string s_tenantId;

    ClusterClient(const std::string &clusterId) noexcept;

    std::string getStatus(void) noexcept;

//shared with NodeClient.cpp:
    size_t boundNcCount = 0;
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

    static Ubiquity::ErrorCode::Type ensureSchemaForOneModelIsInCache(
            const int64_t modelId
            ) noexcept;

//shared with Ubiquity.cpp:
    std::atomic<bool> canTerminate = false;

    static std::map<std::string, std::string> s_resourceCache;
    static std::string s_apiUrlRoot;

    static void clusterThreadFunction(
            std::shared_ptr<ClusterClient> cc
            ) noexcept;

    static Ubiquity::ErrorCode::Type ensureSchemaForAllModelsAreInCache(void) noexcept;

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

    static inline bool loadApplicationIdFromDescription(
            Schema &schema,
            const nlohmann::json &features
            ) noexcept;

    static inline void loadSchemaFeaturesFromDescription(
            Schema &schema,
            const nlohmann::json &features
            ) noexcept;

    inline bool loadIpListFromDescription(
            const nlohmann::json &ipStringArray
            ) noexcept;

    inline bool configureFromString(
            const char *configurationString
            ) noexcept;

    static inline const std::string getResourceStringFromCache(
            const std::string &resourceId
            ) noexcept;

    static inline Ubiquity::ErrorCode::Type createSchemaFromDescription(
            Schema &newSchema,
            const nlohmann::json &schemaDescription,
            const Ubiquity::Application::Type applicationId
            ) noexcept;

    static inline Ubiquity::ErrorCode::Type createSchemaInCacheFromDescription(
            const std::string &schemaDescriptionString,
            const Ubiquity::Application::Type applicationId
            ) noexcept;

    static inline Ubiquity::ErrorCode::Type fetchResourceFromRestApi(
            std::string *const destination,
            const std::string &url
            ) noexcept;

};
