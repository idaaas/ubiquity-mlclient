#pragma once

#include <stdint.h>
#include <stddef.h>

#include <cstdlib>
#include <stdio.h>
#include <strings.h>
#include <unordered_set>
#include "constants.hpp"

#define CLUSTER_ID_MAX_LENGTH 0x100

#define SERIALIZATION_VERSION 2

typedef int32_t stream_id_t;

#define STREAM_ID_SYNC 1
#define STREAM_ID_PREDICTION 2
#define STREAM_ID_FEEDBACK 3

typedef int32_t status_code_t;
typedef int16_t application_type_t;

static constexpr uint64_t MAXIMUM_TRANSMISSION_UNIT = 1280;
static constexpr uint64_t MAXIMUM_MESSAGE_SIZE = 0x8000;

static constexpr size_t MAXIMUM_MODEL_COUNT = 0x10000ul;
static constexpr size_t MAXIMUM_MODEL_REPLICA_COUNT = 0x80000ul;

static constexpr size_t MODEL_MAP_CAPACITY = 2 * MAXIMUM_MODEL_COUNT; // Twice the maximum model count to ensure load factor below 0.5.
static constexpr size_t MODEL_MAP_MASK = (MODEL_MAP_CAPACITY - 1);

static constexpr size_t MAXIMUM_NODE_COUNT = 256; // /!\ Must be a power of 2.

#define NODE_CLIENT_MAP_SIZE (MAXIMUM_NODE_COUNT * 2)

static constexpr size_t NODE_COUNT_MASK = MAXIMUM_NODE_COUNT - 1;
static constexpr size_t COMPATIBLE_NODE_MAP_SIZE = MAXIMUM_NODE_COUNT / 64;

static constexpr size_t TOPOLOGY_BLOCK_COUNT = 2;
static constexpr size_t MODEL_METADATA_BLOCK_COUNT = 512;
static constexpr size_t MODEL_SHARDING_BLOCK_COUNT = MAXIMUM_NODE_COUNT;
static constexpr size_t MAXIMUM_MODEL_COUNT_PER_METADATA_BLOCK = 128;
static constexpr size_t MAXIMUM_MODEL_COUNT_PER_SHARDING_BLOCK = MAXIMUM_MODEL_REPLICA_COUNT / MAXIMUM_NODE_COUNT;

static constexpr uint16_t BLOCK_TYPE_TOPOLOGY = 0;
static constexpr uint16_t BLOCK_TYPE_MODEL_METADATA = 1;
static constexpr uint16_t BLOCK_TYPE_MODEL_SHARDING = 2;
static constexpr uint16_t BLOCK_TYPE_COUNT = 3;

typedef uint32_t client_session_id_t;

typedef uint32_t ip4_t;

typedef std::unordered_set<ip4_t> ip_set_t;

typedef float node_weight_t;

struct cluster_info_t {
    ip4_t ip_list[MAXIMUM_NODE_COUNT];
    size_t ip_count;
};

typedef struct md5_hash_s {
    uint64_t chunks[2];
} md5_hash_t;

typedef md5_hash_t root_hash_t;
typedef md5_hash_t block_hash_t;

namespace output_t {
    enum value_t : uint32_t {
        REGISTRATION_REQUEST = 0x1,
        DISCONNECTION_REQUEST = 0x5,
        HEARTBEAT = 0x6,
        PRED_OPEN_REQUEST = 0x7,
        SERVER_READY_REQUEST = 0x8,
        COUNT,
    };
    static const char *const strings[COUNT] = {
        "UNDEFINED",
        "REGISTRATION_REQUEST",
        "UNDEFINED",
        "UNDEFINED",
        "UNDEFINED",
        "DISCONNECTION_REQUEST",
        "HEARTBEAT",
        "PRED_OPEN_REQUEST",
        "SERVER_READY_REQUEST",
    };
}

struct message_common_t {
    uint8_t version;
    uint8_t type;
    uint16_t reserved;
} __attribute__((packed));

static inline const char *ubiquity_get_status_string(
        status_code_t status_code
        ) noexcept
{
    static const char *const status_strings[] = {
        "UNDEFINED",
        "SUCCESS",
        "NO_DATA",
        "BACK_PRESSURED",
        "NO_COMPATIBLE_NODE",

        "ARCHITECTURE_NOT_SUPPORTED",
        "AERON_INITIALIZATION_FAILED",

        "INVALID_RESOURCE_ID",
        "INVALID_APP_FOR_MODEL",
        "MISMATCHED_FEATURE_TYPE",
        "INVALID_FEEDBACK_LEVEL",
        "MISSING_CREATIVES",
        "TOO_MANY_CREATIVES",
        "MISPLACED_CREATIVE_ID",

        "NO_API_RESPONSE",
        "FORBIDDEN",
        "UNKNOWN_RESOURCE",
        "OTHER_FETCH_ERROR",
        "INVALID_CLUSTER_DESCRIPTION",
        "NO_METADATA",
        "INVALID_HANDLE",
        "UNAUTHORIZED",
        "INTERNAL_API_ERROR",
        "NO_MEMORY",
        "CONNECTION_TIMEOUT",
    };
    return status_code <= STATUS_CODE_CONNECTION_TIMEOUT
        ? status_strings[status_code]
        : "UNDEFINED";
}
