#include "ClientHandleImpl.hpp"
#include "debug.hpp"
#include "Base32C.hpp"

#include <xxh3.h>

#include <atomic>

Ubiquity::ClientHandle::Impl::Impl(void) {
    this->atomicBuffer.wrap(this->message.buffer, ClusterClient::MAXIMUM_TRANSMISSION_UNIT);
}

Ubiquity::ClientHandle::Impl::~Impl(void) {
}

Ubiquity::StatusCode Ubiquity::ClientHandle::Impl::getStatusCode(void) noexcept {
    return this->statusCode;
}

const char *Ubiquity::ClientHandle::Impl::getStatusString(void) noexcept {
    return Ubiquity::getStatusString(this->statusCode);
}

const char *Ubiquity::ClientHandle::Impl::getClusterId(void) noexcept {
    return this->clusterClient->clusterId.c_str();
}

const char *Ubiquity::ClientHandle::Impl::getModelId(void) noexcept {
    Base32C::encode(this->encodedModelId, &this->message.common.modelId);
    return this->encodedModelId;
}

int64_t Ubiquity::ClientHandle::Impl::getQueryId(void) noexcept {
    return this->message.common.queryId ^ this->message.common.modelId;
}

int64_t Ubiquity::ClientHandle::Impl::getCreativeId(void) noexcept {
    return this->message.lineItemPrediction.creativeId;
}

float Ubiquity::ClientHandle::Impl::getPrice(void) noexcept {
    switch(this->message.common.applicationId) {
        case Ubiquity::APP_BID_SHADING:
            return this->message.bidShadingPrediction.price;
        case Ubiquity::APP_LINE_ITEM:
            return this->message.lineItemPrediction.price;
        case Ubiquity::APP_PROBABILITY:
            break;
        case Ubiquity::APP_UNDEFINED:
            break;
    }
    return 0.f;
}

float Ubiquity::ClientHandle::Impl::getProbability(void) noexcept {
    switch(this->message.common.applicationId) {
        case Ubiquity::APP_BID_SHADING:
            return this->message.bidShadingPrediction.probability;
        case Ubiquity::APP_LINE_ITEM:
            return this->message.lineItemPrediction.probability;
        case Ubiquity::APP_PROBABILITY:
            return this->message.probabilityPrediction.probability;
        case Ubiquity::APP_UNDEFINED:
            break;
    }
    return 0.f;
}

int32_t Ubiquity::ClientHandle::Impl::getPredictionType(void) noexcept {
    return static_cast<int32_t>(this->message.common.applicationId);
}

const char *Ubiquity::ClientHandle::Impl::getStatus(void) noexcept {
    return this->clusterClient->getStatus().c_str();
}

static constexpr uint8_t MESSAGE_TYPE_PREDICTION = 0x06;

static size_t expectedInputMessageSize[] {
    sizeof(Ubiquity::ClientHandle::Impl::BidShadingPrediction),
    sizeof(Ubiquity::ClientHandle::Impl::ProbabilityPrediction),
    sizeof(Ubiquity::ClientHandle::Impl::LineItemPrediction),
};

Ubiquity::StatusCode Ubiquity::ClientHandle::Impl::poll(void) noexcept {
    if (__glibc_unlikely(this->clusterClient->statusCode != Ubiquity::SUCCESS)) {
        this->statusCode = this->clusterClient->statusCode;
        return this->statusCode;
    }
    auto receiver = [this](
            aeron::concurrent::AtomicBuffer &receptionBuffer,
            aeron::util::index_t offset,
            aeron::util::index_t length,
            aeron::concurrent::logbuffer::Header &header
        ) {
            receptionBuffer.getBytes(offset, this->message.buffer, length);
            this->offset = static_cast<size_t>(length);
        };

    auto predSubList = std::atomic_load(&this->clusterClient->predSubList);
    for (auto &predSub : *predSubList) {
        if (predSub->poll(receiver, 1) > 0) {
            const uint16_t applicationId = static_cast<uint16_t>(this->message.common.applicationId);
            if (__glibc_unlikely(MESSAGE_TYPE_PREDICTION != this->message.common.type
                        || applicationId >= static_cast<uint16_t>(Ubiquity::APP_UNDEFINED)
                        || expectedInputMessageSize[applicationId] != this->offset)) {
                break;
            } else {
                this->statusCode = Ubiquity::SUCCESS;
            }
            return this->statusCode;
        }
    }
    this->statusCode = Ubiquity::NO_DATA;
    return this->statusCode;
}

