#pragma once

#include <memory>

#include "common/common.hpp"
#include "utils/logger.hpp"

typedef struct client_handle_s client_handle_t;
typedef struct cluster_client_s cluster_client_t;
typedef struct web_client_s web_client_t;
typedef struct shared_state_store_s shared_state_store_t;
typedef std::shared_ptr<shared_state_store_t> shared_state_store_ref_t;

/*
 * HANDLE CREATION, CONFIGURATION AND RELEASE
 */

void client_handle_set_default_log_level(
        const ubiquity_log_level_t level
        ) noexcept;

client_handle_t *client_handle_create(
        cluster_client_t *const cluster_client,
        shared_state_store_ref_t shared_state_store_t,
        const char *const cluster_id
        ) noexcept;

void client_handle_set_log_level(
        client_handle_t *const client_handle,
        const int32_t level
        ) noexcept;

client_handle_t *client_handle_delete(
        client_handle_t *const client_handle
        ) noexcept;

/*
 * PREDICTION QUERY STARTER FUNCTIONS
 */

void client_handle_start_bid_shading(
        client_handle_t *const client_handle,
        const float bid_price,
        const float bid_floor,
        const int32_t optimization_mode
        ) noexcept;

void client_handle_start_probability(
        client_handle_t *const client_handle
        ) noexcept;

void client_handle_start_line_item(
        client_handle_t *const client_handle,
        const float max_cpm,
        const float bid_floor,
        const int32_t exposure_count
        ) noexcept;

void client_handle_start_action_rate(
        client_handle_t *const client_handle,
        const float bid_price,
        const float bid_floor,
        const int32_t exposure_count
        ) noexcept;

void client_handle_start_binary_classification(
        client_handle_t *const client_handle
        ) noexcept;

/*
 * PREDICTION QUERY AND FEEDBACK
 */

status_code_t client_handle_predict(
        client_handle_t *const client_handle,
        const char *encoded_model_id,
        const int64_t query_id
        ) noexcept;

status_code_t client_handle_train(
        client_handle_t *const client_handle,
        const char *encoded_model_id,
        int64_t query_id,
        const char* const encoded_feedback_name
        ) noexcept;

status_code_t client_handle_train_with_level(
        client_handle_t *const client_handle,
        const char *encoded_model_id,
        int64_t query_id,
        const uint16_t feedback_level
        ) noexcept;

status_code_t client_handle_cancel(
        client_handle_t *const client_handle,
        const char *encoded_model_id,
        int64_t query_id
        ) noexcept;
/*
 * PREDICTION RESPONSE POLLING
 */

status_code_t client_handle_poll(
        client_handle_t *const client_handle
        ) noexcept;

void client_handle_idle(
        client_handle_t *const client_handle
        ) noexcept;

/*
 * GETTERS
 */

status_code_t client_handle_get_status_code(
        client_handle_t *const client_handle
        ) noexcept;

const char *client_handle_get_status_string(
        client_handle_t *const client_handle
        ) noexcept;

const char *client_handle_get_model_id(
        client_handle_t *const client_handle
        ) noexcept;

int64_t client_handle_get_query_id(
        client_handle_t *const client_handle
        ) noexcept;

int64_t client_handle_get_creative_id(
        client_handle_t *const client_handle
        ) noexcept;

float client_handle_get_price(
        client_handle_t *const client_handle
        ) noexcept;

float client_handle_get_probability(
        client_handle_t *const client_handle
        ) noexcept;

int32_t client_handle_get_prediction_type(
        client_handle_t *const client_handle
        ) noexcept;

int8_t client_handle_get_bid_status(
        client_handle_t *const client_handle
        ) noexcept;

bool client_handle_is_classified(
        client_handle_t *const client_handle
        ) noexcept;

/*
 * SETTERS
 */

void client_handle_put_boolean(
        client_handle_t *const client_handle,
        const int8_t value
        ) noexcept;

void client_handle_put_categorical(
        client_handle_t *const client_handle,
        const int8_t value
        ) noexcept;

void client_handle_put_creative(
        client_handle_t *const client_handle,
        const int64_t creative_id
        ) noexcept;

void client_handle_put_nominal(
        client_handle_t *const client_handle,
        const char *value
        ) noexcept;

void client_handle_put_numeric(
        client_handle_t *const client_handle,
        const float value
        ) noexcept;

/*
 * DIAGNOSTIC
 */

const char *client_handle_get_diagnostic_string(
        client_handle_t *const client_handle
        ) noexcept;
