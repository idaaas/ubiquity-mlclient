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

inline bool ClusterClient::loadApplicationIdFromDescription(
        Schema &schema,
        const nlohmann::json &applicationLabel
        ) noexcept
{
    static const struct {
        const char *const label;
        Ubiquity::Application::Type applicationId;
    } labelAndId[] = {
        "bid-shading",              Ubiquity::Application::BID_SHADING,
        "line-item",                Ubiquity::Application::LINE_ITEM,
        "probability",   Ubiquity::Application::PROBABILITY,
    };
    for (const auto& [label, type] : labelAndId) {
        if (applicationLabel == label) {
            schema.applicationId = type;
            return true;
        }
    }
    return false;
}

inline void ClusterClient::loadSchemaFeaturesFromDescription(
        Schema &schema,
        const nlohmann::json &features
        ) noexcept
{
    static const struct {
        const char *const label;
        Schema::FeatureType type;
    } labelAndType[] = {
        "boolean",      Schema::FeatureType::BOOLEAN,
        "numeric",      Schema::FeatureType::NUMERIC,
        "nominal",      Schema::FeatureType::NOMINAL,
        "categorical",  Schema::FeatureType::CATEGORICAL,
    };
    // TODO : error on bad type ?

    for (const auto& feature : features) {
        for (const auto& [key, value] : feature.items()) {
            if (key == "type"){
                for (const auto& [label, type] : labelAndType) {
                    if (value == label) {
                        schema.features.push_back(type);
                        break;
                    }
                }
                break;
            }
        }
    }
    schema.features.push_back(Schema::FeatureType::UNDEFINED);
}

inline Ubiquity::ErrorCode::Type ClusterClient::createSchemaFromDescription(
        Schema &newSchema,
        const nlohmann::json &schemaDescription,
        const Ubiquity::Application::Type applicationId
        ) noexcept
{
    std::string encodedModelId = schemaDescription.value("model_id", "");
    if (encodedModelId.size() != 13 || !Base32C::decode(reinterpret_cast<uint64_t *>(&newSchema.modelId), encodedModelId.c_str())) {
        return Ubiquity::ErrorCode::INVALID_SCHEMA_DESCRIPTION;
    }
    if (Ubiquity::Application::UNDEFINED != applicationId) {
        newSchema.applicationId = applicationId;
    }
    else if (!loadApplicationIdFromDescription(newSchema, schemaDescription["kind"])) {
        return Ubiquity::ErrorCode::INVALID_SCHEMA_DESCRIPTION;
    }
    newSchema.feedbackDelays.clear();
    newSchema.feedbackDelays.reserve(schemaDescription["feedback_delays"].size());
    if (!schemaDescription["feedback_delays"].is_array() || schemaDescription["feedback_delays"].size() == 0) {
        return Ubiquity::ErrorCode::INVALID_SCHEMA_DESCRIPTION;
    }
    for (auto &delay : schemaDescription["feedback_delays"]) {
        newSchema.feedbackDelays.push_back(delay.get<uint32_t>());
    }
    newSchema.feedbackDelays.push_back(-1);
    if (!schemaDescription["features"].is_array() || schemaDescription["features"].size() == 0) {
        return Ubiquity::ErrorCode::INVALID_SCHEMA_DESCRIPTION;
    }
    loadSchemaFeaturesFromDescription(newSchema, schemaDescription["features"]);
    return Ubiquity::ErrorCode::SUCCESS;
}

