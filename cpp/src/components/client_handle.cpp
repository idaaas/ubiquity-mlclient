#include "client_handle.hpp"

#include "ubiquity.hpp"
#include "cluster_client.hpp"
#include "shared_state_store.hpp"
#include "idle_strategy.hpp"
#include "load_balancer.hpp"
#include "common/common.hpp"
#include "common/model_info.hpp"
#include "utils/prng.hpp"
#include "utils/logger.hpp"
#include "utils/Base32C.hpp"

#include <xxh3.h>
#include <sstream>

#define MESSAGE_TYPE_QUERY 0x05
#define MESSAGE_TYPE_PREDICTION 0x06
#define MESSAGE_TYPE_FEEDBACK 0x07
#define MESSAGE_TYPE_CANCEL 0x08

static constexpr bool app_uses_creatives[APPLICATION_TYPE_COUNT] = {
    false, // bid shading
    false, // probability
    true, // line item
    true, // action rate
};

typedef struct cluster_client_s cluster_client_t;

namespace handle_state_t {
    enum value_t : uint8_t {
        READY = 0,
        PREDICTING,
        PREDICT_SUCCEEDED,
        POLLED,
        TRAINED,
        CANCELED,
    };
    static constexpr const char *origin_function_name[]{
        nullptr,
        nullptr,
        nullptr,
        "poll",
        "train",
        "cancel",
    };
}

namespace error_origin_t {
    enum value_t : uint8_t {
        PREDICT = 0,
        POLL,
        TRAIN,
        CANCEL,
    };
    static constexpr const char *function_name[]{
        "predict",
        "poll",
        "train",
        "cancel",
    };
}

typedef struct message_application_s { 
    message_common_t _;
    uint16_t application_id;
    uint16_t feedback_level_or_feature_list_length;
    uint64_t model_id;
    int64_t query_id;
} __attribute__((packed)) message_application_t;

typedef struct message_prediction_bid_shading_s { 
    message_application_t _;
    float price;
    float probability;
} __attribute__((packed)) message_prediction_bid_shading_t;

typedef struct message_prediction_probability_s { 
    message_application_t _;
    float probability;
} __attribute__((packed)) message_prediction_probability_t;

typedef struct message_prediction_line_item_s { 
    message_application_t _;
    int64_t creative_id;
    float price;
    float probability;
} __attribute__((packed)) message_prediction_line_item_t;

typedef struct message_prediction_action_rate_s { 
    message_application_t _;
    int64_t creative_id;
    float price;
} __attribute__((packed)) message_prediction_action_rate_t;

typedef struct message_query_s { 
    message_application_t _;
    uint32_t feedback_delay;
} __attribute__((packed)) message_query_t;

typedef struct message_query_bid_shading_s { 
    message_query_t _;
    float bid_floor;
    float bid_price;
    int32_t optimization_mode;
} __attribute__((packed)) message_query_bid_shading_t;

typedef message_query_t message_query_probability_t;

typedef struct message_query_with_creatives_s { 
    message_query_t _;
    int32_t num_creative;
} __attribute__((packed)) message_query_with_creatives_t;

typedef struct message_query_line_item_s { 
    message_query_with_creatives_t _;
    float max_cpm;
    float bid_floor;
    int32_t exposure_count;
} __attribute__((packed)) message_query_line_item_t;

typedef struct message_query_action_rate_s { 
    message_query_with_creatives_t _;
    float bid_price;
    float bid_floor;
    int32_t exposure_count;
} __attribute__((packed)) message_query_action_rate_t;

typedef struct message_feedback_s { 
    message_application_t _;
    uint32_t feedback_delay;
    uint32_t next_feedback_delay;
    uint32_t event_occured;
} __attribute__((packed)) message_feedback_t;

typedef struct message_feedback_first_level_s { 
    message_feedback_t _;
    uint32_t reserved;
    double weight;
} __attribute__((packed)) message_feedback_first_level_t;

typedef struct message_cancel_s { 
  message_application_t _;
  uint32_t feedback_delay;
} __attribute__((packed)) message_cancel_t;

typedef union message_buffer_u {
    uint8_t buffer[MAXIMUM_MESSAGE_SIZE];
    message_common_t common;
    message_application_t application;
    message_prediction_bid_shading_t bid_shading_prediction;
    message_prediction_probability_t probability_prediction;
    message_prediction_line_item_t line_item_prediction;
    message_prediction_action_rate_t action_rate_prediction;
    message_query_t query;
    message_query_with_creatives_t creative_query;
    message_query_bid_shading_t bid_shading_query;
    message_query_line_item_t line_item_query;
    message_query_action_rate_t action_rate_query;
    message_feedback_t feedback;
    message_feedback_first_level_t feedback_first_level;
    message_cancel_t cancel;
} message_buffer_t;

#define CREATIVE_RESERVOIR_SIZE 0x80
#define CREATIVE_COUNT_MASK (CREATIVE_RESERVOIR_SIZE - 1)
#define CREATIVE_SLOTS_COUNT 18

