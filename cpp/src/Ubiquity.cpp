#include "ubiquity_client/Ubiquity.hpp"
#include "ClientHandleImpl.hpp"
#include "debug.hpp"

#include <aeronmd.h>
#include <xxh3.h>

#include <cstddef>
#include <utility>
#include <atomic>
#include <mutex>

#include <iostream>

const char *Ubiquity::getErrorString(
        Ubiquity::ErrorCode::Type errorCode
        ) noexcept
{
    static const char *const errorStrings[] = {
       "UNDEFINED",
       "SUCCESS",
       "PENDING",
       "TIMEOUT",
       "ARCHITECTURE_NOT_SUPPORTED",
       "AERON_INITIALIZATION_FAILED",
       "FORBIDDEN",
       "INVALID_RESOURCE_ID",
       "UNKNOWN_RESOURCE",
       "OTHER_FETCH_ERROR",
       "INVALID_CLUSTER_DESCRIPTION",
       "INVALID_SCHEMA_DESCRIPTION",
       "CLIENT_NOT_CONNECTED",
       "SCHEMA_UNAVAILABLE",
       "INVALID_APPLICATION_FOR_MODEL",
       "MISPLACED_CREATIVE_ID",
       "TOO_FEW_CREATIVES",
       "TOO_MANY_CREATIVES",
       "MISMATCHED_FEATURE_TYPE",
       "INVALID_FEEDBACK_LEVEL",
       "MTU_EXCEEDED",
       "BACK_PRESSURED",
       "NO_COMPATIBLE_NODE",
       "INVALID_PREDICTION_RESPONSE",
    };
    return errorStrings[static_cast<int32_t>(errorCode)];
}

/*
 * MAIN CONFIGURATION
 */

static aeron_driver_context_t *s_aeronDriverContext = nullptr;
static aeron_driver_t *s_aeronDriver = nullptr;
static bool s_initialized = false;
static std::string s_aeronDir = "/dev/shm/ubiquity-driver";

Ubiquity::~Ubiquity(void) {
    if (s_initialized) {
        aeron_driver_close(s_aeronDriver);
        aeron_driver_context_close(s_aeronDriverContext);
        s_aeronDriver = nullptr;
        s_aeronDriverContext = nullptr;
        ClusterClient::s_aeronClient = nullptr;
    }
}

static void onException(
        const std::exception &e
        ) noexcept
{
    std::cerr << "Aeron exception: " << e.what() << std::endl;
}

static inline bool isBigEndian(void) noexcept {
    union {
        uint32_t uint32;
        uint8_t uint8[4];
    } value = {1};
    return value.uint8[0] == 0;
}

static inline bool initializeAeron(void) noexcept {
    static bool s_aeronInitialized = false;
    if (s_aeronInitialized) {
        return true;
    }
    if (aeron_driver_context_init(&s_aeronDriverContext) < 0) {
        DBG("Ubiquity : Aeron driver context initialization failed");
        return false;
    }
    aeron_driver_context_set_agent_on_start_function(s_aeronDriverContext, aeron_set_thread_affinity_on_start, s_aeronDriverContext);
    aeron_driver_context_set_dir(s_aeronDriverContext, s_aeronDir.c_str());
    aeron_driver_context_set_dir_delete_on_shutdown(s_aeronDriverContext, true);
    aeron_driver_context_set_dir_delete_on_start(s_aeronDriverContext, true);
    aeron_driver_context_set_mtu_length(s_aeronDriverContext, 1280);
    aeron_driver_context_set_perform_storage_checks(s_aeronDriverContext, true);
#ifndef NDEBUG
    aeron_driver_context_set_print_configuration(s_aeronDriverContext, true);
#endif
     aeron_driver_context_set_receiver_idle_strategy(s_aeronDriverContext, "backoff");
    aeron_driver_context_set_rejoin_stream(s_aeronDriverContext, true);
    aeron_driver_context_set_reliable_stream(s_aeronDriverContext, true);
     aeron_driver_context_set_sender_idle_strategy(s_aeronDriverContext, "backoff");
    aeron_driver_context_set_shared_idle_strategy(s_aeronDriverContext, "backoff");
    aeron_driver_context_set_threading_mode(s_aeronDriverContext, AERON_THREADING_MODE_SHARED);
    aeron_driver_context_set_term_buffer_sparse_file(s_aeronDriverContext, false);
    aeron_driver_context_set_term_buffer_length(s_aeronDriverContext, 65536);
    if(aeron_driver_init(&s_aeronDriver, s_aeronDriverContext) < 0) {
        aeron_driver_context_close(s_aeronDriverContext);
        DBG("Ubiquity : Aeron driver initialization failed");
        return false;
    }
    if(aeron_driver_start(s_aeronDriver, false) < 0) {
        aeron_driver_context_close(s_aeronDriverContext);
        aeron_driver_close(s_aeronDriver);
        DBG("Ubiquity : Aeron driver start failed");
        return false;
    }
    ClusterClient::s_aeronClient = std::make_unique<aeron::Aeron>(
            aeron::Context()
               .errorHandler(onException)
               .aeronDir(s_aeronDir)
               .preTouchMappedMemory(true)
    );
    s_aeronInitialized = true;
    return true;
}