Ubiquity::StatusCode Ubiquity::ClientHandle::Impl::predict(void) noexcept {
    if (__glibc_unlikely(Ubiquity::APP_LINE_ITEM == this->message.common.applicationId
                && 0 == this->message.lineItemQuery.numCreative)) {
        this->statusCode = Ubiquity::MISSING_CREATIVES;
        return this->statusCode;
    }
    if (__glibc_likely(this->statusCode == Ubiquity::SUCCESS || this->statusCode == Ubiquity::NO_COMPATIBLE_NODE)) {
        IGNORE("ClientHandle(" << this->clusterClient->clusterId
                << ") : Sending prediction query(" << this->message.common.queryId
                << ") for model(" << this->message.common.modelId << ")");
        //hexDump(this->atomicBuffer, this->offset);
        this->statusCode = this->clusterClient->loadBalancer->sendApplicationMessage(
                this->message.common.modelId,
                this->message.common.queryId,
                this->atomicBuffer,
                this->offset,
                false
                );
    }
    return this->statusCode;
}

inline bool Ubiquity::ClientHandle::Impl::nextFeatureTypeMatches(
        const ClusterClient::Schema::FeatureType featureType
        ) noexcept
{
    const auto &features = this->schema.features;
    if (__glibc_unlikely(features.size() <= this->featureOrdinal || features[this->featureOrdinal] != featureType)) {
        if (Ubiquity::SUCCESS == this->statusCode) {
            this->statusCode = Ubiquity::MISMATCHED_FEATURE_TYPE;
        }
        return false;
    }
    return true;
}

inline void Ubiquity::ClientHandle::Impl::putFeature8(
        const uint8_t value,
        const ClusterClient::Schema::FeatureType featureType
        ) noexcept
{
    if (__glibc_likely(this->nextFeatureTypeMatches(featureType))) {
        this->atomicBuffer.putUInt8(this->offset, value);
        this->offset += sizeof(uint8_t);
        this->featureOrdinal++;
    }
}

inline void Ubiquity::ClientHandle::Impl::putFeature32(
        const uint32_t value,
        const ClusterClient::Schema::FeatureType featureType
        ) noexcept
{
    if (__glibc_likely(this->nextFeatureTypeMatches(featureType))) {
        this->atomicBuffer.putInt32(this->offset, 0);
        this->offset += (-this->offset) & 3;
        this->atomicBuffer.putInt32(this->offset, value);
        this->offset += sizeof(uint32_t);
        this->featureOrdinal++;
    }
}

void Ubiquity::ClientHandle::Impl::putBoolean(
        const bool value
        ) noexcept
{
    return this->putFeature8(value, ClusterClient::Schema::FeatureType::BOOLEAN);
}

void Ubiquity::ClientHandle::Impl::putCategorical(
        const int8_t value
        ) noexcept
{
    return this->putFeature8(value, ClusterClient::Schema::FeatureType::CATEGORICAL);
}

void Ubiquity::ClientHandle::Impl::putCreative(
        const int64_t creativeId
        ) noexcept
{
    static constexpr int32_t MAX_CREATIVE_COUNT = 20;

    if (__glibc_unlikely(this->featureOrdinal > 0)) {
        this->statusCode = Ubiquity::MISPLACED_CREATIVE_ID;
        return;
    }
    if (__glibc_unlikely(MAX_CREATIVE_COUNT <= this->message.lineItemQuery.numCreative)) {
        this->statusCode = Ubiquity::TOO_MANY_CREATIVES;
        return;
    }
    this->atomicBuffer.getAndAddInt32(offsetof(LineItemQueryMessage, numCreative), 1);
    this->atomicBuffer.putInt64(this->offset, creativeId);
    this->offset += sizeof(uint64_t);
}