typedef struct client_handle_s {
    status_code_t status_code = STATUS_CODE_SUCCESS;
    handle_state_t::value_t state = handle_state_t::READY;
    error_origin_t::value_t error_origin;
    std::string diagnostic_string;
    char cluster_id[CLUSTER_ID_MAX_LENGTH];

    ubiquity_prng_t prng;
    ubiquity_logger_t logger;
    uint64_t id;
    size_t cycle_progress;
    int64_t creative_reservoir[CREATIVE_RESERVOIR_SIZE];
    feature_type_list_t feature_type_list;
    char encoded_model_id[14];
    uint16_t application_id = APPLICATION_TYPE_UNDEFINED;
    int32_t num_creative = 0;
    size_t feature_ordinal = 0;
    size_t offset = 0;

    model_info_ref_t cached_model_info;
    model_info_t empty_model_info;

    cluster_client_t *cluster_client;
    std::shared_ptr<shared_state_store_t> store;

    message_buffer_t message;
    backoff_idle_strategy_t idle_strategy;
} client_handle_t;

/*
 * HANDLE CREATION AND RELEASE
 */

static client_handle_t *s_invalid_handle;

static inline client_handle_t *client_handle_get_invalid_handle(void) {
    if (nullptr == s_invalid_handle) {
        s_invalid_handle = new (std::nothrow) client_handle_t{};
        s_invalid_handle->status_code = STATUS_CODE_INVALID_HANDLE;
    }
    return s_invalid_handle;
}

client_handle_t *client_handle_create(
        cluster_client_t *const cluster_client,
        std::shared_ptr<shared_state_store_t> store,
        const char *const cluster_id
        ) noexcept
{
    if (nullptr == cluster_client) {
        return client_handle_get_invalid_handle();
    }
    client_handle_t *client_handle = new (std::nothrow) client_handle_t{};

    // Copy arguments.
    client_handle->cluster_client = cluster_client;
    client_handle->store = store;
    strcpy(
            client_handle->cluster_id,
            cluster_id
            );

    // Initialize logger and pseudo random generator..
    client_handle->logger = ubiquity_logger_create(false);
    ubiquity_prng_seed(&client_handle->prng);

    // Each handle has its own random id.
    client_handle->id = ubiquity_prng_generate(&client_handle->prng);

    // Empty model info to use when no info matches a model id.
    model_info_initialize(
            &client_handle->empty_model_info,
            0
            );

    // One more handle uses the cluster client.
    cluster_client_increment_use_count(cluster_client);

    LOG_INFO_2(client_handle->logger, "event=handle_create handle={:x} status={}",
            client_handle->id,
            ubiquity_get_status_string(client_handle->status_code)
            );

    client_handle->status_code = STATUS_CODE_SUCCESS;

    // initialize the backoff idle strategy
    backoff_idle_strategy_init(&client_handle->idle_strategy);

    return client_handle;
}

void client_handle_set_log_level(
        client_handle_t *const client_handle,
        const int32_t level
        ) noexcept
{
    ubiquity_logger_set_level(client_handle->logger, level);
}

client_handle_t *client_handle_delete(
        client_handle_t *const client_handle
        ) noexcept
{
    client_handle_get_invalid_handle();
    if (s_invalid_handle != client_handle) {
        LOG_INFO_1(client_handle->logger, "event=handle_delete handle={:x}",
                client_handle->id
                );
        cluster_client_decrement_use_count(client_handle->cluster_client);
        ubiquity_logger_delete(client_handle->logger);
        delete client_handle;
    }
    return s_invalid_handle;
}

/*
 * PREDICTION QUERY STARTER FUNCTIONS
 */

static const size_t feature_list_offset_by_application_type[] = {
    sizeof(message_query_bid_shading_t),
    sizeof(message_query_probability_t),
    sizeof(message_query_line_item_t),
    sizeof(message_query_action_rate_t),
};


model_info_t *client_handle_get_model_info(
        client_handle_t *const client_handle
        ) noexcept 
{
    return client_handle->cached_model_info == nullptr
    ? &client_handle->empty_model_info
    : client_handle->cached_model_info.get();
}

static inline void client_handle_prepare_prediction_query(
        client_handle_t *const client_handle,
        const uint16_t application_id
        ) noexcept
{
    client_handle->application_id = application_id;

    client_handle->num_creative = 0;
    feature_type_list_reset(&client_handle->feature_type_list);
    client_handle->offset = feature_list_offset_by_application_type[(size_t)application_id];
    client_handle->feature_ordinal = 0;
    client_handle->state = handle_state_t::PREDICTING;
}

void client_handle_start_bid_shading(
        client_handle_t *const client_handle,
        const float bid_price,
        const float bid_floor,
        const int32_t optimization_mode
        ) noexcept
{
    client_handle_prepare_prediction_query(
            client_handle,
            APPLICATION_TYPE_BID_SHADING
            );
    client_handle->message.bid_shading_query.bid_floor = bid_floor;
    client_handle->message.bid_shading_query.bid_price = bid_price;
    client_handle->message.bid_shading_query.optimization_mode = optimization_mode;
    LOG_DEBUG_4(client_handle->logger, " event=start_query handle={:x} application=bid_shading bid_price={} bid_floor={} optimization_mode={}",
            client_handle->id,
            bid_price,
            bid_floor,
            optimization_mode
            );
}

void client_handle_start_probability(
        client_handle_t *const client_handle
        ) noexcept
{
    client_handle_prepare_prediction_query(
            client_handle,
            APPLICATION_TYPE_PROBABILITY
            );
    LOG_DEBUG_1(client_handle->logger, "event=start_query handle={:x} application=probability",
            client_handle->id
            );
}

