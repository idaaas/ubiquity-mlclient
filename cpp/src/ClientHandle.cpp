#include "ubiquity_client/Ubiquity.hpp"
#include "ClientHandleImpl.hpp"

Ubiquity::ClientHandle::ClientHandle(std::shared_ptr<Ubiquity::ClientHandle::Impl> &&pimpl) noexcept
    : pimpl(std::move(pimpl)) {
}

void Ubiquity::ClientHandle::close(void) noexcept {
    this->pimpl.reset();
}

Ubiquity::ErrorCode::Type Ubiquity::ClientHandle::getLastError(void) noexcept {
    return this->pimpl->getErrorCode();
}

const char *Ubiquity::ClientHandle::getLastErrorString(void) noexcept {
    return this->pimpl->getErrorString();
}

const char *Ubiquity::ClientHandle::getClusterId() noexcept {
    return this->pimpl->getClusterId();
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

Ubiquity::Application::Type Ubiquity::ClientHandle::getPredictionType(void) noexcept {
    return this->pimpl->getPredictionType();
}

const char *Ubiquity::ClientHandle::getStatus(void) noexcept {
    return this->pimpl->getStatus();
}

Ubiquity::ErrorCode::Type Ubiquity::ClientHandle::poll(void) noexcept {
    return this->pimpl->poll();
}

Ubiquity::ErrorCode::Type Ubiquity::ClientHandle::predict(void) noexcept {
    return this->pimpl->predict();
}

void Ubiquity::ClientHandle::putBoolean(const bool value) noexcept {
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

void Ubiquity::ClientHandle::setBidPrice(const float bidPrice) noexcept {
    this->pimpl->setBidPrice(bidPrice);
}

void Ubiquity::ClientHandle::setCreativeId(const int64_t creativeId) noexcept {
    this->pimpl->setCreativeId(creativeId);
}

void Ubiquity::ClientHandle::setConversionProbability(const float conversionProbability) noexcept {
    this->pimpl->setConversionProbability(conversionProbability);
}

void Ubiquity::ClientHandle::setWinningProbability(const float winningProbability) noexcept {
    this->pimpl->setWinningProbability(winningProbability);
}

void Ubiquity::ClientHandle::startBidShading(const char *modelId, const int64_t queryId, const float bidPrice, const float bidFloor) noexcept {
    this->pimpl->startBidShading(modelId, queryId, bidPrice, bidFloor);
}

void Ubiquity::ClientHandle::startLineItem(const char *modelId, const int64_t queryId, const float maxCPM) noexcept {
    this->pimpl->startLineItem(modelId, queryId, maxCPM);
}

void Ubiquity::ClientHandle::startProbabilityEstimation(const char *modelId, const int64_t queryId) noexcept {
    this->pimpl->startProbabilityEstimation(modelId, queryId);
}

Ubiquity::ErrorCode::Type Ubiquity::ClientHandle::train(const char *modelId, int64_t queryId, int level) noexcept {
    return this->pimpl->train(modelId, queryId, level);
}