void Ubiquity::ClientHandle::Impl::putNominal(
        const char *value
        ) noexcept
{
    this->putFeature32(
            XXH3_64bits(value, strlen(value)),
            ClusterClient::Schema::FeatureType::NOMINAL);
}

void Ubiquity::ClientHandle::Impl::putNumeric(
        const float value
        ) noexcept
{
    const auto *const val = reinterpret_cast<const void*>(&value);
    this->putFeature32(
            *reinterpret_cast<const int32_t*>(val),
            ClusterClient::Schema::FeatureType::NUMERIC);
}

void Ubiquity::ClientHandle::Impl::setBidPrice(
        const float bidPrice
        ) noexcept
{
    this->message.bidShadingFeedback.bidPrice = bidPrice;
}

void Ubiquity::ClientHandle::Impl::setCreativeId(
        const int64_t creativeId
        ) noexcept
{
    this->message.lineItemFeedback.creativeId = creativeId;
}

void Ubiquity::ClientHandle::Impl::setConversionProbability(
        const float conversionProbability
        ) noexcept
{
    this->message.lineItemFeedback.conversionProba = conversionProbability;
}

void Ubiquity::ClientHandle::Impl::setWinningProbability(
        const float winningProbability
        ) noexcept
{
    this->message.bidShadingFeedback.proba = winningProbability;
}

enum class Ubiquity::ClientHandle::Impl::MessageType : uint8_t {
    QUERY = 0x05,
    FEEDBACK = 0x07,
};

inline Ubiquity::StatusCode Ubiquity::ClientHandle::Impl::getSchema(
        const int64_t modelId
        ) noexcept
{
    auto schemaCache = atomic_load(&ClusterClient::s_schemaCache);
    auto it = schemaCache->find(modelId);
    if (__glibc_unlikely(schemaCache->end() == it)) {
        return Ubiquity::SCHEMA_UNAVAILABLE;
    }
    this->schema = it->second;
    return Ubiquity::SUCCESS;
}

static constexpr uint8_t SERIALIZATION_VERSION = 0;

inline void Ubiquity::ClientHandle::Impl::prepareCommonHeader(
        const int64_t modelId,
        const int64_t queryId,
        const Ubiquity::ApplicationType applicationId,
        const MessageType messageType
        ) noexcept
{
    this->statusCode = this->getSchema(modelId);
    if (__glibc_unlikely(Ubiquity::SUCCESS != this->statusCode)) {
        return;
    }
    if (__glibc_unlikely(applicationId != this->schema.applicationId)) {
        this->statusCode = Ubiquity::INVALID_APP_FOR_MODEL;
        return;
    }
    this->featureOrdinal = 0;
    this->message.common.version = SERIALIZATION_VERSION;
    this->message.common.type = static_cast<uint8_t>(messageType);
    this->message.common.applicationId = applicationId;
    this->message.common.modelId = modelId;
    this->message.common.queryId = queryId ^ modelId;
    this->statusCode = Ubiquity::SUCCESS;
}


inline void Ubiquity::ClientHandle::Impl::prepareQueryHeader(
        const char *const encodedModelId,
        const int64_t queryId,
        const Ubiquity::ApplicationType applicationId
        ) noexcept
{
    static const size_t queryMessageSize[] = {
        sizeof(BidShadingQueryMessage),
        sizeof(QueryMessage),
        sizeof(LineItemQueryMessage),
    };

    uint64_t modelId;
    if (__glibc_unlikely(!Base32C::decode(&modelId, encodedModelId))) {
        this->statusCode = Ubiquity::INVALID_RESOURCE_ID;
        return;
    }

    prepareCommonHeader(modelId, queryId, applicationId, MessageType::QUERY);
    this->message.common.reserved.schemaVersion = 0;
    this->message.query.feedbackDelay = this->schema.feedbackDelays[0];
    this->offset = queryMessageSize[static_cast<size_t>(applicationId)];
}