void client_handle_start_line_item(
        client_handle_t *const client_handle,
        const float max_cpm,
        const float bid_floor,
        const int32_t exposure_count
        ) noexcept
{
    client_handle_prepare_prediction_query(
            client_handle,
            APPLICATION_TYPE_LINE_ITEM
            );
    client_handle->message.line_item_query.max_cpm = max_cpm;
    client_handle->message.line_item_query.bid_floor = bid_floor;
    client_handle->message.line_item_query.exposure_count = exposure_count;
    LOG_DEBUG_4(client_handle->logger, "event=start_query handle={:x} application=line_item max_cpm={} bid_floor={} exposure_count={}",
            client_handle->id,
            max_cpm,
            bid_floor,
            exposure_count
            );
}

void client_handle_start_action_rate(
        client_handle_t *const client_handle,
        const float bid_price,
        const float bid_floor,
        const int32_t exposure_count
        ) noexcept
{
    client_handle_prepare_prediction_query(
            client_handle,
            APPLICATION_TYPE_ACTION_RATE
            );
    client_handle->message.action_rate_query.bid_price = bid_price;
    client_handle->message.action_rate_query.bid_floor = bid_floor;
    client_handle->message.action_rate_query.exposure_count = exposure_count;
    LOG_DEBUG_4(client_handle->logger, "event=start_query handle={:x} application=action_rate bid_price={} bid_floor={} exposure_count={}",
            client_handle->id,
            bid_price,
            bid_floor,
            exposure_count
            );
}

/*
 * PREDICTION QUERY AND FEEDBACK
 */

static inline void client_handle_prepare_common_header(
        client_handle_t *const client_handle,
        const char *const encoded_model_id,
        const int64_t query_id,
        const uint8_t message_type
        ) noexcept
{
    base_32C_decode(
            &client_handle->message.application.model_id,
            encoded_model_id
            );
    const uint64_t model_id = client_handle->message.application.model_id;
    // IMPORTANT: since we use copy-on-write, the cached value of model_info needs to be refreshed each time!!!
    client_handle->cached_model_info = store_get_model_info(
        client_handle->store.get(),
        model_id
        );
    model_info_t* model_info = client_handle_get_model_info(client_handle);
    client_handle->message.common.type = static_cast<uint8_t>(message_type);
    client_handle->message.application.application_id = model_info_get_application_id(model_info);
    client_handle->message.application.query_id = query_id ^ model_id;
}


static inline status_code_t client_handle_check_predict_setup_errors(
        client_handle_t *const client_handle
        ) noexcept
{
    static constexpr const uint8_t status_code_per_error_level[64] = {
        (uint8_t)STATUS_CODE_SUCCESS, // 0
        (uint8_t)STATUS_CODE_MISMATCHED_FEATURE_TYPE, 0, // 1
        (uint8_t)STATUS_CODE_MISSING_CREATIVES, 0, 0, 0, // 3
        (uint8_t)STATUS_CODE_MISPLACED_CREATIVE_ID, 0, 0, 0, 0, 0, 0, 0, // 7
        (uint8_t)STATUS_CODE_NO_MODEL_METADATA, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 15
        (uint8_t)STATUS_CODE_INVALID_RESOURCE_ID, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 31
        (uint8_t)STATUS_CODE_INVALID_APP_FOR_MODEL, // 63
    };
    model_info_t *model_info = client_handle_get_model_info(client_handle);
    uint8_t error_level;
    if (!model_info_has_metadata(model_info)) {
        error_level = 15;
    } else {
        error_level =
            (handle_state_t::PREDICTING != client_handle->state) * 63
            | (0 == client_handle->message.application.model_id) * 31
            | (((0 != model_info_get_model_id(model_info)) & (client_handle->application_id != model_info_get_application_id(model_info))) | (client_handle->application_id >= APPLICATION_TYPE_COUNT)) * 63
            | ((0 < client_handle->num_creative) & !app_uses_creatives[client_handle->message.application.application_id]) * 7
            | ((0 == client_handle->num_creative) & app_uses_creatives[client_handle->message.application.application_id]) * 3
            | !model_info_feature_type_lists_are_identical(
                        model_info,
                        &client_handle->feature_type_list
                        );
    }
    client_handle->status_code = static_cast<status_code_t>(status_code_per_error_level[error_level]);
    return client_handle->status_code;
}

