#include "components/client_handle.hpp"

#include "ubiquity_test.hpp"

#include "common/common.hpp"
#include "common/model_info.hpp"

#include "utils/Base32C.hpp"
#include "utils/prng.hpp"

#include "xxh3.h"

#include <string>

#define MESSAGE_TYPE_QUERY 0x05
#define MESSAGE_TYPE_PREDICTION 0x06
#define MESSAGE_TYPE_FEEDBACK 0x07
#define MESSAGE_TYPE_CANCEL 0x08

#define APPLICATION_TYPE_BID_SHADING 0
#define APPLICATION_TYPE_PROBABILITY 1
#define APPLICATION_TYPE_LINE_ITEM 2
#define APPLICATION_TYPE_ACTION_RATE 3

#define BUFFER_MESSAGE 0
#define BUFFER_FEATURES 2
#define BUFFER_CREATIVES 3
#define BUFFER_PREDICTION 4

/*
 * MOCKS
 */

/*
    Shared state store.
*/

typedef struct shared_state_store_s { 
} shared_state_store_t;

typedef struct std::shared_ptr<shared_state_store_s> shared_state_store_ref_t;
shared_state_store_ref_t s_store = std::shared_ptr<shared_state_store_t>(new shared_state_store_t());

typedef std::shared_ptr<model_info_t> model_info_ref_t;

model_info_ref_t s_model_info_ref = model_info_allocate();
model_info_t *s_model_info = s_model_info_ref.get();

model_info_ref_t store_get_model_info(
        shared_state_store_t *const store,
        uint64_t model_id
        ) noexcept
{
    ubiquity_test_emit("store_get_model_info",
            "model_id", model_id
            );
    return s_model_info_ref;
}


size_t poll_message(
        shared_state_store_t *const store,
        const stream_id_t stream_id,
        uint8_t *const buffer
        ) noexcept
{
    (void)stream_id;
    if (buffer_size(BUFFER_PREDICTION) > 0) {
        buffer_copy(BUFFER_PREDICTION, buffer);
        const size_t message_length = buffer_size(BUFFER_PREDICTION);
        buffer_clear(BUFFER_PREDICTION);
        return message_length;
    }
    return 0;
}

status_code_t shared_send_message_with_rendezvous(
        shared_state_store_t *const store,
        const ip_set_t *const compatible_node_set,
        const int64_t balance_key,
        const stream_id_t stream_id,
        uint8_t *const message_content,
        const size_t message_size
        ) noexcept
{
    ubiquity_test_emit("shared_send_message_with_rendezvous",
            "compatible_node_set", compatible_node_set,
            "balance_key", balance_key,
            "stream_id", stream_id,
            "message", message_size, message_content
            );
    return STATUS_CODE_SUCCESS;
}


/*
    Cluster client.
*/

const char *s_cluster_id = "some_id";

typedef struct cluster_client_s { 
    int use_count;
} cluster_client_t;

cluster_client_t s_cluster_client = { 0 };

void cluster_client_increment_use_count(
        cluster_client_t *const cluster_client
        ) noexcept
{
    ubiquity_test_emit("increment_use_count");
    ++cluster_client->use_count;
}

void cluster_client_decrement_use_count(
        cluster_client_t *const cluster_client
        ) noexcept
{
    ubiquity_test_emit("decrement_use_count");
    --cluster_client->use_count;
}

/*
    Idle strategy 
 */

typedef struct backoff_idle_strategy_s backoff_idle_strategy_t;

void backoff_idle_strategy_init (        
        backoff_idle_strategy_t* idle_strategy 
        ) noexcept 
{
}

void backoff_idle_strategy_idle(
        backoff_idle_strategy_t* idle_strategy 
        ) noexcept 
{
}

void backoff_idle_strategy_set_work_count(
        backoff_idle_strategy_t* idle_strategy,
        int work_count 
        ) noexcept 
{
}

/*
 * TESTS
 */

typedef struct __attribute__((packed)) {
    message_common_t _;
    uint16_t application_id;
    uint16_t feedback_level_or_features_length;
    uint64_t model_id;
    int64_t query_id;
} message_application_t;

typedef struct __attribute__((packed)) {
    message_application_t _;
    float price;
    float probability;
} message_prediction_bid_shading_t;

typedef struct __attribute__((packed)) {
    message_application_t _;
    float probability;
} message_prediction_probability_t;

typedef struct __attribute__((packed)) {
    message_application_t _;
    int64_t creative_id;
    float price;
    float probability;
} message_prediction_line_item_t;

typedef struct __attribute__((packed)) {
    message_application_t _;
    int64_t creative_id;
    float price;
} message_prediction_action_rate_t;

typedef struct __attribute__((packed)) {
    message_application_t _;
    uint32_t feedback_delay;
} message_query_t;

typedef struct __attribute__((packed)) {
    message_query_t _;
    float bid_floor;
    float bid_price;
    int32_t optimization_mode;
} message_query_bid_shading_t;

typedef message_query_t message_query_probability_t;

typedef struct __attribute__((packed)) {
    message_query_t _;
    int32_t num_creative;
} message_query_with_creatives_t;

typedef struct __attribute__((packed)) {
    message_query_with_creatives_t _;
    float max_cpm;
    float bid_floor;
    int32_t exposure_count;
} message_query_line_item_t;

typedef struct __attribute__((packed)) {
    message_query_with_creatives_t _;
    float bid_price;
    float bid_floor;
    int32_t exposure_count;
} message_query_action_rate_t;

