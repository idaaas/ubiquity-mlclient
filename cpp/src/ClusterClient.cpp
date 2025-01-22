#include "ClusterClient.hpp"
#include "debug.hpp"
#include "Base32C.hpp"

#include <curl/curl.h>

#include <atomic>
#include <arpa/inet.h>

/*
 * TOPOLOGY MANAGEMENT
 */


inline void ClusterClient::unbindOneClient(
        const uint32_t ip
        ) noexcept
{
    for (size_t ncIndex = 0; ncIndex < this->nextUnboundNcIndex; ++ncIndex) {
        NodeClient &nc = this->ncArray[ncIndex];
        if (ip == nc.ip) {
            nc.unbind();
        }
    }
}

inline void ClusterClient::unbindSomeClients(
        const uint32_t *const ipList,
        uint16_t ipCount
        ) noexcept
{
    for (size_t ipIndex = 0; ipIndex < ipCount; ++ipIndex) {
        this->unbindOneClient(ipList[ipIndex]);
    }
}

inline void ClusterClient::bindAndOrRefreshOneClient(
        const uint32_t ip
        ) noexcept
{
    size_t unboundNcIndex = this->nextUnboundNcIndex;
    for (size_t ncIndex = 0; ncIndex < this->nextUnboundNcIndex; ++ncIndex) {
        NodeClient &nc = this->ncArray[ncIndex];
        if (ip == nc.ip) {
            nc.refreshed = true;
            return;
        }
        if (nc.isUnbound()) {
            unboundNcIndex = ncIndex;
        }
    }
    if (unboundNcIndex < NODE_CLIENT_POOL_SIZE) {
        NodeClient &nc = this->ncArray[unboundNcIndex];
        nc.bind(ip, unboundNcIndex);
        nc.refreshed = true;
        if (unboundNcIndex == this->nextUnboundNcIndex) {
            this->nextUnboundNcIndex++;
        }
    }
}

inline void ClusterClient::bindAndOrRefreshSomeClients(
        const uint32_t *const ipList,
        uint16_t ipCount
        ) noexcept
{
    for (size_t ipIndex = 0; ipIndex < ipCount; ++ipIndex) {
        this->bindAndOrRefreshOneClient(ipList[ipIndex]);
    }
}

inline void ClusterClient::markAllClientsAsUnrefreshed(void) noexcept
{
    for (size_t ncIndex = 0; ncIndex < this->nextUnboundNcIndex; ++ncIndex) {
        this->ncArray[ncIndex].refreshed = false;
    }
}

inline void ClusterClient::unbindAllUnrefreshedClients(void) noexcept
{
    for (size_t ncIndex = 0; ncIndex < this->nextUnboundNcIndex; ++ncIndex)
    {
        NodeClient &nc = this->ncArray[ncIndex];
        if (!nc.refreshed && !nc.isUnbound()) {
            nc.unbind();
        }
    }
}

void ClusterClient::removeNodesFromTopology(
        const uint32_t *const ipList,
        const uint16_t ipCount
        ) noexcept
{
    if (!this->isTerminating) {
        this->unbindSomeClients(ipList, ipCount);
    }
}

void ClusterClient::addNodesToTopology(
        const uint32_t *const ipList,
        const uint16_t ipCount
        ) noexcept
{
    if (!this->isTerminating) {
        this->bindAndOrRefreshSomeClients(ipList, ipCount);
    }
}

void ClusterClient::refreshTopology(
        const uint32_t *const ipList,
        const uint16_t ipCount
        ) noexcept
{
    if (!this->isTerminating) {
        this->markAllClientsAsUnrefreshed();
        this->bindAndOrRefreshSomeClients(ipList, ipCount);
        this->unbindAllUnrefreshedClients();
    }
}

/*
 * CLUSTER CONFIGURATION LOADING
 */

inline bool ClusterClient::loadIpListFromDescription(
        const nlohmann::json &ipStringArray
        ) noexcept
{
    if (!ipStringArray.is_array() || ipStringArray.size() == 0) {
        this->statusCode = Ubiquity::INVALID_CLUSTER_DESCRIPTION;
        return false;
    }
    std::vector<uint32_t> ipList;
    ipList.reserve(ipStringArray.size());
    uint32_t ip;
    for (const auto &ipString : ipStringArray) {
        if (inet_pton(AF_INET, ipString.get<std::string>().c_str(), &ip)) {
            ipList.emplace_back(ip);
        }
    }
    if (0 == ipList.size()) {
        this->statusCode = Ubiquity::INVALID_CLUSTER_DESCRIPTION;
        return false;
    }
    this->addNodesToTopology(&ipList[0], ipList.size());
    this->topologyAlreadyReceived = false;
    this->statusCode = Ubiquity::SUCCESS;
    return true;
}