status_code_t client_handle_predict(
        client_handle_t *const client_handle,
        const char *const encoded_model_id,
        const int64_t query_id
        ) noexcept
{
    // TODO : should the copy be here or in common header preparation ?
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstringop-truncation"
    strncpy(client_handle->encoded_model_id, encoded_model_id, 14);
    #pragma GCC diagnostic pop

    client_handle_prepare_common_header(
            client_handle,
            encoded_model_id,
            query_id,
            MESSAGE_TYPE_QUERY
            );

    model_info_t *model_info = client_handle_get_model_info(client_handle);
    client_handle->message.common.version = SERIALIZATION_VERSION;
    client_handle->message.application.feedback_level_or_feature_list_length = client_handle->offset
        - feature_list_offset_by_application_type[(size_t)client_handle->application_id];
    client_handle->message.query.feedback_delay = model_info_get_feedback_delay(
            model_info,
            0
            );
    client_handle->error_origin = error_origin_t::PREDICT;
    if (STATUS_CODE_SUCCESS != client_handle_check_predict_setup_errors(client_handle)) {
        LOG_ERROR_5(client_handle->logger, "event=predict handle={:x} model_id={} query_id={} internal_query_id={} status={}",
                client_handle->id,
                encoded_model_id,
                query_id,
                0 + client_handle->message.application.query_id,
                ubiquity_get_status_string(client_handle->status_code)
                );
        return client_handle->status_code;
    }

    // If creatives were put, they must be appened to the message.
    if (0 < client_handle->num_creative) {
        client_handle->message.creative_query.num_creative = client_handle->num_creative;
        client_handle->offset += (~client_handle->offset + 1) & 7;
        const int32_t selection_size = CREATIVE_SLOTS_COUNT < client_handle->num_creative
            ? CREATIVE_SLOTS_COUNT
            : client_handle->num_creative;
        for (int32_t cursor = 0; cursor < selection_size; ++cursor) {
            const uint64_t selected = ubiquity_prng_generate(&client_handle->prng) % client_handle->num_creative;
            *((uint64_t *)(client_handle->message.buffer + client_handle->offset) + cursor) = client_handle->creative_reservoir[selected];
            client_handle->creative_reservoir[selected] = client_handle->creative_reservoir[--client_handle->num_creative];
        }
        client_handle->offset += selection_size * sizeof(uint64_t);
    }

    client_handle->status_code = shared_send_message_with_rendezvous(
            client_handle->store.get(),
            &model_info->compatible_node_set,
            client_handle->message.application.query_id,    // Use query id as a balance key.
            STREAM_ID_PREDICTION,
            client_handle->message.buffer,
            client_handle->offset
            );
    if (STATUS_CODE_SUCCESS == client_handle->status_code) {
            LOG_DEBUG_5(client_handle->logger, "event=predict handle={:x} model_id={} query_id={} internal_query_id={} status={}",
            client_handle->id,
            encoded_model_id,
            query_id,
            0 + client_handle->message.application.query_id,
            ubiquity_get_status_string(client_handle->status_code)
            );
        client_handle->state = handle_state_t::PREDICT_SUCCEEDED;
        client_handle->application_id = APPLICATION_TYPE_UNDEFINED;
    }
    return client_handle->status_code;
}

static inline status_code_t client_handle_check_feedback_setup_errors(
        client_handle_t *const client_handle
        ) noexcept
{
    static constexpr const uint8_t status_code_per_error_level[8] = {
        (uint8_t)STATUS_CODE_SUCCESS, // 0
        (uint8_t)STATUS_CODE_INVALID_FEEDBACK_LEVEL, 0, // 1
        (uint8_t)STATUS_CODE_NO_MODEL_METADATA, 0, 0, 0, // 3
        (uint8_t)STATUS_CODE_INVALID_RESOURCE_ID, // 7
    };
    model_info_t *model_info = client_handle_get_model_info(client_handle);
    uint8_t error_level =
        (0 == client_handle->message.application.model_id) * 7
        | (client_handle->message.application.feedback_level_or_feature_list_length >= model_info_get_feedback_count(model_info));
    error_level |= 3 * !model_info_has_metadata(model_info);
    client_handle->status_code = static_cast<status_code_t>(status_code_per_error_level[error_level]);
    return client_handle->status_code;
}

static inline status_code_t client_handle_train_impl(
        client_handle_t *const client_handle,
        const char *encoded_model_id,
        int64_t query_id,
        const char* const feedback_name,
        const uint16_t feedback_level
        ) noexcept
{
    client_handle_prepare_common_header(
            client_handle,
            encoded_model_id,
            query_id,
            MESSAGE_TYPE_FEEDBACK
            );
    model_info_t *model_info = client_handle_get_model_info(client_handle);
    client_handle->message.common.version = SERIALIZATION_VERSION;
    size_t level = feedback_level;
    if (MAXIMUM_FEEDBACK_LEVEL == level) {
        level = model_info_find_feedback_level(
                model_info,
                feedback_name
                );
    }
    client_handle->message.application.feedback_level_or_feature_list_length = level;
    client_handle->state = handle_state_t::TRAINED;
    client_handle->error_origin = error_origin_t::TRAIN;
    client_handle->message.feedback.event_occured = 1;
    if (0 == level) {
        client_handle->message.feedback_first_level.weight = 1.0;
    }

    // Check if there were no error in setting up the feedback message.
    if (STATUS_CODE_SUCCESS != client_handle_check_feedback_setup_errors(client_handle)) {
        LOG_ERROR_5(client_handle->logger, "event=train handle={:x} model_id={} query_id={} internal_query_id={} status={}",
                client_handle->id,
                encoded_model_id,
                query_id,
                0 + client_handle->message.application.query_id,
                ubiquity_get_status_string(client_handle->status_code)
                );
        return client_handle->status_code;
    }

    // Loading feedback delays. Must only happens after checking setup errors to avoid invalid feedback index !
    client_handle->message.feedback.feedback_delay = model_info_get_feedback_delay(
            model_info,
            level
            );
    client_handle->message.feedback.next_feedback_delay = model_info_get_feedback_delay(
            model_info,
            level + 1
            );

    // Send the feedback message.
    client_handle->status_code = shared_send_message_with_rendezvous(
            client_handle->store.get(),
            &model_info->compatible_node_set,
            client_handle->message.application.query_id,    // Use query id as a balance key.
            STREAM_ID_FEEDBACK,
            client_handle->message.buffer,
            0 == level
                ? sizeof(message_feedback_first_level_t)
                : sizeof(message_feedback_t)
            );
    LOG_DEBUG_5(client_handle->logger, "event=train handle={:x} model_id={} query_id={} internal_query_id={} status={}",
            client_handle->id,
            encoded_model_id,
            query_id,
            0 + client_handle->message.application.query_id,
            ubiquity_get_status_string(client_handle->status_code)
            );
    return client_handle->status_code;
}