inline Ubiquity::ErrorCode::Type ClusterClient::createSchemaInCacheFromDescription(
        const std::string &schemaDescriptionString,
        const Ubiquity::Application::Type applicationId
        ) noexcept
{
    DBG("Creating schema in cache from fetched description");
    nlohmann::json schemaDescriptionList;
    try {
        std::istringstream(schemaDescriptionString.c_str()) >> schemaDescriptionList;
    } catch (nlohmann::json::exception &e) {
        return Ubiquity::ErrorCode::INVALID_SCHEMA_DESCRIPTION;
    }
    Schema *newSchema;
    size_t count;
    if (schemaDescriptionList["items"].is_array()) {
        count = schemaDescriptionList["items"].size();
        newSchema = new Schema[count];
        size_t i = 0;
        for (auto &schemaDescription : schemaDescriptionList["items"]) {
            createSchemaFromDescription(newSchema[i], schemaDescription, Ubiquity::Application::UNDEFINED);
            ++i;
        }
    } else {
        count = 1;
        newSchema = new Schema[1];
        createSchemaFromDescription(newSchema[0], schemaDescriptionList, applicationId);
    }

    static std::mutex s_cacheMutex;
    std::lock_guard<std::mutex> guard(s_cacheMutex);
    const auto &newSchemaCache = std::make_shared<SchemaCache>(*s_schemaCache);
    for (size_t i = 0; i < count; ++i) {
        DBG("Inserting schema for model " << newSchema[i].modelId);
        newSchemaCache->insert({newSchema[i].modelId, newSchema[i]});
    }
    delete[] newSchema;
    std::atomic_store(&s_schemaCache, newSchemaCache);
    return Ubiquity::ErrorCode::SUCCESS;
}

inline bool ClusterClient::loadIpListFromDescription(
        const nlohmann::json &ipStringArray
        ) noexcept
{
    if (!ipStringArray.is_array() || ipStringArray.size() == 0) {
        this->errorCode = Ubiquity::ErrorCode::INVALID_CLUSTER_DESCRIPTION;
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
        this->errorCode = Ubiquity::ErrorCode::INVALID_CLUSTER_DESCRIPTION;
        return false;
    }
    this->addNodesToTopology(&ipList[0], ipList.size());
    this->topologyAlreadyReceived = false;
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
        this->errorCode = Ubiquity::ErrorCode::INVALID_CLUSTER_DESCRIPTION;
        return false;
    }
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

inline Ubiquity::ErrorCode::Type ClusterClient::fetchResourceFromRestApi(
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
            return Ubiquity::ErrorCode::TIMEOUT;
        } else {
            return Ubiquity::ErrorCode::OTHER_FETCH_ERROR;
        }
    }
    switch (httpStatus) {
        case 200:
            break;
        case 404:
            return Ubiquity::ErrorCode::UNKNOWN_RESOURCE;
        case 400:
            return Ubiquity::ErrorCode::INVALID_RESOURCE_ID;
        case 500:
            return Ubiquity::ErrorCode::FORBIDDEN;
        default:
            DBG("Failed to fetch resource from " << url << " (HTTP status " << httpStatus << ")");
            return Ubiquity::ErrorCode::OTHER_FETCH_ERROR;
    }
    return Ubiquity::ErrorCode::SUCCESS;
}

Ubiquity::ErrorCode::Type ClusterClient::ensureSchemaForOneModelIsInCache(
        const int64_t modelId
        ) noexcept
{
    DBG("Looking for schema with model ID " << modelId << " in schema cache");
    const auto schemaCache = atomic_load(&s_schemaCache);
    const auto it = schemaCache->find(modelId);
    if (schemaCache->end() != it) {
        return Ubiquity::ErrorCode::SUCCESS;
    }
    char encodedModelId[14];
    Base32C::encode(encodedModelId, reinterpret_cast<const uint64_t *>(&modelId));
    std::string schemaDescriptionString = getResourceStringFromCache(encodedModelId);
    Ubiquity::Application::Type applicationId = Ubiquity::Application::LINE_ITEM;
    if (schemaDescriptionString.empty()) {
        Ubiquity::ErrorCode::Type errorCode;
        errorCode = fetchResourceFromRestApi(&schemaDescriptionString, std::string(s_apiUrlRoot + "/models/line-item/") + encodedModelId);
        if (Ubiquity::ErrorCode::UNKNOWN_RESOURCE == errorCode) {
            applicationId = Ubiquity::Application::BID_SHADING;
            errorCode = fetchResourceFromRestApi(&schemaDescriptionString, std::string(s_apiUrlRoot + "/models/bid-shading/") + encodedModelId);
        }
        if (Ubiquity::ErrorCode::SUCCESS != errorCode) {
            return errorCode;
        }
    }
    return createSchemaInCacheFromDescription(schemaDescriptionString, applicationId);
}