void Ubiquity::setCredential(
        const char *tenantId
        ) noexcept
{
    ClusterClient::s_tenantId = std::string(tenantId);
}

static inline Ubiquity::ErrorCode::Type initialize(void) noexcept
{
    if (s_initialized) {
        return Ubiquity::ErrorCode::SUCCESS;
    }
    DBG("Ubiquity : Global initialization");

    if (isBigEndian()) {
        DBG("Ubiquity : Architecture not supported");
        return Ubiquity::ErrorCode::ARCHITECTURE_NOT_SUPPORTED;
    }

    if (!initializeAeron()) {
        return Ubiquity::ErrorCode::AERON_INITIALIZATION_FAILED;
    }

    Ubiquity::ErrorCode::Type errorCode = ClusterClient::ensureSchemaForAllModelsAreInCache();
    if (Ubiquity::ErrorCode::SUCCESS != errorCode) {
        DBG("Ubiquity : Error (" << Ubiquity::getErrorString(errorCode) << ") during initial schema fetching");
        return errorCode;
    }

    NodeClient::compileLookupTables();

    s_initialized = true;
    DBG("Ubiquity : Successful initialization");
    return Ubiquity::ErrorCode::SUCCESS;
}

void Ubiquity::setAeronDirectory(
        const char *directoryPath
        ) noexcept
{
    s_aeronDir = directoryPath;
}

void Ubiquity::setApiAddress(
        const char *address
        ) noexcept
{
    ClusterClient::s_apiUrlRoot = std::string("https://") + address + "/v1";
}

void Ubiquity::setResourceStringInCache(
        const char *const id,
        const char *const resourceString
        ) noexcept
{
    DBG("setConfigInCache " << id);
    ClusterClient::s_resourceCache[std::string(id)] = std::string(resourceString);
}

/*
 * HANDLES AND CONNECTIONS
 */

static std::mutex s_clusterClientsMutex;
static std::unordered_map<std::string, std::weak_ptr<ClusterClient>> s_clusterClients;

static inline std::shared_ptr<ClusterClient> getClusterClient(
        const char *const clusterId,
        const bool startThread
        ) noexcept
{
    std::lock_guard<std::mutex> guard(s_clusterClientsMutex);

    const std::string &clusterIdString = std::string(clusterId);
    auto &weakPtrRef = s_clusterClients[clusterIdString];
    auto clusterClient = weakPtrRef.lock();
    if (!clusterClient || clusterClient->canTerminate) {
        DBG("Ubiquity : Creating new ClusterClient(" << clusterIdString << ")");
        clusterClient = std::make_shared<ClusterClient>(clusterIdString);
        weakPtrRef = clusterClient;
        if (startThread) {
            std::thread(&ClusterClient::clusterThreadFunction, clusterClient).detach();
        }
    } else {
        DBG("Ubiquity : Returning handle to existing ClusterClient(" << clusterIdString << ")");
    }
    return clusterClient;
}

Ubiquity::ClientHandle Ubiquity::getClientHandleById(
        const char *const clusterId
        ) noexcept
{
    auto clientHandleImpl = std::make_shared<Ubiquity::ClientHandle::Impl>();
    clientHandleImpl->clusterClient = getClusterClient(clusterId, false);
    if (Ubiquity::ErrorCode::PENDING == std::atomic_load(&clientHandleImpl->clusterClient->errorCode)) {
        clientHandleImpl->errorCode = Ubiquity::ErrorCode::CLIENT_NOT_CONNECTED;
    }
    return Ubiquity::ClientHandle(std::move(clientHandleImpl));
}

static std::string lastClusterId;

Ubiquity::ClientHandle Ubiquity::getClientHandle(void) noexcept
{
    return getClientHandleById(lastClusterId.c_str());
}

Ubiquity::ErrorCode::Type Ubiquity::openClient(
        const char *const clusterId
        ) noexcept
{
    lastClusterId = clusterId;
    Ubiquity::ErrorCode::Type errorCode = initialize();
    if (Ubiquity::ErrorCode::SUCCESS != errorCode) {
        return errorCode;
    }
    auto clusterClient = getClusterClient(clusterId, true);
    while (Ubiquity::ErrorCode::PENDING == std::atomic_load(&clusterClient->errorCode)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::atomic_load(&clusterClient->errorCode);
}

void Ubiquity::closeClient(
        const char *const clusterId
        ) noexcept
{
    std::lock_guard<std::mutex> guard(s_clusterClientsMutex);

    const std::string &clusterIdString = std::string(clusterId);
    auto &weakPtrRef = s_clusterClients[clusterIdString];
    auto clusterClient = weakPtrRef.lock();
    if (clusterClient) {
        DBG("Ubiquity : Autorizing ClusterClient(" << clusterClient->clusterId << ") to terminate.");
        std::atomic_store(&clusterClient->canTerminate, true);
    }
}

int64_t Ubiquity::xxh3_64bits(
        const char *source
        ) noexcept
{
    return XXH3_64bits(source, strlen(source));
}
