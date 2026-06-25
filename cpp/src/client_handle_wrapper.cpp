#include "ubiquity_client/Ubiquity.hpp"
#include "components/client_handle.hpp"

Ubiquity::ClientHandle::ClientHandle(void) noexcept { }

void Ubiquity::ClientHandle::close(void) noexcept {
    this->pimpl = client_handle_delete(reinterpret_cast<client_handle_t *>(this->pimpl));
}

Ubiquity::StatusCode Ubiquity::ClientHandle::getStatusCode(void) noexcept {
    return client_handle_get_status_code(reinterpret_cast<client_handle_t *>(this->pimpl));
}

const char *Ubiquity::ClientHandle::getStatusString(void) noexcept {
    return client_handle_get_status_string(reinterpret_cast<client_handle_t *>(this->pimpl));
}

int64_t Ubiquity::ClientHandle::getCreativeId(void) noexcept {
    return client_handle_get_creative_id(reinterpret_cast<client_handle_t *>(this->pimpl));
}

int64_t Ubiquity::ClientHandle::getQueryId(void) noexcept {
    return client_handle_get_query_id(reinterpret_cast<client_handle_t *>(this->pimpl));
}

const char *Ubiquity::ClientHandle::getModelId(void) noexcept {
    return client_handle_get_model_id(reinterpret_cast<client_handle_t *>(this->pimpl));
}

float Ubiquity::ClientHandle::getPrice(void) noexcept {
    return client_handle_get_price(reinterpret_cast<client_handle_t *>(this->pimpl));
}

float Ubiquity::ClientHandle::getProbability(void) noexcept {
    return client_handle_get_probability(reinterpret_cast<client_handle_t *>(this->pimpl));
}

int8_t Ubiquity::ClientHandle::getBidStatus(void) noexcept {
    return client_handle_get_bid_status(reinterpret_cast<client_handle_t *>(this->pimpl));
}

int32_t Ubiquity::ClientHandle::getPredictionType(void) noexcept {
    return client_handle_get_prediction_type(reinterpret_cast<client_handle_t *>(this->pimpl));
}

Ubiquity::StatusCode Ubiquity::ClientHandle::poll(void) noexcept {
    return client_handle_poll(reinterpret_cast<client_handle_t *>(this->pimpl));
}

void Ubiquity::ClientHandle::idle(void) noexcept {
    return client_handle_idle(reinterpret_cast<client_handle_t *>(this->pimpl));
}

void Ubiquity::ClientHandle::putBoolean(const int8_t value) noexcept {
    return client_handle_put_boolean(reinterpret_cast<client_handle_t *>(this->pimpl), value);
}

void Ubiquity::ClientHandle::putCategorical(const int8_t value) noexcept {
    return client_handle_put_categorical(reinterpret_cast<client_handle_t *>(this->pimpl), value);
}

void Ubiquity::ClientHandle::putCreative(const int64_t value) noexcept {
    return client_handle_put_creative(reinterpret_cast<client_handle_t *>(this->pimpl), value);
}

void Ubiquity::ClientHandle::putNominal(const char *value) noexcept {
    return client_handle_put_nominal(reinterpret_cast<client_handle_t *>(this->pimpl), value);
}

void Ubiquity::ClientHandle::putNumeric(const float value) noexcept {
    return client_handle_put_numeric(reinterpret_cast<client_handle_t *>(this->pimpl), value);
}

void Ubiquity::ClientHandle::startBidShading(const float bidPrice, const float bidFloor, const int32_t optimizationMode) noexcept {
    return client_handle_start_bid_shading(reinterpret_cast<client_handle_t *>(this->pimpl), bidPrice, bidFloor, optimizationMode);
}

void Ubiquity::ClientHandle::startLineItem(const float maxCPM, const float bidFloor, const int32_t exposureCount) noexcept {
    return client_handle_start_line_item(reinterpret_cast<client_handle_t *>(this->pimpl), maxCPM, bidFloor, exposureCount);
}

void Ubiquity::ClientHandle::startActionRate(const float bidPrice, const float bidFloor, const int32_t exposureCount) noexcept {
    return client_handle_start_action_rate(reinterpret_cast<client_handle_t *>(this->pimpl), bidPrice, bidFloor, exposureCount);
}

void Ubiquity::ClientHandle::startProbability(void) noexcept {
    return client_handle_start_probability(reinterpret_cast<client_handle_t *>(this->pimpl));
}

Ubiquity::StatusCode Ubiquity::ClientHandle::predict(const char *modelId, const int64_t queryId) noexcept {
    return client_handle_predict(reinterpret_cast<client_handle_t *>(this->pimpl), modelId, queryId);
}

Ubiquity::StatusCode Ubiquity::ClientHandle::train(const char *modelId, const int64_t queryId, const uint16_t feedbackLevel) noexcept {
    return client_handle_train_with_level(reinterpret_cast<client_handle_t *>(this->pimpl), modelId, queryId, feedbackLevel);
}

Ubiquity::StatusCode Ubiquity::ClientHandle::cancel(const char *modelId, const int64_t queryId) noexcept {
    return client_handle_cancel(reinterpret_cast<client_handle_t *>(this->pimpl), modelId, queryId);
}

const char *Ubiquity::ClientHandle::getDiagnosticString() noexcept {
    return client_handle_get_diagnostic_string(reinterpret_cast<client_handle_t *>(this->pimpl));
}

void Ubiquity::ClientHandle::setLogLevel(const int32_t level) noexcept {
    return client_handle_set_log_level(reinterpret_cast<client_handle_t *>(this->pimpl), level);
}