typedef struct __attribute__((packed)) {
    message_application_t _;
    uint32_t feedback_delay;
    uint32_t next_feedback_delay;
    uint32_t event_occured;
} message_feedback_t;

typedef struct __attribute__((packed)) {
    message_feedback_t _;
    uint32_t reserved;
    double weight;
} message_feedback_first_level_t;

typedef struct __attribute__((packed)) {
  message_application_t _;
  uint32_t feedback_delay;
} message_cancel_t;

typedef union {
    uint8_t buffer[MAXIMUM_TRANSMISSION_UNIT];
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

int test_create_delete(void)
{

    // Expects a new handle to be created, increasing use count of the cluster client.
    client_handle_t *client_handle = client_handle_create(
            &s_cluster_client,
            s_store,
            s_cluster_id
            );
    ubiquity_test_expect("increment_use_count");
    ubiquity_test_done_expecting();
    ubiquity_test_assert_equal_string(
            "SUCCESS",
            client_handle_get_status_string(client_handle)
            );

    // Expects the handle to be deleted only once, decreasing use count of the cluster client only once.
    // An invalid handle should be returned at first delete (because it is used by the client handle class to get another pimpl).
    // Releasing an invalid handle should return the same invalid handle (i.e. `client_handle_delete` is idempotent).
    client_handle = client_handle_delete(client_handle);
    client_handle = client_handle_delete(client_handle);
    ubiquity_test_expect("decrement_use_count");
    ubiquity_test_done_expecting();
    ubiquity_test_assert_equal_string(
            "INVALID_HANDLE",
            client_handle_get_status_string(client_handle)
            );
    ubiquity_test_assert_equal(0, s_cluster_client.use_count);
    return 0;
}

static inline void load_feature_type_array(
        model_info_t *const model_info,
        const feature_type_t *const feature_type_array
        )
{
    size_t count = 0;
    feature_type_list_reset(&model_info->feature_type_list);
    while (FEATURE_TYPE_NONE != feature_type_array[count]) {
        feature_type_list_set_element(
                &model_info->feature_type_list,
                count,
                feature_type_array[count]
                );
        ++count;
    }
    model_info->feature_type_count = count;
}

feedback_id_t decode_feedback_id(
        const char *const feedback_name
        )
{
    return XXH3_64bits(
            feedback_name,
            strlen(feedback_name)
            );
}

typedef struct feedback_s {
    const char *feedback_name;
    feedback_delay_t feedback_delay;
} feedback_t;

static inline void load_feedback_list(
        model_info_t *const model_info,
        const feedback_t *const feedback_list
        )
{
    model_info_clear_feedback_list(model_info);
    size_t index = 0;
    while (NULL != feedback_list[index].feedback_name) {
        model_info_add_feedback(
                model_info,
                decode_feedback_id(feedback_list[index].feedback_name),
                feedback_list[index].feedback_delay
                );
        ++index;
    }
}

int test_predict(void)
{
    // Constants.
    const uint64_t model_id = 1;
    const uint64_t query_id = 123;
    const feature_type_t feature_type_array_1[] = {
        FEATURE_TYPE_NUMERIC,
        FEATURE_TYPE_NOMINAL,
        FEATURE_TYPE_BOOLEAN,
        FEATURE_TYPE_CATEGORICAL,
        FEATURE_TYPE_NUMERIC,
        FEATURE_TYPE_NOMINAL,
        FEATURE_TYPE_BOOLEAN,
        FEATURE_TYPE_CATEGORICAL,
        FEATURE_TYPE_NONE
    };
    feedback_t feedback_list_1[] = {
        "first_feedback", 0x3333,
        NULL, 0,
    };

    // Variables.
    status_code_t status_code;
    message_buffer_t message;

    // Expects a new handle to be created, increasing use count of the cluster client.
    client_handle_t *client_handle = client_handle_create(
            &s_cluster_client,
            s_store,
            s_cluster_id
            );
    ubiquity_test_expect("increment_use_count");
    ubiquity_test_done_expecting();
    ubiquity_test_assert_equal_string(
            "SUCCESS",
            client_handle_get_status_string(client_handle)
            );

    // Set model information.
    model_info_initialize(
            s_model_info,
            model_id
            );
    model_info_set_application_id(
            s_model_info,
            APPLICATION_TYPE_BID_SHADING
            );
    load_feature_type_array(
            s_model_info,
            feature_type_array_1
            );
    load_feedback_list(
            s_model_info,
            feedback_list_1
            );

    // Prepare query.
    char encoded_model_id[14];
    base_32C_encode(encoded_model_id, &model_id);
    client_handle_start_probability(client_handle);
    client_handle_put_boolean(client_handle, 1);

    // Send prediction query.
    status_code = client_handle_predict(
            client_handle,
            encoded_model_id,
            query_id
            );
    ubiquity_test_expect("store_get_model_info",
        "model_id" , model_id);
    ubiquity_test_done_expecting();

    // App APPLICATION_TYPE_PROBABILITY should be invalid for model's app APPLICATION_TYPE_BID_SHADING.
    ubiquity_test_assert_equal(STATUS_CODE_INVALID_APP_FOR_MODEL, status_code);
    ubiquity_test_done_expecting();

    // Try again with application ID 1.
    s_model_info->application_id = APPLICATION_TYPE_PROBABILITY;

    client_handle_start_probability(client_handle);
    client_handle_put_numeric(client_handle, 1);

    status_code = client_handle_predict(
            client_handle,
            encoded_model_id,
            query_id
            );
    ubiquity_test_expect("store_get_model_info",
        "model_id" , model_id);        
    // Feature types are not matching.
    ubiquity_test_assert_equal_string(
            "MISMATCHED_FEATURE_TYPE",
            client_handle_get_status_string(client_handle)
            );
    ubiquity_test_done_expecting();

    // Try again with matching feature types.
    client_handle_start_probability(client_handle);
    client_handle_put_numeric(client_handle, 0.1);
    client_handle_put_nominal(client_handle, "hey");
    client_handle_put_boolean(client_handle, 1);
    client_handle_put_categorical(client_handle, 7);
    client_handle_put_numeric(client_handle, 0.3);
    client_handle_put_nominal(client_handle, "hey");
    client_handle_put_boolean(client_handle, 0);
    client_handle_put_categorical(client_handle, 8);

    status_code = client_handle_predict(
            client_handle,
            encoded_model_id,
            query_id
            );
    ubiquity_test_expect("store_get_model_info",
        "model_id" , model_id);
    // This time the sending should be successful.
    ubiquity_test_assert_equal_string(
            "SUCCESS",
            client_handle_get_status_string(client_handle)
            );

    // Message header for type probability.
    message.common.version = SERIALIZATION_VERSION;
    message.common.type = MESSAGE_TYPE_QUERY;
    message.common.reserved = 0;
    message.application.application_id = APPLICATION_TYPE_PROBABILITY;
    message.application.feedback_level_or_features_length = 0x16;
    message.application.model_id = model_id;
    message.application.query_id = query_id ^ model_id;
    message.query.feedback_delay = 0x3333;

    // Buffer containing the features.
    buffer_clear(BUFFER_FEATURES);
    buffer_push_f32(BUFFER_FEATURES, 0.1); // numeric
    buffer_push_u32(BUFFER_FEATURES, 0x1d10fa30); // nominal "hey" through xxh3
    buffer_push_u8(BUFFER_FEATURES, 1); // boolean
    buffer_push_u8(BUFFER_FEATURES, 7); // categorical
    buffer_push_u16(BUFFER_FEATURES, 0); // padding
    buffer_push_f32(BUFFER_FEATURES, 0.3); // numeric
    buffer_push_u32(BUFFER_FEATURES, 0x1d10fa30); // nominal "hey" through xxh3
    buffer_push_u8(BUFFER_FEATURES, 0); // boolean
    buffer_push_u8(BUFFER_FEATURES, 8); // categorical

    // Buffer containing the whole message with header and features.
    buffer_clear(BUFFER_MESSAGE);
    buffer_push_mem(
            BUFFER_MESSAGE,
            &message,
            sizeof(message_query_probability_t)
            );
    buffer_push_buffer(
            BUFFER_MESSAGE,
            BUFFER_FEATURES
            );

    // Check that a message with the right content is passed along the right compatible_node_set.
    ubiquity_test_expect("shared_send_message_with_rendezvous",
            "compatible_node_set", &s_model_info->compatible_node_set,
            "balance_key", query_id ^ model_id,
            "stream_id", STREAM_ID_PREDICTION,
            "message", buffer_size(BUFFER_MESSAGE), buffer_content(BUFFER_MESSAGE)
            );
    ubiquity_test_done_expecting();

    const float bid_price = 0.123;
    const float bid_floor = 0.456;
    const int32_t optimization_mode = 1;

    // Same for bid shading.
    s_model_info->application_id = APPLICATION_TYPE_BID_SHADING;
    client_handle_start_bid_shading(
            client_handle,
            bid_price,
            bid_floor,
            optimization_mode
            );
    client_handle_put_numeric(client_handle, 0.1);
    client_handle_put_nominal(client_handle, "hey");
    client_handle_put_boolean(client_handle, 1);
    client_handle_put_categorical(client_handle, 7);
    client_handle_put_numeric(client_handle, 0.3);
    client_handle_put_nominal(client_handle, "hey");
    client_handle_put_boolean(client_handle, 0);
    client_handle_put_categorical(client_handle, 8);

    status_code = client_handle_predict(
            client_handle,
            encoded_model_id,
            query_id
            );
    ubiquity_test_expect("store_get_model_info",
        "model_id" , model_id);            
    ubiquity_test_assert_equal_string(
            "SUCCESS",
            client_handle_get_status_string(client_handle)
            );

    // Set values specific to bid shading.
    message.application.application_id = APPLICATION_TYPE_BID_SHADING;
    message.bid_shading_query.bid_floor = bid_floor;
    message.bid_shading_query.bid_price = bid_price;
    message.bid_shading_query.optimization_mode = optimization_mode;

    // Buffer containing the whole message with header and features.
    buffer_clear(BUFFER_MESSAGE);
    buffer_push_mem(
            BUFFER_MESSAGE,
            &message,
            sizeof(message_query_bid_shading_t)
            );
    buffer_push_buffer(
            BUFFER_MESSAGE,
            BUFFER_FEATURES
            );

    // Check that a message with the right content is passed along the right compatible_node_set.
    ubiquity_test_expect("shared_send_message_with_rendezvous",
            "compatible_node_set", &s_model_info->compatible_node_set,
            "balance_key", query_id ^ model_id,
            "stream_id", STREAM_ID_PREDICTION,
            "message", buffer_size(BUFFER_MESSAGE), buffer_content(BUFFER_MESSAGE)
            );
    ubiquity_test_done_expecting();

    // Same for line item.
    const float max_cpm = 0.789;
    const int32_t exposure_count = 0x4488;
    s_model_info->application_id = APPLICATION_TYPE_LINE_ITEM;
    client_handle_start_line_item(
            client_handle,
            max_cpm,
            bid_floor,
            exposure_count
            );

    client_handle_put_numeric(client_handle, 0.1);
    client_handle_put_nominal(client_handle, "hey");
    client_handle_put_boolean(client_handle, 1);
    client_handle_put_categorical(client_handle, 7);
    client_handle_put_numeric(client_handle, 0.3);
    client_handle_put_nominal(client_handle, "hey");
    client_handle_put_boolean(client_handle, 0);
    client_handle_put_categorical(client_handle, 8);

    status_code = client_handle_predict(
            client_handle,
            encoded_model_id,
            query_id
            );
    ubiquity_test_expect("store_get_model_info",
        "model_id" , model_id);
    // Should fail because no creative was put.
    ubiquity_test_assert_equal_string(
            "MISSING_CREATIVES",
            client_handle_get_status_string(client_handle)
            );

    // Again with creative.
    const int64_t creative_id = 0x777;
    client_handle_start_line_item(
            client_handle,
            max_cpm,
            bid_floor,
            exposure_count
            );

    client_handle_put_creative(client_handle, creative_id);

    client_handle_put_numeric(client_handle, 0.1);
    client_handle_put_nominal(client_handle, "hey");
    client_handle_put_boolean(client_handle, 1);
    client_handle_put_categorical(client_handle, 7);
    client_handle_put_numeric(client_handle, 0.3);
    client_handle_put_nominal(client_handle, "hey");
    client_handle_put_boolean(client_handle, 0);
    client_handle_put_categorical(client_handle, 8);

    status_code = client_handle_predict(
            client_handle,
            encoded_model_id,
            query_id
            );
    ubiquity_test_expect("store_get_model_info",
        "model_id" , model_id);
    // This time the sending should be successful.
    ubiquity_test_assert_equal_string(
            "SUCCESS",
            client_handle_get_status_string(client_handle)
            );

    // Set values specific to line item.
    message.application.application_id = APPLICATION_TYPE_LINE_ITEM;
    message.line_item_query.max_cpm = max_cpm;
    message.line_item_query.bid_floor = bid_floor;
    message.line_item_query.exposure_count = exposure_count;

    // Buffer containing creatives.
    buffer_clear(BUFFER_CREATIVES);
    buffer_push_i64(
            BUFFER_CREATIVES,
            creative_id
            );
    message.creative_query.num_creative = 1;

    // Buffer containing the whole message with header and features.
    buffer_clear(BUFFER_MESSAGE);
    buffer_push_mem(
            BUFFER_MESSAGE,
            &message,
            sizeof(message_query_line_item_t)
            );
    buffer_push_buffer(
            BUFFER_MESSAGE,
            BUFFER_FEATURES
            );
    buffer_align(
            BUFFER_MESSAGE,
            8
            );
    buffer_push_buffer(
            BUFFER_MESSAGE,
            BUFFER_CREATIVES
            );

    // Check that a message with the right content is passed along the right compatible_node_set.
    ubiquity_test_expect("shared_send_message_with_rendezvous",
            "compatible_node_set", &s_model_info->compatible_node_set,
            "balance_key", query_id ^ model_id,
            "stream_id", STREAM_ID_PREDICTION,
            "message", buffer_size(BUFFER_MESSAGE), buffer_content(BUFFER_MESSAGE)
            );
    ubiquity_test_done_expecting();

    // Again for action rate.
    s_model_info->application_id = APPLICATION_TYPE_ACTION_RATE;
    client_handle_start_action_rate(
            client_handle,
            bid_price,
            bid_floor,
            exposure_count
            );

    client_handle_put_creative(client_handle, creative_id);

    client_handle_put_numeric(client_handle, 0.1);
    client_handle_put_nominal(client_handle, "hey");
    client_handle_put_boolean(client_handle, 1);
    client_handle_put_categorical(client_handle, 7);
    client_handle_put_numeric(client_handle, 0.3);
    client_handle_put_nominal(client_handle, "hey");
    client_handle_put_boolean(client_handle, 0);
    client_handle_put_categorical(client_handle, 8);

    status_code = client_handle_predict(
            client_handle,
            encoded_model_id,
            query_id
            );
    ubiquity_test_expect("store_get_model_info",
        "model_id" , model_id);            
    ubiquity_test_assert_equal_string(
            "SUCCESS",
            client_handle_get_status_string(client_handle)
            );

    // Set values specific to action rate.
    message.application.application_id = APPLICATION_TYPE_ACTION_RATE;
    message.action_rate_query.bid_price = bid_price;
    message.action_rate_query.bid_floor = bid_floor;
    message.action_rate_query.exposure_count = exposure_count;

    // Buffer containing the whole message with header and features.
    buffer_clear(BUFFER_MESSAGE);
    buffer_push_mem(
            BUFFER_MESSAGE,
            &message,
            sizeof(message_query_action_rate_t)
            );
    buffer_push_buffer(
            BUFFER_MESSAGE,
            BUFFER_FEATURES
            );
    buffer_align(
            BUFFER_MESSAGE,
            8
            );
    buffer_push_buffer(
            BUFFER_MESSAGE,
            BUFFER_CREATIVES
            );

    // Check that a message with the right content is passed along the right compatible_node_set.
    ubiquity_test_expect("shared_send_message_with_rendezvous",
            "compatible_node_set", &s_model_info->compatible_node_set,
            "balance_key", query_id ^ model_id,
            "stream_id", STREAM_ID_PREDICTION,
            "message", buffer_size(BUFFER_MESSAGE), buffer_content(BUFFER_MESSAGE)
            );
    ubiquity_test_done_expecting();
    return 0;
}

typedef struct {
    int64_t creative_id;
    size_t creative_offset;
} creative_check_t;

#include <stdio.h>

#define TESTED_CREATIVE_COUNT 10
#define MAX_CREATIVE_COUNT 20
#define SENT_LINE_ITEM_QUERY 1

bool creative_was_selected(
        const event_argument_t *const argument,
        void *const user_data
        )
{
    const uint8_t *buffer;
    memcpy(&buffer, &argument->value, sizeof(buffer));
	int32_t num_creatives = TESTED_CREATIVE_COUNT < MAX_CREATIVE_COUNT ? TESTED_CREATIVE_COUNT : MAX_CREATIVE_COUNT;

    const creative_check_t *const check = (creative_check_t *)user_data;
	const int64_t *creatives = (const int64_t *)(buffer + check->creative_offset);

    for (int32_t cursor = 0; cursor < num_creatives; cursor++) {
        int64_t creative_id = creatives[cursor];
        if (check->creative_id == creative_id) {
            return true;
        }
    }
    return false;
}

int64_t creative_ids[TESTED_CREATIVE_COUNT];

status_code_t send_a_line_item_query(
        client_handle_t *const client_handle,
        const char *const encoded_model_id,
        const uint64_t query_id,
        const float max_cpm,
        const float bid_floor,
        const int32_t exposure_count
        )
{
    client_handle_start_line_item(
            client_handle,
            max_cpm,
            bid_floor,
            exposure_count
            );

    for (int64_t i = 0; i < TESTED_CREATIVE_COUNT; i++) {
        client_handle_put_creative(client_handle, creative_ids[i]);
    }

    client_handle_put_numeric(client_handle, 0.1);
    client_handle_put_nominal(client_handle, "hey");
    client_handle_put_boolean(client_handle, 1);
    client_handle_put_categorical(client_handle, 7);
    client_handle_put_numeric(client_handle, 0.3);
    client_handle_put_nominal(client_handle, "hey");
    client_handle_put_boolean(client_handle, 0);
    client_handle_put_categorical(client_handle, 8);

    const status_code_t status_code = client_handle_predict(
            client_handle,
            encoded_model_id,
            query_id
            );

    return status_code;
}

int test_creative_balance(void)
{
    // Constants.
    const uint64_t model_id = 1;
    const uint64_t query_id = 123;
    const feature_type_t feature_type_array_1[] = {
        FEATURE_TYPE_NUMERIC,
        FEATURE_TYPE_NOMINAL,
        FEATURE_TYPE_BOOLEAN,
        FEATURE_TYPE_CATEGORICAL,
        FEATURE_TYPE_NUMERIC,
        FEATURE_TYPE_NOMINAL,
        FEATURE_TYPE_BOOLEAN,
        FEATURE_TYPE_CATEGORICAL,
        FEATURE_TYPE_NONE
    };
    feedback_t feedback_list_1[] = {
        "first_feedback", 0x3333,
        NULL, 0,
    };

    // Expects a new handle to be created, increasing use count of the cluster client.
    client_handle_t *client_handle = client_handle_create(
            &s_cluster_client,
            s_store,
            s_cluster_id
            );
    ubiquity_test_expect("increment_use_count");
    ubiquity_test_done_expecting();
    ubiquity_test_assert_equal_string(
            "SUCCESS",
            client_handle_get_status_string(client_handle)
            );

    // Set model information.
    model_info_initialize(
            s_model_info,
            model_id
            );
    model_info_set_application_id(
            s_model_info,
            APPLICATION_TYPE_LINE_ITEM
            );
    load_feature_type_array(
            s_model_info,
            feature_type_array_1
            );
    load_feedback_list(
            s_model_info,
            feedback_list_1
            );

    // Prepare line item query.
    char encoded_model_id[14];
    base_32C_encode(encoded_model_id, &model_id);
    const float max_cpm = 0.789;
    const float bid_floor = 0.456;
    const int32_t exposure_count = 0x4488;
	ubiquity_prng_t prng;
	ubiquity_prng_seed(&prng);
	for (int i = 0; i < TESTED_CREATIVE_COUNT; ++i) {
		creative_ids[i] = (int64_t) ubiquity_prng_generate(&prng);
	}

    for (size_t cursor = 0; cursor < SENT_LINE_ITEM_QUERY; cursor++) {
        ubiquity_test_assert_equal(STATUS_CODE_SUCCESS, send_a_line_item_query(client_handle, encoded_model_id, query_id, max_cpm, bid_floor, exposure_count));
        ubiquity_test_expect("store_get_model_info",
                "model_id" , model_id);
        // Consume all send message balanced events.
        ubiquity_test_expect_partial("shared_send_message_with_rendezvous", 3,
                "compatible_node_set", &s_model_info->compatible_node_set,
                "balance_key", query_id ^ model_id,
                "stream_id", STREAM_ID_PREDICTION
                );
    }
    
	const size_t creative_offset = 72;

	// Count occurences of every creative ID to later verify balance.
	creative_check_t check = {
			0,
			creative_offset
	};

    int64_t counts[TESTED_CREATIVE_COUNT];
	for (int i = 0; i < TESTED_CREATIVE_COUNT; ++i) {
			check.creative_id = creative_ids[i];
			counts[i] = ubiquity_test_count_filtered_by_arguments("shared_send_message_with_rendezvous",
					"compatible_node_set", ubiquity_test_ignore, NULL,
					"balance_key", ubiquity_test_ignore, NULL,
					"stream_id", ubiquity_test_ignore, NULL,
					"message", creative_was_selected, &check
					);
	}
	// Debug: print counts
	fprintf(stderr, "\nCreative ID counts:\n");
	for (int64_t i = 0; i < TESTED_CREATIVE_COUNT; ++i) {
		fprintf(stderr, "Creative %ld: %ld\n", i, counts[i]);
	}
	fflush(stderr);

    // Verify that creative balancing works within a 5% margin.
    ubiquity_test_assert_almost_equal_array(
            0.05,
            TESTED_CREATIVE_COUNT,
            counts
            );
    ubiquity_test_done_expecting();
    return 0;
}

int test_feedback(void)
{
    // Constants.
    const uint64_t model_id = 1;
    const uint64_t query_id = 123;
    const feature_type_t feature_type_array_1[] = {
        FEATURE_TYPE_NUMERIC,
        FEATURE_TYPE_NOMINAL,
        FEATURE_TYPE_BOOLEAN,
        FEATURE_TYPE_CATEGORICAL,
        FEATURE_TYPE_NUMERIC,
        FEATURE_TYPE_NOMINAL,
        FEATURE_TYPE_BOOLEAN,
        FEATURE_TYPE_CATEGORICAL,
        FEATURE_TYPE_NONE
    };
    feedback_t feedback_list_1[] = {
        "FIRST FEEDBACK", 0x3333,
        "SECOND FEEDBACK", 0x1111,
        NULL, 0,
    };

    // Variables.
    message_buffer_t message;
    status_code_t status_code;

    // Expects a new handle to be created, increasing use count of the cluster client.
    client_handle_t *client_handle = client_handle_create(
            &s_cluster_client,
            s_store,
            s_cluster_id
            );
    ubiquity_test_expect("increment_use_count");
    ubiquity_test_done_expecting();
    ubiquity_test_assert_equal_string(
            "SUCCESS",
            client_handle_get_status_string(client_handle)
            );

    // Set model information.
    model_info_initialize(
            s_model_info,
            model_id
            );
    model_info_set_application_id(
            s_model_info,
            APPLICATION_TYPE_LINE_ITEM
            );
    load_feature_type_array(
            s_model_info,
            feature_type_array_1
            );
    load_feedback_list(
            s_model_info,
            feedback_list_1
            );

    // Send line item feedback.
    char encoded_model_id[14];
    base_32C_encode(encoded_model_id, &model_id);

    status_code = client_handle_train(
            client_handle,
            encoded_model_id,
            query_id,
            "BAD FEEDBACK"
            );

    ubiquity_test_expect("store_get_model_info",
        "model_id" , model_id);
    // Expect an error because the feedback is not registered in the model metadata.
    ubiquity_test_assert_equal(
            STATUS_CODE_INVALID_FEEDBACK_LEVEL,
            status_code
            );
    ubiquity_test_assert_equal_string(
            "INVALID_FEEDBACK_LEVEL",
            client_handle_get_status_string(client_handle)
            );

    // Again with a valid feedback level (first feedback).
    status_code = client_handle_train(
            client_handle,
            encoded_model_id,
            query_id,
            "FIRST FEEDBACK"
            );

    ubiquity_test_expect("store_get_model_info",
        "model_id" , model_id);            
    ubiquity_test_assert_equal_string(
            "SUCCESS",
            client_handle_get_status_string(client_handle)
            );
    ubiquity_test_assert_equal(
            STATUS_CODE_SUCCESS,
            status_code
            );

    // Message header for line item feedback.
    message.common.version = SERIALIZATION_VERSION;
    message.common.type = MESSAGE_TYPE_FEEDBACK;
    message.common.reserved = 0;
    message.application.application_id = APPLICATION_TYPE_LINE_ITEM;
    message.application.feedback_level_or_features_length = 0;
    message.application.model_id = model_id;
    message.application.query_id = query_id ^ model_id;
    message.feedback.feedback_delay = 0x3333;
    message.feedback.next_feedback_delay = 0x1111;
    message.feedback.event_occured = 1;
    message.feedback_first_level.reserved = 0;
    message.feedback_first_level.weight = 1.0;

    // Buffer containing the whole message with header and features.
    buffer_clear(BUFFER_MESSAGE);
    buffer_push_mem(BUFFER_MESSAGE, &message, sizeof(message_feedback_first_level_t));

    // Check that a message with the right content is passed along the right compatible_node_set.
    ubiquity_test_expect("shared_send_message_with_rendezvous",
            "compatible_node_set", &s_model_info->compatible_node_set,
            "balance_key", query_id ^ model_id,
            "stream_id", STREAM_ID_FEEDBACK,
            "message", buffer_size(BUFFER_MESSAGE), buffer_content(BUFFER_MESSAGE)
            );
    ubiquity_test_done_expecting();

    // Again but with higher feedback level.
    status_code = client_handle_train(
            client_handle,
            encoded_model_id,
            query_id,
            "SECOND FEEDBACK"
            );

    ubiquity_test_expect("store_get_model_info",
        "model_id" , model_id);            
    ubiquity_test_assert_equal(
            STATUS_CODE_SUCCESS,
            status_code
            );

    // Message header for line item feedback.
    message.application.feedback_level_or_features_length = 1;
    message.feedback.feedback_delay = 0x1111;
    message.feedback.next_feedback_delay = 0;
    message.feedback.event_occured = 1;

    // Buffer containing the whole message with header and features.
    buffer_clear(BUFFER_MESSAGE);
    buffer_push_mem(BUFFER_MESSAGE, &message, sizeof(message_feedback_t));

    // Check that a message with the right content is passed along the right compatible_node_set.
    ubiquity_test_expect("shared_send_message_with_rendezvous",
            "compatible_node_set", &s_model_info->compatible_node_set,
            "balance_key", query_id ^ model_id,
            "stream_id", STREAM_ID_FEEDBACK,
            "message", buffer_size(BUFFER_MESSAGE), buffer_content(BUFFER_MESSAGE)
            );


    ubiquity_test_done_expecting();
    return 0;
}

int test_cancel(void)
{
    // Constants.
    const uint64_t model_id = 1;
    const uint64_t query_id = 123;
    const feature_type_t feature_type_array_1[] = {
        FEATURE_TYPE_NUMERIC,
        FEATURE_TYPE_NOMINAL,
        FEATURE_TYPE_BOOLEAN,
        FEATURE_TYPE_CATEGORICAL,
        FEATURE_TYPE_NUMERIC,
        FEATURE_TYPE_NOMINAL,
        FEATURE_TYPE_BOOLEAN,
        FEATURE_TYPE_CATEGORICAL,
        FEATURE_TYPE_NONE
    };
    feedback_t feedback_list_1[] = {
        "FIRST FEEDBACK", 0x3333,
        "SECOND FEEDBACK", 0x1111,
        NULL, 0,
    };

    // Variables.
    message_buffer_t message;
    status_code_t status_code;

    // Expects a new handle to be created, increasing use count of the cluster client.
    client_handle_t *client_handle = client_handle_create(
            &s_cluster_client,
            s_store,
            s_cluster_id
            );
    ubiquity_test_expect("increment_use_count");
    ubiquity_test_done_expecting();
    ubiquity_test_assert_equal_string(
            "SUCCESS",
            client_handle_get_status_string(client_handle)
            );

    // Set model information.
    model_info_initialize(
            s_model_info,
            model_id
            );
    model_info_set_application_id(
            s_model_info,
            APPLICATION_TYPE_LINE_ITEM
            );
    load_feature_type_array(
            s_model_info,
            feature_type_array_1
            );
    load_feedback_list(
            s_model_info,
            feedback_list_1
            );

    // Send line item feedback.
    char encoded_model_id[14];
    base_32C_encode(encoded_model_id, &model_id);

    status_code = client_handle_cancel(
            client_handle,
            encoded_model_id,
            query_id
            );
    ubiquity_test_expect("store_get_model_info",
        "model_id" , model_id);            
    ubiquity_test_assert_equal(
            STATUS_CODE_SUCCESS,
            status_code
            );

    // Message header for line item feedback.
    message.common.version = SERIALIZATION_VERSION;
    message.common.type = MESSAGE_TYPE_CANCEL;
    message.common.reserved = 0;
    message.application.application_id = APPLICATION_TYPE_LINE_ITEM;
    message.application.feedback_level_or_features_length = 0;
    message.application.model_id = model_id;
    message.application.query_id = query_id ^ model_id;
    message.cancel.feedback_delay = 0x3333;

    // Buffer containing the whole message with header and features.
    buffer_clear(BUFFER_MESSAGE);
    buffer_push_mem(
            BUFFER_MESSAGE,
            &message,
            sizeof(message_cancel_t)
            );

    // Check that a message with the right content is passed along the right compatible_node_set.
    ubiquity_test_expect("shared_send_message_with_rendezvous",
            "compatible_node_set", &s_model_info->compatible_node_set,
            "balance_key", query_id ^ model_id,
            "stream_id", STREAM_ID_FEEDBACK,
            "message", buffer_size(BUFFER_MESSAGE), buffer_content(BUFFER_MESSAGE)
            );
    ubiquity_test_done_expecting();

    return 0;
}

int test_poll(void)
{
    message_buffer_t message;

    // Ids.
    const uint64_t model_id = 1;
    char encoded_model_id[14];
    base_32C_encode(encoded_model_id, &model_id);
    const uint64_t query_id = 123;
    int64_t creative_id;
    float price;
    float probability;

    // Expects a new handle to be created, increasing use count of the cluster client.
    client_handle_t *client_handle = client_handle_create(
            &s_cluster_client,
            s_store,
            s_cluster_id
            );
    ubiquity_test_expect("increment_use_count");
    ubiquity_test_done_expecting();
    ubiquity_test_assert_equal_string(
            "SUCCESS",
            client_handle_get_status_string(client_handle)
            );

    // Poll. Expect status code no data.
    ubiquity_test_assert_equal(
            STATUS_CODE_NO_DATA,
            client_handle_poll(client_handle)
            );

    // Poll a probability message.
    probability = 0.4f;
    message.common.version = SERIALIZATION_VERSION;
    message.common.type = MESSAGE_TYPE_PREDICTION;
    message.common.reserved = 0;
    message.application.application_id = APPLICATION_TYPE_PROBABILITY;
    message.application.feedback_level_or_features_length = 0;
    message.application.model_id = model_id;
    message.application.query_id = query_id ^ model_id;
    message.probability_prediction.probability = probability;

    // Buffer containing the whole message.
    buffer_clear(BUFFER_PREDICTION);
    buffer_push_mem(
            BUFFER_PREDICTION,
            &message,
            sizeof(message_prediction_probability_t)
            );

    // Should succeed.
    ubiquity_test_assert_equal(
            STATUS_CODE_SUCCESS,
            client_handle_poll(client_handle)
            );

    // Polled values should equal message's values.
    ubiquity_test_assert_equal_string(
            encoded_model_id,
            client_handle_get_model_id(client_handle)
            );
    ubiquity_test_assert_equal(
            query_id,
            client_handle_get_query_id(client_handle)
            );
    ubiquity_test_assert_equal(
            probability,
            client_handle_get_probability(client_handle)
            );

    // Again with a bid shading message.
    price = 3.6f;
    probability = 0.7f;
    message.common.type = MESSAGE_TYPE_PREDICTION;
    message.common.reserved = 0;
    message.application.application_id = APPLICATION_TYPE_BID_SHADING;
    message.application.feedback_level_or_features_length = 0;
    message.application.model_id = model_id;
    message.application.query_id = query_id ^ model_id;
    message.bid_shading_prediction.price = price;
    message.bid_shading_prediction.probability = probability;

    // Message should be ignored because its size is invalid.
    buffer_clear(BUFFER_PREDICTION);
    buffer_push_mem(
            BUFFER_PREDICTION,
            &message,
            sizeof(message_prediction_probability_t)
            );
    ubiquity_test_assert_equal(
            STATUS_CODE_NO_DATA,
            client_handle_poll(client_handle)
            );

    // Should succeed.
    buffer_clear(BUFFER_PREDICTION);
    buffer_push_mem(
            BUFFER_PREDICTION,
            &message,
            sizeof(message_prediction_bid_shading_t)
            );
    ubiquity_test_assert_equal(
            STATUS_CODE_SUCCESS,
            client_handle_poll(client_handle)
            );

    // Polled values should equal message's values.
    ubiquity_test_assert_equal_string(
            encoded_model_id,
            client_handle_get_model_id(client_handle)
            );
    ubiquity_test_assert_equal(
            query_id,
            client_handle_get_query_id(client_handle)
            );
    ubiquity_test_assert_equal(
            price,
            client_handle_get_price(client_handle)
            );
    ubiquity_test_assert_equal(
            probability,
            client_handle_get_probability(client_handle)
            );

    // Again with a line item message.
    creative_id = 123;
    price = 8.2f;
    probability = 1.1f;
    message.common.type = MESSAGE_TYPE_PREDICTION;
    message.common.reserved = 0;
    message.application.application_id = APPLICATION_TYPE_LINE_ITEM;
    message.application.feedback_level_or_features_length = 0;
    message.application.model_id = model_id;
    message.application.query_id = query_id ^ model_id;
    message.line_item_prediction.creative_id = creative_id;
    message.line_item_prediction.price = price;
    message.line_item_prediction.probability = probability;

    // Buffer containing the whole message.
    buffer_clear(BUFFER_PREDICTION);
    buffer_push_mem(
            BUFFER_PREDICTION,
            &message,
            sizeof(message_prediction_line_item_t)
            );

    // Should succeed.
    ubiquity_test_assert_equal(
            STATUS_CODE_SUCCESS,
            client_handle_poll(client_handle)
            );

    // Polled values should equal message's values.
    ubiquity_test_assert_equal_string(
            encoded_model_id,
            client_handle_get_model_id(client_handle)
            );
    ubiquity_test_assert_equal(
            query_id,
            client_handle_get_query_id(client_handle)
            );
    ubiquity_test_assert_equal(
            creative_id,
            client_handle_get_creative_id(client_handle)
            );
    ubiquity_test_assert_equal(
            price,
            client_handle_get_price(client_handle)
            );
    ubiquity_test_assert_equal(
            probability,
            client_handle_get_probability(client_handle)
            );

    // Again with a action rate message.
    creative_id = 323;
    price = 2.9f;
    message.common.type = MESSAGE_TYPE_PREDICTION;
    message.common.reserved = 0;
    message.application.application_id = APPLICATION_TYPE_ACTION_RATE;
    message.application.feedback_level_or_features_length = 0;
    message.application.model_id = model_id;
    message.application.query_id = query_id ^ model_id;
    message.action_rate_prediction.creative_id = creative_id;
    message.action_rate_prediction.price = price;

    // Buffer containing the whole message.
    buffer_clear(BUFFER_PREDICTION);
    buffer_push_mem(
            BUFFER_PREDICTION,
            &message,
            sizeof(message_prediction_action_rate_t)
            );

    // Should succeed.
    ubiquity_test_assert_equal(
            STATUS_CODE_SUCCESS,
            client_handle_poll(client_handle)
            );

    // Polled values should equal message's values.
    ubiquity_test_assert_equal_string(
            encoded_model_id,
            client_handle_get_model_id(client_handle)
            );
    ubiquity_test_assert_equal(
            query_id,
            client_handle_get_query_id(client_handle)
            );
    ubiquity_test_assert_equal(
            creative_id,
            client_handle_get_creative_id(client_handle)
            );
    ubiquity_test_assert_equal(
            price,
            client_handle_get_price(client_handle)
            );

    return 0;
}

int main(void) {
    ubiquity_test_declare("store_get_model_info", 1,
            "model_id", "u64"
            );

    ubiquity_test_declare("shared_send_message_with_rendezvous", 4,
            "compatible_node_set", "pointer",
            "balance_key", "i64",
            "stream_id", "i32",
            "message", "buffer"
            );

    ubiquity_test_declare("increment_use_count", 0);

    ubiquity_test_declare("decrement_use_count", 0);

    ubiquity_test_function_t test_list[] = {
        test_create_delete,
        test_predict,
        test_creative_balance, 
        test_feedback,
        test_cancel,
        test_poll,
        0,
    };
    return ubiquity_test_all(test_list);
}