inline bool ClusterClient::configureFromString(
        const char *configurationString
        ) noexcept
{
    DBG("ClusterClient(" << this->clusterId << ") : Loading configuration");
    nlohmann::json clusterDescription;
    try {
        std::istringstream(configurationString) >> clusterDescription;
    } catch (nlohmann::json::exception &e) {
        this->statusCode = Ubiquity::INVALID_CLUSTER_DESCRIPTION;
        return false;
    }
    this->statusCode = Ubiquity::SUCCESS;
    return this->loadIpListFromDescription(clusterDescription["contact_points"]);
}

static size_t writeResourceString(void *buffer, size_t size, size_t nmemb, void *userp) noexcept {
    std::string *destination = reinterpret_cast<std::string *>(userp);
    const size_t length = size * nmemb;
    try {
        destination->append(reinterpret_cast<char *>(buffer), length);
    } catch (std::length_error &e) {
        DBG("writeResource : Received resource string is too long !");
        return 0;
    }
    return length;
}

static constexpr size_t CURL_OPERATION_TIME_LIMIT_MS = 5000L;

inline Ubiquity::StatusCode ClusterClient::fetchResourceFromRestApi(
        std::string *const destination,
        const std::string &url
        ) noexcept
{
    DBG("Fetching resource from " << url);

    destination->clear();

    CURL *curlHandle = curl_easy_init();

    curl_easy_setopt(curlHandle, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeResourceString);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, destination);
    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT_MS, CURL_OPERATION_TIME_LIMIT_MS);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "accept: application/json");
    headers = curl_slist_append(headers, ("authorization: Bearer " + s_tenantId).c_str());
    curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);

    char curlErrorString[CURL_ERROR_SIZE];
    curl_easy_setopt(curlHandle, CURLOPT_ERRORBUFFER, curlErrorString);
    CURLcode curlError = curl_easy_perform(curlHandle);

    long httpStatus;
    curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &httpStatus);

    curl_easy_cleanup(curlHandle);

    if (CURLE_OK != curlError) {
        DBG("CURL request failed with error (" << curlError << ") \"" << curlErrorString << "\"");
        if (CURLE_OPERATION_TIMEDOUT == curlError) {
            return Ubiquity::NO_API_RESPONSE;
        } else {
            return Ubiquity::OTHER_FETCH_ERROR;
        }
    }
    switch (httpStatus) {
        case 200:
            break;
        case 404:
            return Ubiquity::UNKNOWN_RESOURCE;
        case 400:
            return Ubiquity::INVALID_RESOURCE_ID;
        case 401:
            return Ubiquity::UNAUTHORIZED;
        case 403:
            return Ubiquity::FORBIDDEN;
        case 500:
        case 502:
            return Ubiquity::INTERNAL_API_ERROR;
        default:
            DBG("Failed to fetch resource from " << url << " (HTTP status " << httpStatus << ")");
            return Ubiquity::OTHER_FETCH_ERROR;
    }
    return Ubiquity::SUCCESS;
}

void ClusterClient::ensureSchemaForOneModelIsInCache(
        const int64_t modelId,
        const int16_t applicationId,
        const int32_t *const feedbackDelays,
        const int16_t feedbackDelaysCount,
        const int8_t *const features
        ) noexcept
{
    const auto &schemaCache = atomic_load(&s_schemaCache);
    const auto &schema = (*schemaCache)[modelId];
    if (schema.modelId != modelId) {
        DBG("Inserting new schema(" << modelId << ") in cache");
        Schema newSchema{};
        newSchema.modelId = modelId;
        newSchema.applicationId = applicationId;

        // Insert feedback delays.
        newSchema.feedbackDelays.clear();
        for (int16_t i = 0; i < feedbackDelaysCount; ++i) {
            const auto feedbackDelay = feedbackDelays[i];
            newSchema.feedbackDelays.push_back(feedbackDelay);
        }

        // Mark the end of the feedback delays list.
        newSchema.feedbackDelays.push_back(-1);

        // Insert feature types.
        const auto featuresCount = reinterpret_cast<const int16_t *>(features)[0];
        for (int8_t i = 0; i < featuresCount; ++i) {
            newSchema.features.push_back(static_cast<Schema::FeatureType>(features[2 + i]));
        }

        // Mark the end of the feature types list.
        newSchema.features.push_back(Schema::FeatureType::UNDEFINED);

        // Create the new schema cache containing the new schema + older schemas (copy on write).
        const auto &newSchemaCache = std::make_shared<SchemaCache>(*s_schemaCache);
        (*newSchemaCache)[newSchema.modelId] = newSchema;
        std::atomic_store(&s_schemaCache, newSchemaCache);
    }
}