status_code_t client_handle_train_with_level(
        client_handle_t *const client_handle,
        const char *encoded_model_id,
        int64_t query_id,
        const uint16_t feedback_level
        ) noexcept
{
    return client_handle_train_impl(
            client_handle,
            encoded_model_id,
            query_id,
            "",
            feedback_level
            );
}

status_code_t client_handle_train(
        client_handle_t *const client_handle,
        const char *encoded_model_id,
        int64_t query_id,
        const char* const feedback_name
        ) noexcept
{
    return client_handle_train_impl(
            client_handle,
            encoded_model_id,
            query_id,
            feedback_name,
            MAXIMUM_FEEDBACK_LEVEL
            );
}

status_code_t client_handle_cancel(
        client_handle_t *const client_handle,
        const char *encoded_model_id,
        int64_t query_id
        ) noexcept
{
    client_handle_prepare_common_header(
            client_handle,
            encoded_model_id,
            query_id,
            MESSAGE_TYPE_CANCEL
            );
    model_info_t *model_info = client_handle_get_model_info(client_handle);
    client_handle->message.common.version = SERIALIZATION_VERSION;
    client_handle->message.application.feedback_level_or_feature_list_length = 0;
    client_handle->message.cancel.feedback_delay = model_info_get_feedback_delay(
            model_info,
            0
            );
    client_handle->state = handle_state_t::CANCELED;
    client_handle->error_origin = error_origin_t::CANCEL;
    if (STATUS_CODE_SUCCESS != client_handle_check_feedback_setup_errors(client_handle)) {
        LOG_ERROR_5(client_handle->logger, "event=cancel handle={:x} model_id={} query_id={}internal_query_id={} status={}",
                client_handle->id,
                encoded_model_id,
                query_id,
                0 + client_handle->message.application.query_id,
                ubiquity_get_status_string(client_handle->status_code)
                );
        return client_handle->status_code;
    }
    
    client_handle->status_code = shared_send_message_with_rendezvous(
            client_handle->store.get(),
            &model_info->compatible_node_set,
            client_handle->message.application.query_id,    // Use query id as a balance key.
            STREAM_ID_FEEDBACK,
            client_handle->message.buffer,
            sizeof(message_cancel_t)
            );
    LOG_DEBUG_5(client_handle->logger, "event=cancel handle={:x} model_id={} query_id={} internal_query_id={} status={}",
            client_handle->id,
            encoded_model_id,
            query_id,
            0 + client_handle->message.application.query_id,
            ubiquity_get_status_string(client_handle->status_code)
            );
    return client_handle->status_code;
}

/*
 * PREDICTION RESPONSE POLLING
 */

status_code_t client_handle_poll(
        client_handle_t *const client_handle
        ) noexcept
{
    static constexpr size_t expected_input_message_size[] {
        sizeof(message_prediction_bid_shading_t),
        sizeof(message_prediction_probability_t),
        sizeof(message_prediction_line_item_t),
        sizeof(message_prediction_action_rate_t),
    };

    client_handle->error_origin = error_origin_t::POLL;
    client_handle->state = handle_state_t::POLLED;

    client_handle->status_code = STATUS_CODE_NO_DATA;
    size_t message_length;
    if (0 != (message_length = poll_message(
                    client_handle->store.get(),
                    STREAM_ID_PREDICTION,
                    client_handle->message.buffer))) {
            const uint16_t application_id = static_cast<uint16_t>(client_handle->message.application.application_id);
            if (__glibc_unlikely(MESSAGE_TYPE_PREDICTION != client_handle->message.common.type
                        || application_id >= static_cast<uint16_t>(APPLICATION_TYPE_COUNT)
                        || expected_input_message_size[application_id] != message_length)) {
                LOG_WARNING_1(client_handle->logger, "event=poll handle={:x} status=INVALID_PREDICTION_RECEIVED",
                        client_handle->id
                        );
            } else {
                client_handle->status_code = STATUS_CODE_SUCCESS;
                LOG_DEBUG_4(client_handle->logger, "event=poll handle={:x} query_id={} internal_query_id={} status={}",
                        client_handle->id,
                        0 + (client_handle->message.application.model_id ^ client_handle->message.application.query_id), 
                        0 + client_handle->message.application.query_id,
                        ubiquity_get_status_string(client_handle->status_code)
                        );
            }
    }
    backoff_idle_strategy_set_work_count(&client_handle->idle_strategy, client_handle->status_code == STATUS_CODE_SUCCESS ? 1 : 0);
    return client_handle->status_code;
}

void client_handle_idle(
        client_handle_t *const client_handle
        ) noexcept
{
    backoff_idle_strategy_idle(&client_handle->idle_strategy);
}
/*
 * SETTERS
 */