Ubiquity::ErrorCode::Type ClusterClient::ensureSchemaForAllModelsAreInCache(void) noexcept
{
    std::string schemaDescriptionString = getResourceStringFromCache("ALL_SCHEMAS");
    if (schemaDescriptionString.empty()) {
        Ubiquity::ErrorCode::Type errorCode;
        errorCode = fetchResourceFromRestApi(&schemaDescriptionString, s_apiUrlRoot + "/models");
        if (Ubiquity::ErrorCode::SUCCESS != errorCode) {
            return errorCode;
        }
    }
    return createSchemaInCacheFromDescription(schemaDescriptionString, Ubiquity::Application::UNDEFINED);
}

inline const std::string ClusterClient::getResourceStringFromCache(
        const std::string &resourceId
        ) noexcept {
    DBG("Looking for string resource with ID " << resourceId << " in resource cache");
    auto it = s_resourceCache.find(resourceId);
    if (s_resourceCache.end() != it) {
        DBG("Using resource string from cache");
        return it->second;
    }
    return std::string();
}

inline bool ClusterClient::loadConfiguration(void) noexcept {
    DBG("ClusterClient(" << this->clusterId << ") : Loading cluster configuration");
    std::string configString = getResourceStringFromCache(this->clusterId);
    if (configString.empty()) {
        Ubiquity::ErrorCode::Type errorCode = fetchResourceFromRestApi(&configString, s_apiUrlRoot + "/clusters/" + this->clusterId);
        if (Ubiquity::ErrorCode::SUCCESS != errorCode) {
            this->errorCode = errorCode;
            return false;
        }
    }
    if (!this->configureFromString(configString.c_str())) {
        DBG("ClusterClient(" << this->clusterId
                << ") : Configuration failed with error " << Ubiquity::getErrorString(this->errorCode));
        return false;
    }
    return true;
}

/*
 * CLUSTER CREATION AND MAIN LOOP
 */

ClusterClient::ClusterClient(
        const std::string &clusterId
        ) noexcept
{
    this->clusterId = clusterId;
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
void ClusterClient::clusterThreadFunction(
        std::shared_ptr<ClusterClient> cc
        ) noexcept
{
    DBG("ClusterClient(" << cc->clusterId << ") : Starting dedicated thread");
    if (cc->loadConfiguration()) {
        while (cc.use_count() > 1 || cc->boundNcCount > 0) {
            if (cc.use_count() == 1 && !cc->isTerminating && std::atomic_load(&cc->canTerminate)) {
                cc->terminate();
            }
            for (size_t i = 0; i < cc->nextUnboundNcIndex; ++i) {
                cc->ncArray[i].invoke();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    DBG("ClusterClient(" << cc->clusterId << ") : Terminated");
}

std::string ClusterClient::getStatus(void) noexcept
{
    nlohmann::json json = {
        {"nodeClient_nbr", this->boundNcCount},
        {"nodeClients", nlohmann::json::array()}
    };
    for (size_t i = 0; i < this->nextUnboundNcIndex; ++i) {
        auto &nc = this->ncArray[i];
        if (nc.isUnbound()) {
            continue;
        }
        nlohmann::json ncStatus = {
            {"clientId", nc.clientId},
            {"ip", nc.ipString},
            {"state", nc.getStateString()},
            {"invalidMessageTypeCount", nc.invalidMessageTypeCount},
            {"invalidMessageLengthCount", nc.invalidMessageLengthCount},
        };
        json["nodeClients"].push_back(ncStatus);
    }
    return json.dump();
}

std::string ClusterClient::s_apiUrlRoot = std::string("https://api.ubiquity.ai/v1");
std::shared_ptr<ClusterClient::SchemaCache> ClusterClient::s_schemaCache = std::make_shared<SchemaCache>();
std::map<std::string, std::string> ClusterClient::s_resourceCache;
std::shared_ptr<aeron::Aeron> ClusterClient::s_aeronClient;
std::string ClusterClient::s_tenantId;
