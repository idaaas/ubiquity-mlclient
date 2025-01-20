#include "ubiquity_client/Ubiquity.hpp"
#include "ClientHandleImpl.hpp"
#include "debug.hpp"

static Ubiquity::ClientHandle::Impl s_invalidHandleImpl{ Ubiquity::INVALID_HANDLE };

Ubiquity::ClientHandle::ClientHandle(void) noexcept {
    this->pimpl = new Ubiquity::ClientHandle::Impl();
}

void Ubiquity::ClientHandle::close(void) noexcept {
    if (&s_invalidHandleImpl != this->pimpl) {
        DBG("Closing one handle.");
        delete this->pimpl;
        this->pimpl = &s_invalidHandleImpl;
    }
}

Ubiquity::StatusCode Ubiquity::ClientHandle::getStatusCode(void) noexcept {
    return this->pimpl->getStatusCode();
}

const char *Ubiquity::ClientHandle::getStatusString(void) noexcept {
    return this->pimpl->getStatusString();
}

int64_t Ubiquity::ClientHandle::getCreativeId(void) noexcept {
    return this->pimpl->getCreativeId();
}

int64_t Ubiquity::ClientHandle::getQueryId(void) noexcept {
    return this->pimpl->getQueryId();
}

const char *Ubiquity::ClientHandle::getModelId(void) noexcept {
    return this->pimpl->getModelId();
}

float Ubiquity::ClientHandle::getPrice(void) noexcept {
    return this->pimpl->getPrice();
}

float Ubiquity::ClientHandle::getProbability(void) noexcept {
    return this->pimpl->getProbability();
}

int32_t Ubiquity::ClientHandle::getPredictionType(void) noexcept {
    return this->pimpl->getPredictionType();
}

Ubiquity::StatusCode Ubiquity::ClientHandle::poll(void) noexcept {
    return this->pimpl->poll();
}

Ubiquity::StatusCode Ubiquity::ClientHandle::predict(void) noexcept {
    return this->pimpl->predict();
}

void Ubiquity::ClientHandle::putBoolean(const int8_t value) noexcept {
    this->pimpl->putBoolean(value);
}

void Ubiquity::ClientHandle::putCategorical(const int8_t value) noexcept {
    this->pimpl->putCategorical(value);
}

void Ubiquity::ClientHandle::putCreative(const int64_t value) noexcept {
    this->pimpl->putCreative(value);
}

void Ubiquity::ClientHandle::putNominal(const char *value) noexcept {
    this->pimpl->putNominal(value);
}

void Ubiquity::ClientHandle::putNumeric(const float value) noexcept {
    this->pimpl->putNumeric(value);
}

void Ubiquity::ClientHandle::startBidShading(const char *modelId, const int64_t queryId, const float bidPrice, const float bidFloor) noexcept {
    this->pimpl->startBidShading(modelId, queryId, bidPrice, bidFloor);
}

void Ubiquity::ClientHandle::startLineItem(const char *modelId, const int64_t queryId, const float maxCPM) noexcept {
    this->pimpl->startLineItem(modelId, queryId, maxCPM);
}

void Ubiquity::ClientHandle::startProbability(const char *modelId, const int64_t queryId) noexcept {
    this->pimpl->startProbability(modelId, queryId);
}

Ubiquity::StatusCode Ubiquity::ClientHandle::train(const char *modelId, int64_t queryId, int32_t level) noexcept {
    return this->pimpl->train(modelId, queryId, level);
}

Ubiquity::StatusCode Ubiquity::ClientHandle::cancel(const char *modelId, int64_t queryId) noexcept {
    return this->pimpl->cancel(modelId, queryId);
}