static inline void put_feature_8(
        client_handle_t *const client_handle,
        const uint8_t value,
        const feature_type_t feature_type
        ) noexcept
{
    if (__glibc_likely(client_handle->feature_ordinal < 256 && client_handle->offset + sizeof(uint8_t) <= MAXIMUM_MESSAGE_SIZE)) {
        client_handle->message.buffer[client_handle->offset] = value;
        client_handle->offset += sizeof(uint8_t);
        feature_type_list_set_element(
                &client_handle->feature_type_list,
                client_handle->feature_ordinal,
                feature_type
                );
        client_handle->feature_ordinal++;
    }
}

static inline void put_feature_32(
        client_handle_t *const client_handle,
        const uint32_t value,
        const feature_type_t feature_type
        ) noexcept
{
    if (__glibc_likely(client_handle->feature_ordinal < 256 && client_handle->offset + 7 <= MAXIMUM_MESSAGE_SIZE)) {
        *(uint32_t *)(client_handle->message.buffer + client_handle->offset) = 0;
        client_handle->offset += (-client_handle->offset) & 3;
        if (__glibc_likely(client_handle->offset + sizeof(uint32_t) <= MAXIMUM_MESSAGE_SIZE)) {
            *(uint32_t *)(client_handle->message.buffer + client_handle->offset) = value;
            client_handle->offset += sizeof(uint32_t);
        }
        feature_type_list_set_element(
                &client_handle->feature_type_list,
                client_handle->feature_ordinal,
                feature_type
                );
        client_handle->feature_ordinal++;
    }
}

void client_handle_put_boolean(
        client_handle_t *const client_handle,
        const int8_t value
        ) noexcept
{
    return put_feature_8(
            client_handle,
            value,
            FEATURE_TYPE_BOOLEAN
            );
    LOG_DEBUG_2(client_handle->logger, "event=put_boolean handle={:x} value={}",
            client_handle->id,
            value
            );
}

void client_handle_put_categorical(
        client_handle_t *const client_handle,
        const int8_t value
        ) noexcept
{
    return put_feature_8(
            client_handle,
            value,
            FEATURE_TYPE_CATEGORICAL
            );
    LOG_DEBUG_2(client_handle->logger, "event=put_categorical handle={:x} value={}",
            client_handle->id,
            value
            );
}

void client_handle_put_nominal(
        client_handle_t *const client_handle,
        const char *raw_value
        ) noexcept
{
    const uint32_t value = XXH3_64bits(raw_value, strlen(raw_value));
    put_feature_32(
            client_handle,
            value,
            FEATURE_TYPE_NOMINAL
            );
    LOG_DEBUG_3(client_handle->logger, "event=put_nominal handle={:x} raw_value={} value={}",
            client_handle->id,
            raw_value,
            value
            );
}

void client_handle_put_numeric(
        client_handle_t *const client_handle,
        const float value
        ) noexcept
{
    const auto *const val = reinterpret_cast<const void*>(&value);
    put_feature_32(
            client_handle,
            *reinterpret_cast<const int32_t*>(val),
            FEATURE_TYPE_NUMERIC
            );
    LOG_DEBUG_2(client_handle->logger, "event=put_numeric handle={:x} value={}",
            client_handle->id,
            value
            );
}

void client_handle_put_creative(
        client_handle_t *const client_handle,
        const int64_t creative_id
        ) noexcept
{
    client_handle->creative_reservoir[client_handle->num_creative++ & CREATIVE_COUNT_MASK] = creative_id;
    LOG_DEBUG_2(client_handle->logger, "event=put_creative handle={:x} value={}",
            client_handle->id,
            creative_id
            );
}

/*
 * GETTERS
 */

status_code_t client_handle_get_status_code(
        client_handle_t *const client_handle
        ) noexcept
{
    return client_handle->status_code;
}

const char *client_handle_get_status_string(
        client_handle_t *const client_handle
        ) noexcept
{
    return ubiquity_get_status_string(client_handle->status_code);
}

const char *client_handle_get_model_id(
        client_handle_t *const client_handle
        ) noexcept
{
    base_32C_encode(
            client_handle->encoded_model_id,
            &client_handle->message.application.model_id
            );
    return client_handle->encoded_model_id;
}

int64_t client_handle_get_query_id(
        client_handle_t *const client_handle
        ) noexcept
{
    return client_handle->message.application.query_id ^ client_handle->message.application.model_id;
}

int64_t client_handle_get_creative_id(
        client_handle_t *const client_handle
        ) noexcept
{
    return client_handle->message.line_item_prediction.creative_id;
}

float client_handle_get_price(
        client_handle_t *const client_handle
        ) noexcept
{
    switch(client_handle->message.application.application_id) {
        case APPLICATION_TYPE_BID_SHADING:
            return client_handle->message.bid_shading_prediction.price;
        case APPLICATION_TYPE_LINE_ITEM:
            return client_handle->message.line_item_prediction.price;
        case APPLICATION_TYPE_ACTION_RATE:
            return client_handle->message.action_rate_prediction.price;
        case APPLICATION_TYPE_UNDEFINED:
            break;
    }
    return 0.f;
}

float client_handle_get_probability(
        client_handle_t *const client_handle
        ) noexcept
{
    switch(client_handle->message.application.application_id) {
        case APPLICATION_TYPE_BID_SHADING:
            return client_handle->message.bid_shading_prediction.probability;
        case APPLICATION_TYPE_LINE_ITEM:
            return client_handle->message.line_item_prediction.probability;
        case APPLICATION_TYPE_PROBABILITY:
            return client_handle->message.probability_prediction.probability;
        case APPLICATION_TYPE_ACTION_RATE:
            break;
        case APPLICATION_TYPE_UNDEFINED:
            break;
    }
    return 0.f;
}