inline const std::string ClusterClient::consumeResourceStringFromCache(
        const std::string &resourceId
        ) noexcept {
    DBG("Looking for string resource with ID " << resourceId << " in resource cache");
    auto it = s_resourceCache.find(resourceId);
    if (s_resourceCache.end() != it) {
        DBG("Using resource string from cache");
        auto resource = it->second;

        // The resource is consumed and must be downloaded again or set again.
        s_resourceCache.erase(it);
        return resource;
    }
    return std::string();
}

inline bool ClusterClient::loadConfiguration(void) noexcept {
    DBG("ClusterClient(" << this->clusterId << ") : Loading cluster configuration");
    std::string configString = consumeResourceStringFromCache(this->clusterId);
    if (configString.empty()) {
        Ubiquity::StatusCode statusCode = fetchResourceFromRestApi(&configString, s_apiUrlRoot + "/clusters/" + this->clusterId);
        if (Ubiquity::SUCCESS != statusCode) {
            this->statusCode = statusCode;
            return false;
        }
    }
    if (!this->configureFromString(configString.c_str())) {
        DBG("ClusterClient(" << this->clusterId
                << ") : Configuration failed with error " << Ubiquity::getStatusString(this->statusCode));
        return false;
    }
    return true;
}

/*
 * CLUSTER CLIENT CREATION AND MAIN LOOP
 */

ClusterClient::ClusterClient(void) noexcept
{
    this->loadBalancer = std::make_shared<LoadBalancer>();
    for (size_t i = 0; i < NODE_CLIENT_POOL_SIZE; ++i) {
        this->ncArray[i].initialize(this);
    }
    this->predSubList = std::make_shared<PredSubList>();
}

inline void ClusterClient::terminate(void) noexcept {
    DBG("ClusterClient(" << this->clusterId << ") : Unbinding all node clients before termination");
    for (size_t i = 0; i < this->nextUnboundNcIndex; ++i) {
        if (!this->ncArray[i].isUnbound()) {
            this->ncArray[i].unbind();
        }
    }
    this->isTerminating = true;
}

static constexpr auto TIME_BEFORE_RECONFIGURATION = std::chrono::seconds(60);
static std::chrono::time_point<std::chrono::high_resolution_clock> s_configExpirationTime;

static inline void resetConfigExpirationTime() {
    s_configExpirationTime = std::chrono::high_resolution_clock::now() + TIME_BEFORE_RECONFIGURATION;
}

static inline bool configurationIsNotExpired() {
    return std::chrono::high_resolution_clock::now() < s_configExpirationTime;
}

void ClusterClient::clusterThreadFunction(
        std::shared_ptr<ClusterClient> cc
        ) noexcept
{
    DBG("ClusterClient(" << cc->clusterId << ") : Starting dedicated thread");
    while (cc->loadConfiguration() && !cc->isTerminating) {
        resetConfigExpirationTime();
        while (configurationIsNotExpired()) {
            if (cc.use_count() == 1 && !cc->isTerminating && std::atomic_load(&cc->canTerminate)) {
                cc->terminate();
            }
            for (size_t i = 0; i < cc->nextUnboundNcIndex; ++i) {
                cc->ncArray[i].invoke();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (cc->nonDisconnectedNcCount > 0) {
                resetConfigExpirationTime();
            };
        }
    }
    DBG("ClusterClient(" << cc->clusterId << ") : Terminated");
}

std::string ClusterClient::s_apiUrlRoot = std::string("https://api.ubiquity.ai/v1");
std::shared_ptr<ClusterClient::SchemaCache> ClusterClient::s_schemaCache = std::make_shared<SchemaCache>();
std::map<std::string, std::string> ClusterClient::s_resourceCache;
std::shared_ptr<aeron::Aeron> ClusterClient::s_aeronClient;
std::string ClusterClient::s_tenantId;