void Ubiquity::ClientHandle::Impl::startBidShading(
        const char *encodedModelId,
        const int64_t queryId,
        const float bidPrice,
        const float bidFloor
        ) noexcept
{
    prepareQueryHeader(encodedModelId, queryId, Ubiquity::APP_BID_SHADING);
    this->message.bidShadingQuery.bidFloor = bidFloor;
    this->message.bidShadingQuery.bidPrice = bidPrice;
}

void Ubiquity::ClientHandle::Impl::startProbability(
        const char *encodedModelId,
        const int64_t queryId
        ) noexcept
{
    prepareQueryHeader(encodedModelId, queryId, Ubiquity::APP_PROBABILITY);
}

void Ubiquity::ClientHandle::Impl::startLineItem(
        const char *encodedModelId,
        const int64_t queryId,
        const float maxCPM
        ) noexcept
{
    prepareQueryHeader(encodedModelId, queryId, Ubiquity::APP_LINE_ITEM);
    this->message.lineItemQuery.maxCPM = maxCPM;
    this->message.lineItemQuery.numCreative = 0;
}

void Ubiquity::ClientHandle::Impl::prepareFeedbackHeader(
        const uint64_t modelId,
        const int64_t queryId,
        const Ubiquity::ApplicationType applicationId,
        const uint16_t feedbackLevel
        ) noexcept
{
    prepareCommonHeader(modelId, queryId, applicationId, MessageType::FEEDBACK);
    this->message.common.reserved.feedbackLevel = feedbackLevel;
    if (__glibc_unlikely(feedbackLevel + 1u >= this->schema.feedbackDelays.size())) {
        this->statusCode = Ubiquity::INVALID_FEEDBACK_LEVEL;
        return;
    }
    this->message.feedback.feedbackDelay = this->schema.feedbackDelays[feedbackLevel];
    this->message.feedback.nextFeedbackDelay = this->schema.feedbackDelays[feedbackLevel + 1];
}

Ubiquity::StatusCode Ubiquity::ClientHandle::Impl::train(
        const char *encodedModelId,
        int64_t queryId,
        int level
        ) noexcept
{
    static const size_t feedbackMessageSize[] = {
        sizeof(BidShadingFeedbackMessage),
        sizeof(ProbabilityFeedbackMessage),
        sizeof(LineItemFeedbackMessage),
    };

    if (__glibc_unlikely(Ubiquity::SUCCESS != this->statusCode)) {
        return this->statusCode;
    }

    uint64_t modelId;
    if (__glibc_unlikely(!Base32C::decode(&modelId, encodedModelId))) {
        this->statusCode = Ubiquity::INVALID_RESOURCE_ID;
        return this->statusCode;
    }
    this->statusCode = this->getSchema(modelId);
    if (__glibc_unlikely(Ubiquity::SUCCESS != this->statusCode)) {
        return this->statusCode;
    }
    const Ubiquity::ApplicationType feedbackType = this->schema.applicationId;
    if (0 == level) {
        switch (feedbackType) {
            case Ubiquity::APP_BID_SHADING:
                this->message.bidShadingFeedback.weight = 1.0;
                this->message.bidShadingFeedback.executed = 1;
                break;
            case Ubiquity::APP_PROBABILITY:
                this->message.probabilityFeedback.weight = 1.0;
                this->message.probabilityFeedback.executed = 1;
                break;
            case Ubiquity::APP_LINE_ITEM:
                this->message.lineItemFeedback.weight = 1.0;
                this->message.lineItemFeedback.executed = 1;
                break;
            case Ubiquity::APP_UNDEFINED:
                break;
        }
    } else {
        this->message.feedbackNextLevels.eventOccurred = 1;
    }
    prepareFeedbackHeader(modelId, queryId, feedbackType, level);
    if (Ubiquity::SUCCESS == this->statusCode) {
        IGNORE("ClientHandle(" << this->clusterClient->clusterId
                << ") : Sending training feedback(" << this->message.common.queryId
                << ") for model(" << this->message.common.modelId << ")");
        this->statusCode = this->clusterClient->loadBalancer->sendApplicationMessage(
                this->message.common.modelId,
                this->message.common.queryId,
                this->atomicBuffer,
                feedbackMessageSize[static_cast<size_t>(feedbackType)],
                true
                );
    }
    return this->statusCode;
}