int32_t client_handle_get_prediction_type(
        client_handle_t *const client_handle
        ) noexcept
{
    return static_cast<int32_t>(client_handle->message.application.application_id);
}

int8_t client_handle_get_bid_status(
        client_handle_t *const client_handle
        ) noexcept
{
  switch(client_handle->message.application.application_id) {
    case APPLICATION_TYPE_LINE_ITEM:
      return client_handle->message.line_item_prediction.price == 0 ? BID_STATUS_NO_BID : BID_STATUS_BID;
    case APPLICATION_TYPE_ACTION_RATE:
      return client_handle->message.action_rate_prediction.price == 0 ? BID_STATUS_NO_BID : BID_STATUS_BID;
    default:
      return BID_STATUS_BID;
  }
}

/*
 * DIAGNOSTIC
 */

const char *client_handle_get_diagnostic_string(
        client_handle_t *const client_handle
        ) noexcept
{
    static const char *application_type_strings[APPLICATION_TYPE_COUNT]{
        "BID_SHADING",
        "PROBABILITY",
        "LINE_ITEM",
        "ACTION_RATE",
    };
    static const char *starter_function_name[APPLICATION_TYPE_COUNT]{
        "start_bid_shading",
        "start_probability",
        "start_line_item",
        "start_action_rate",
    };
    static const char *feature_type_strings[]{
        "NUMERIC",
        "RESERVED_TYPE",
        "NOMINAL",
        "BOOLEAN",
        "CATEGORICAL",
    };

    uint8_t error_count = 0;
    std::stringstream diagnostic_string{};

    // TODO : check model_info for NULL
    model_info_t *model_info = client_handle_get_model_info(client_handle);
    if (handle_state_t::READY == client_handle->state && error_origin_t::PREDICT == client_handle->error_origin) {
        diagnostic_string << "\n\nError #" << std::to_string(++error_count) << ": "
            << "\nYou did not call any query starter function to prepare the last call to `predict`, for instance `start_line_item` or `start_probability`."
            << "\nStarter functions initialize buffers and states for the query and allow you to provide additional parameters which are specific to their kind of prediction queries.";
    } else if (0 == client_handle->message.application.model_id) {
        diagnostic_string << "\n\nError #" << std::to_string(++error_count) << ": "
            << "\nModel ID `" << client_handle->encoded_model_id << "` provided to `" << error_origin_t::function_name[client_handle->error_origin] << "` is ill-formed so it could not be decoded.";
    } else if (!model_info_has_metadata(model_info)) {
        diagnostic_string << "\n\nError #" << std::to_string(++error_count) << ": "
            << "\nThe schema corresponding to model " << client_handle->encoded_model_id << " has not been loaded yet.\nThis might be resolved after a few seconds (for the nodes to install the model if it's a new one and/or send the schema to your client),"
            << " unless you provided a well-formed model ID but it corresponds to no existing model."
            << "\nIn most cases, i.e. when you're certain the model IDs you provide correspond to existing models, you can simply ignore client_handle error or loop until it disappears.";
    } else if (error_origin_t::TRAIN == client_handle->error_origin || error_origin_t::CANCEL == client_handle->error_origin) {
        if (client_handle->message.application.feedback_level_or_feature_list_length >= model_info_get_feedback_count(model_info)) {
            diagnostic_string << "\n\nError #" << std::to_string(++error_count) << ": "
                << "The feedback level [" << client_handle->message.application.feedback_level_or_feature_list_length << "] you provided to `" << error_origin_t::function_name[client_handle->error_origin]
                << "` exceeds model " << client_handle->encoded_model_id << " 's maximum feedback level [" << model_info_get_feedback_count(model_info) << "].";
        }
    } else if (error_origin_t::PREDICT == client_handle->error_origin) {
        base_32C_encode(client_handle->encoded_model_id, &client_handle->message.application.model_id);

        if (handle_state_t::READY == client_handle->state) {
            diagnostic_string << "\n\nError #" << std::to_string(++error_count) << ": "
                << "\nYou did not call any query starter function to prepare the last call to `predict`."
                << "\nModel " << client_handle->encoded_model_id << " is a " << application_type_strings[model_info_get_application_id(model_info)] << " model, so `" << starter_function_name[model_info_get_application_id(model_info)]
                << "` must be called before any feature/creative putting methods and before `predict`"
                << "\nIt initializes buffers and states for the query and allows you to provide some parameters which are specific to " << application_type_strings[model_info_get_application_id(model_info)] << " prediction queries.";
        } else if (handle_state_t::PREDICT_SUCCEEDED == client_handle->state || APPLICATION_TYPE_UNDEFINED == client_handle->application_id) {
            diagnostic_string << "\n\nError #" << std::to_string(++error_count) << ": "
                << "\nYou called `predict` again after a successful previous call to `predict` without calling any query starter function first."
                << "\nWhen a call to `predict` is successful, the whole query parameters are consumed to avoid query duplication, so you need to initialize a whole new query and put new features/creatives into it."
                << "\nModel " << client_handle->encoded_model_id << " is a " << application_type_strings[model_info_get_application_id(model_info)] << " model, so you should call `" << starter_function_name[model_info_get_application_id(model_info)] << "` for that purpose.";
        } else if (handle_state_t::origin_function_name[client_handle->state]) {
            diagnostic_string << "\n\nError #" << std::to_string(++error_count) << ": "
                << "\nThough you *did* call `" << starter_function_name[client_handle->application_id] << "` first, you then called `" << handle_state_t::origin_function_name[client_handle->state] << "`"
                << " on the same client handle before you finally called `predict`; this_t call to `" << handle_state_t::origin_function_name[client_handle->state] << "` invalidated everything `" << starter_function_name[client_handle->application_id] << "` had initialized."
                << "\nAs a general rule, you must never call `predict`, `poll`, `train` and/or `cancel`"
                << " concurrently using the same client handle, though it's fine to call them sequentially."
                << "\nIf you need to call them concurrently, consider getting new client handles by calling `get_client_handle` for each concurrent usage."
                << "\n/!\\ beware_t that passing a client handle to another function or thread does *not* create a new client handle, only a shallow copy of the passed one sharing the same state !";
        } else if (model_info_get_application_id(model_info) != client_handle->application_id) {
            diagnostic_string << "\n\nError #" << std::to_string(++error_count) << ": "
                << "\nYou called `" << starter_function_name[client_handle->application_id] << "` to start preparing a prediction query for model " << client_handle->encoded_model_id
                << ", but client_handle model is of type " << application_type_strings[model_info_get_application_id(model_info)] << "."
                << "\nDid you mean to call `" << starter_function_name[model_info_get_application_id(model_info)] << "` instead ?";
        }
        if (0 < client_handle->num_creative && !app_uses_creatives[model_info_get_application_id(model_info)]) {
            diagnostic_string << "\n\nError #" << std::to_string(++error_count) << ": "
                << "\nCreatives have been put into a prediction query for model " << client_handle->encoded_model_id << " of type " << application_type_strings[model_info_get_application_id(model_info)] << " which have no use for creatives.";
        } else if (0 == client_handle->num_creative && app_uses_creatives[model_info_get_application_id(model_info)]) {
            diagnostic_string << "\n\nError #" << std::to_string(++error_count) << ": "
                << "\nNo creatives have been put into a prediction query for model " << client_handle->encoded_model_id << " of type " << application_type_strings[model_info_get_application_id(model_info)] << " which needs at least one creative to function.";
        }
        if (!model_info_feature_type_lists_are_identical(
                    model_info,
                    &client_handle->feature_type_list
                    )) {
            const size_t feature_type_count = model_info_get_feature_type_count(model_info);
            const uint8_t min_feature_ordinal = client_handle->feature_ordinal < feature_type_count ? client_handle->feature_ordinal : feature_type_count;
            const uint8_t max_feature_ordinal = client_handle->feature_ordinal > feature_type_count ? client_handle->feature_ordinal : feature_type_count;
            const bool has_less_features = client_handle->feature_ordinal < feature_type_count;
            for (uint8_t feature_ordinal = 0; feature_ordinal < max_feature_ordinal; ++feature_ordinal) {
                if (min_feature_ordinal == feature_ordinal) {
                    diagnostic_string << "\n\nError #" << std::to_string(++error_count) << ": " << (has_less_features ? "Less" : "More") << " features were put than expected by model " << client_handle->encoded_model_id << " :";
                } else if (0 == feature_ordinal) {
                    diagnostic_string << "\n\nError #" << std::to_string(++error_count) << ": " << "Some feature types in the query do not match the schema for model " << client_handle->encoded_model_id << " :";
                }

                const uint8_t actual_type = feature_type_list_get_element(
                        &client_handle->feature_type_list,
                        feature_ordinal
                        );
                const uint8_t expected_type = model_info_feature_type_list_get_element(
                        model_info,
                        feature_ordinal
                        );
                if (expected_type != actual_type) {
                    if (min_feature_ordinal > feature_ordinal) {
                        diagnostic_string << "\n    Feature #" << std::to_string(feature_ordinal) << " should be " << feature_type_strings[expected_type] << " instead of " << feature_type_strings[actual_type];
                    } else if (has_less_features) {
                        diagnostic_string << "\n    Missing feature #" << std::to_string(feature_ordinal) << " with expected type " << feature_type_strings[expected_type];
                    } else {
                        diagnostic_string << "\n    Extra feature #" << std::to_string(feature_ordinal) << " having type " << feature_type_strings[actual_type];
                    }
                }
            }
            if (0 == client_handle->feature_ordinal) {
                diagnostic_string << "\n\nIt seems like you did not put any feature at all. if_t you did, did you call `" << starter_function_name[model_info_get_application_id(model_info)] << "` *before* you put the features ?";
            }
        }
    }
    client_handle->diagnostic_string = "\n##### diagnostic_t for last call to `";
    client_handle->diagnostic_string += error_origin_t::function_name[client_handle->error_origin];
    client_handle->diagnostic_string += "` : ";
    client_handle->diagnostic_string += std::to_string(error_count);
    client_handle->diagnostic_string += " error(s) detected. #####";
    client_handle->diagnostic_string += diagnostic_string.str();
    client_handle->diagnostic_string += "\n\n##### end_t of error diagnostic #####\n";
    return client_handle->diagnostic_string.c_str();
}
