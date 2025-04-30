#include "ClientHandleImpl.hpp"
#include "debug.hpp"
#include "Base32C.hpp"

#include <xxh3.h>

#include <atomic>

enum class Ubiquity::ClientHandle::Impl::MessageType : uint8_t {
    QUERY = 0x05,
    FEEDBACK = 0x07,
    CANCEL = 0x08,
};

static constexpr bool appUsesCreatives[Ubiquity::NUMBER_OF_APP_TYPES] = {
    false, // Bid shading
    false, // Probability
    true, // Line item
    true, // Action rate
};

Ubiquity::ClientHandle::Impl::Impl(void) {
    this->atomicBuffer.wrap(this->message.buffer, ClusterClient::MAXIMUM_TRANSMISSION_UNIT);
}

Ubiquity::ClientHandle::Impl::Impl(const Ubiquity::StatusCode statusCode) {
    this->statusCode = statusCode;
}

Ubiquity::ClientHandle::Impl::~Impl(void) {
}

Ubiquity::StatusCode Ubiquity::ClientHandle::Impl::getStatusCode(void) noexcept {
    return this->statusCode;
}

const char *Ubiquity::ClientHandle::Impl::getStatusString(void) noexcept {
    return Ubiquity::getStatusString(this->statusCode);
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
        case Ubiquity::APP_ACTION_RATE:
            return this->message.actionRatePrediction.price;
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
        case Ubiquity::APP_ACTION_RATE:
            break;
        case Ubiquity::APP_UNDEFINED:
            break;
    }
    return 0.f;
}

int32_t Ubiquity::ClientHandle::Impl::getPredictionType(void) noexcept {
    return static_cast<int32_t>(this->message.common.applicationId);
}

int8_t Ubiquity::ClientHandle::Impl::getBidStatus(void) noexcept {
  switch(this->message.common.applicationId) {
    case Ubiquity::APP_LINE_ITEM:
      return this->message.lineItemPrediction.price == 0 ? Ubiquity::NO_BID : Ubiquity::BID;
    case Ubiquity::APP_ACTION_RATE:
      return this->message.actionRatePrediction.price == 0 ? Ubiquity::NO_BID : Ubiquity::BID;
    default:
      return Ubiquity::BID;
  } 
}

Ubiquity::StatusCode Ubiquity::ClientHandle::Impl::poll(void) noexcept {
    static constexpr uint8_t MESSAGE_TYPE_PREDICTION = 0x06;
    static constexpr size_t expectedInputMessageSize[] {
        sizeof(Ubiquity::ClientHandle::Impl::BidShadingPrediction),
        sizeof(Ubiquity::ClientHandle::Impl::ProbabilityPrediction),
        sizeof(Ubiquity::ClientHandle::Impl::LineItemPrediction),
        sizeof(Ubiquity::ClientHandle::Impl::ActionRatePrediction),
    };

    this->errorOrigin = ErrorOrigin::POLL;
    if (__glibc_unlikely(this->clusterClient->statusCode != Ubiquity::SUCCESS)) {
        this->statusCode = this->clusterClient->statusCode;
        return this->statusCode;
    }

    this->state = HandleState::POLLED;

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
                        || applicationId >= static_cast<uint16_t>(Ubiquity::NUMBER_OF_APP_TYPES)
                        || expectedInputMessageSize[applicationId] != this->offset)) {
                DBG("Warning : invalid message received in poll.")
                break;
            }
            this->statusCode = Ubiquity::SUCCESS;
            return this->statusCode;
        }
    }
    this->statusCode = Ubiquity::NO_DATA;
    return this->statusCode;
}

inline Ubiquity::StatusCode Ubiquity::ClientHandle::Impl::checkPredictSetupErrors() {
    static constexpr const uint8_t statusCodePerErrorLevel[64] = {
        (uint8_t)Ubiquity::SUCCESS, // 0
        (uint8_t)Ubiquity::MISMATCHED_FEATURE_TYPE, 0, // 1
        (uint8_t)Ubiquity::MISSING_CREATIVES, 0, 0, 0, // 3
        (uint8_t)Ubiquity::MISPLACED_CREATIVE_ID, 0, 0, 0, 0, 0, 0, 0, // 7
        (uint8_t)Ubiquity::SCHEMA_UNAVAILABLE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 15
        (uint8_t)Ubiquity::INVALID_RESOURCE_ID, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 31
        (uint8_t)Ubiquity::INVALID_APP_FOR_MODEL, // 63
    };
    if (__glibc_unlikely(this->misplacedCreativeId)) {
        this->statusCode = Ubiquity::MISPLACED_CREATIVE_ID;
    } else if (__glibc_unlikely(this->tooManyCreatives)) {
        this->statusCode = Ubiquity::TOO_MANY_CREATIVES;
    } else {
        const uint8_t errorLevel =
            (HandleState::PREDICTING != this->state) * 63
            | (0 == this->message.common.modelId) * 31
            | (0 == this->schema.modelId) * 15
            | (((0 != this->schema.modelId) & (this->applicationId != this->schema.applicationId)) | (this->applicationId >= Ubiquity::NUMBER_OF_APP_TYPES)) * 63
            | ((0 < this->numCreative) & !appUsesCreatives[this->message.common.applicationId]) * 7
            | ((0 == this->numCreative) & appUsesCreatives[this->message.common.applicationId]) * 3
            | !!( (this->schema.featureTypeList[0] ^ this->featureTypeList[0])
                | (this->schema.featureTypeList[1] ^ this->featureTypeList[1])
                | (this->schema.featureTypeList[2] ^ this->featureTypeList[2])
                | (this->schema.featureTypeList[3] ^ this->featureTypeList[3])
                | (this->schema.featureTypeList[4] ^ this->featureTypeList[4])
                | (this->schema.featureTypeList[5] ^ this->featureTypeList[5])
                | (this->schema.featureTypeList[6] ^ this->featureTypeList[6])
                | (this->schema.featureTypeList[7] ^ this->featureTypeList[7]))
                | (this->featureOrdinal != this->schema.featureCount);
        this->statusCode = static_cast<Ubiquity::StatusCode>(statusCodePerErrorLevel[errorLevel]);
    }
    return this->statusCode;
}

Ubiquity::StatusCode Ubiquity::ClientHandle::Impl::predict(void) noexcept {
    prepareCommonHeader(this->encodedModelId, this->queryId, MessageType::QUERY);
    this->message.common.reserved.schemaVersion = 0;
    this->message.query.feedbackDelay = this->schema.feedbackDelayList[0];
    this->errorOrigin = ErrorOrigin::PREDICT;
    if (Ubiquity::SUCCESS != this->checkPredictSetupErrors()) {
        return this->statusCode;
    }
    if (Ubiquity::APP_LINE_ITEM == this->applicationId || Ubiquity::APP_ACTION_RATE == this->applicationId) {
        this->message.lineItemQuery.numCreative = this->numCreative;
    }
    this->statusCode = this->clusterClient->loadBalancer->sendApplicationMessage(
            this->message.common.modelId,
            this->message.common.queryId,
            this->atomicBuffer,
            this->offset,
            false
            );
    if (Ubiquity::SUCCESS == this->statusCode) {
        this->state = HandleState::PREDICT_SUCCEEDED;
        this->applicationId = Ubiquity::APP_UNDEFINED;
    }
    return this->statusCode;
}

inline void Ubiquity::ClientHandle::Impl::putFeature8(
        const uint8_t value,
        const ClusterClient::Schema::FeatureType featureType
        ) noexcept
{
    if (__glibc_likely(this->featureOrdinal < 256)) {
        this->atomicBuffer.putUInt8(this->offset, value);
        this->offset += sizeof(uint8_t);
        this->featureTypeList[(this->featureOrdinal & 0xE0) >> 5] |= static_cast<int64_t>(featureType) << ((this->featureOrdinal & 0x1F) * 2);
        this->featureOrdinal++;
    }
}

inline void Ubiquity::ClientHandle::Impl::putFeature32(
        const uint32_t value,
        const ClusterClient::Schema::FeatureType featureType
        ) noexcept
{
    if (__glibc_likely(this->featureOrdinal < 256)) {
        this->atomicBuffer.putInt32(this->offset, 0);
        this->offset += (-this->offset) & 3;
        this->atomicBuffer.putInt32(this->offset, value);
        this->offset += sizeof(uint32_t);
        this->featureTypeList[(this->featureOrdinal & 0xE0) >> 5] |= static_cast<int64_t>(featureType) << ((this->featureOrdinal & 0x1F) * 2);
        this->featureOrdinal++;
    }
}

void Ubiquity::ClientHandle::Impl::putBoolean(
        const int8_t value
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
        this->misplacedCreativeId = 1;
        return;
    }
    if (__glibc_unlikely(MAX_CREATIVE_COUNT <= this->numCreative)) {
        this->tooManyCreatives = 1;
        return;
    }
    ++this->numCreative;
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

inline void Ubiquity::ClientHandle::Impl::prepareCommonHeader(
        const char *const encodedModelId,
        const int64_t queryId,
        const MessageType messageType
        ) noexcept
{
    static constexpr uint8_t SERIALIZATION_VERSION = 0;

    Base32C::decode(&this->message.common.modelId, encodedModelId);
    if (this->message.common.modelId != static_cast<uint64_t>(this->schema.modelId)) {
        auto schemaCache = atomic_load(&ClusterClient::s_schemaCache);
        auto it = schemaCache->find(this->message.common.modelId);
        if (schemaCache->end() != it) {
            this->schema = it->second;
        } else {
            this->schema.modelId = 0;
        }
    }
    if (messageType == MessageType::QUERY && this->schema.applicationId == Ubiquity::APP_LINE_ITEM) {
      this->message.common.version = 0x01;
    } else {
      this->message.common.version = SERIALIZATION_VERSION;
    }
    this->message.common.type = static_cast<uint8_t>(messageType);
    this->message.common.applicationId = this->schema.applicationId;
    this->message.common.queryId = queryId ^ this->message.common.modelId;
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
        sizeof(ActionRateQueryMessage),
    };

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstringop-truncation"
    strncpy(this->encodedModelId, encodedModelId, 14);
    #pragma GCC diagnostic pop
    this->queryId = queryId;
    this->applicationId = applicationId;
    this->misplacedCreativeId = 0;
    this->tooManyCreatives = 0;
    this->numCreative = 0;
    this->featureTypeList[0] = 0;
    this->featureTypeList[1] = 0;
    this->featureTypeList[2] = 0;
    this->featureTypeList[3] = 0;
    this->featureTypeList[4] = 0;
    this->featureTypeList[5] = 0;
    this->featureTypeList[6] = 0;
    this->featureTypeList[7] = 0;
    this->offset = queryMessageSize[static_cast<size_t>(applicationId)];
    this->featureOrdinal = 0;
    this->state = HandleState::PREDICTING;
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
        const float maxCPM,
        const float bidFloor,
        const int32_t exposureCount
        ) noexcept
{
    prepareQueryHeader(encodedModelId, queryId, Ubiquity::APP_LINE_ITEM);
    this->message.lineItemQuery.maxCPM = maxCPM;
    this->message.lineItemQuery.bidFloor = bidFloor;
    this->message.lineItemQuery.exposureCount = exposureCount;
}

void Ubiquity::ClientHandle::Impl::startActionRate(
        const char *encodedModelId,
        const int64_t queryId,
        const float bidPrice
        ) noexcept
{
    prepareQueryHeader(encodedModelId, queryId, Ubiquity::APP_ACTION_RATE);
    this->message.actionRateQuery.bidPrice = bidPrice;
}

inline Ubiquity::StatusCode Ubiquity::ClientHandle::Impl::checkFeedbackSetupErrors() {
    static constexpr const uint8_t statusCodePerErrorLevel[8] = {
        (uint8_t)Ubiquity::SUCCESS, // 0
        (uint8_t)Ubiquity::INVALID_FEEDBACK_LEVEL, 0, // 1
        (uint8_t)Ubiquity::SCHEMA_UNAVAILABLE, 0, 0, 0, // 3
        (uint8_t)Ubiquity::INVALID_RESOURCE_ID, // 7
    };
    const uint8_t errorLevel =
        (0 == this->message.common.modelId) * 7
        | (0 == this->schema.modelId) * 3
        | (this->message.common.reserved.feedbackLevel >= this->schema.feedbackDelayCount);
    this->statusCode = static_cast<Ubiquity::StatusCode>(statusCodePerErrorLevel[errorLevel]);
    return this->statusCode;
}

Ubiquity::StatusCode Ubiquity::ClientHandle::Impl::train(
        const char *encodedModelId,
        int64_t queryId,
        int feedbackLevel
        ) noexcept
{
    prepareCommonHeader(encodedModelId, queryId, MessageType::FEEDBACK);
    this->message.common.reserved.feedbackLevel = feedbackLevel;
    this->message.feedback.feedbackDelay = this->schema.feedbackDelayList[feedbackLevel];
    this->message.feedback.nextFeedbackDelay = this->schema.feedbackDelayList[feedbackLevel + 1];
    this->state = HandleState::TRAINED;
    this->errorOrigin = ErrorOrigin::TRAIN;
    if (Ubiquity::SUCCESS != this->checkFeedbackSetupErrors()) {
        return this->statusCode;
    }
    if (0 == feedbackLevel) {
        switch (this->schema.applicationId) {
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
            case Ubiquity::APP_ACTION_RATE:
                this->message.actionRateFeedback.weight = 1.0;
                this->message.actionRateFeedback.executed = 1;
                break;
            case Ubiquity::APP_UNDEFINED:
                break;
        }
    } else {
        this->message.feedbackNextLevels.eventOccurred = 1;
    }

    static const size_t feedbackMessageSize[] = {
        sizeof(BidShadingFeedbackMessage),
        sizeof(ProbabilityFeedbackMessage),
        sizeof(LineItemFeedbackMessage),
        sizeof(ActionRateQueryMessage),
    };

    if (Ubiquity::SUCCESS == this->statusCode) {
        IGNORE("ClientHandle(" << this->clusterClient->clusterId
                << ") : Sending training feedback(" << this->message.common.queryId
                << ") for model(" << this->message.common.modelId << ")");
        this->statusCode = this->clusterClient->loadBalancer->sendApplicationMessage(
                this->message.common.modelId,
                this->message.common.queryId,
                this->atomicBuffer,
                feedbackMessageSize[static_cast<size_t>(this->schema.applicationId)],
                true
                );
    }
    return this->statusCode;
}

Ubiquity::StatusCode Ubiquity::ClientHandle::Impl::cancel(
        const char *encodedModelId,
        int64_t queryId
        ) noexcept
{
    prepareCommonHeader(encodedModelId, queryId, MessageType::CANCEL);
    this->message.common.reserved.feedbackLevel = 0;
    this->message.feedback.feedbackDelay = this->schema.feedbackDelayList[0];
    this->state = HandleState::CANCELED;
    this->errorOrigin = ErrorOrigin::CANCEL;
    if (Ubiquity::SUCCESS != this->checkFeedbackSetupErrors()) {
        return this->statusCode;
    }
    if (Ubiquity::SUCCESS == this->statusCode) {
        IGNORE("ClientHandle(" << this->clusterClient->clusterId
                << ") : Sending cancel feedback(" << this->message.common.queryId
                << ") for model(" << this->message.common.modelId << ")");
        this->statusCode = this->clusterClient->loadBalancer->sendApplicationMessage(
                this->message.common.modelId,
                this->message.common.queryId,
                this->atomicBuffer,
                sizeof(CancelMessage),
                true
                );
    }
    return this->statusCode;
}

const char *Ubiquity::ClientHandle::Impl::getDiagnosticString() noexcept {
    static constexpr const char *applicationTypeStrings[Ubiquity::NUMBER_OF_APP_TYPES]{
        "BID_SHADING",
        "PROBABILITY",
        "LINE_ITEM",
        "ACTION_RATE",
    };
    static constexpr const char *starterFunctionName[Ubiquity::NUMBER_OF_APP_TYPES]{
        "startBidShading",
        "startProbability",
        "startLineItem",
        "startActionRate",
    };

    uint8_t errorCount = 0;
    std::stringstream diagnosticString{};

    if (HandleState::READY == this->state && ErrorOrigin::PREDICT == this->errorOrigin) {
        diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": "
            << "\nYou did not call any query starter function to prepare the last call to `predict`, for instance `startLineItem` or `startProbability`."
            << "\nStarter functions initialize buffers and states for the query and allow you to provide additional parameters which are specific to their kind of prediction queries.";
    } else if (0 == this->message.common.modelId) {
        diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": "
            << "\nModel ID `" << this->encodedModelId << "` provided to `" << ErrorOrigin::functionName[this->errorOrigin] << "` is ill-formed so it could not be decoded.";
    } else if (__glibc_unlikely(0 == this->schema.modelId)) {
        diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": "
            << "\nThe schema corresponding to model " << this->encodedModelId << " has not been loaded yet.\nThis might be resolved after a few seconds (for the nodes to install the model if it's a new one and/or send the schema to your client),"
            << " unless you provided a well-formed model ID but it corresponds to no existing model."
            << "\nIn most cases, i.e. when you're certain the model IDs you provide correspond to existing models, you can simply ignore this error or loop until it disappears.";
    } else if (ErrorOrigin::TRAIN == this->errorOrigin || ErrorOrigin::CANCEL == this->errorOrigin) {
        if (__glibc_unlikely(this->message.common.reserved.feedbackLevel >= this->schema.feedbackDelayCount)) {
            diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": "
                << "The feedback level [" << this->message.common.reserved.feedbackLevel << "] you provided to `" << ErrorOrigin::functionName[this->errorOrigin]
                << "` exceeds model " << this->encodedModelId << " 's maximum feedback level [" << this->schema.feedbackDelayCount<< "].";
        }
    } else if (ErrorOrigin::PREDICT == this->errorOrigin) {
        Base32C::encode(this->encodedModelId, &this->message.common.modelId);

        if (HandleState::READY == this->state) {
            diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": "
                << "\nYou did not call any query starter function to prepare the last call to `predict`."
                << "\nModel " << this->encodedModelId << " is a " << applicationTypeStrings[this->schema.applicationId] << " model, so `" << starterFunctionName[this->schema.applicationId]
                << "` must be called before any feature/creative putting methods and before `predict`"
                << "\nIt initializes buffers and states for the query and allows you to provide some parameters which are specific to " << applicationTypeStrings[this->schema.applicationId] << " prediction queries.";
        } else if (HandleState::PREDICT_SUCCEEDED == this->state || Ubiquity::APP_UNDEFINED == this->applicationId) {
            diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": "
                << "\nYou called `predict` again after a successful previous call to `predict` without calling any query starter function first."
                << "\nWhen a call to `predict` is successful, the whole query parameters are consumed to avoid query duplication, so you need to initialize a whole new query and put new features/creatives into it."
                << "\nModel " << this->encodedModelId << " is a " << applicationTypeStrings[this->schema.applicationId] << " model, so you should call `" << starterFunctionName[this->schema.applicationId] << "` for that purpose.";
        } else if (HandleState::originFunctionName[this->state]) {
            diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": "
                << "\nThough you *did* call `" << starterFunctionName[this->applicationId] << "` first, you then called `" << HandleState::originFunctionName[this->state] << "`"
                << " on the same client handle before you finally called `predict`; This call to `" << HandleState::originFunctionName[this->state] << "` invalidated everything `" << starterFunctionName[this->applicationId] << "` had initialized."
                << "\nAs a general rule, you must never call `predict`, `poll`, `train` and/or `cancel`"
                << " concurrently using the same client handle, though it's fine to call them sequentially."
                << "\nIf you need to call them concurrently, consider getting new client handles by calling `getClientHandle` for each concurrent usage."
                << "\n/!\\ Beware that passing a client handle to another function or thread does *not* create a new client handle, only a shallow copy of the passed one sharing the same state !";
        } else if (this->schema.applicationId != this->applicationId) {
            diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": "
                << "\nYou called `" << starterFunctionName[this->applicationId] << "` to start preparing a prediction query for model " << this->encodedModelId
                << ", but this model is of type " << applicationTypeStrings[this->schema.applicationId] << "."
                << "\nDid you mean to call `" << starterFunctionName[this->schema.applicationId] << "` instead ?";
        }
        if (0 < this->numCreative && !appUsesCreatives[this->schema.applicationId]) {
            diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": "
                << "\nCreatives have been put into a prediction query for model " << this->encodedModelId << " of type " << applicationTypeStrings[this->schema.applicationId] << " which have no use for creatives.";
        } else if (0 == this->numCreative && appUsesCreatives[this->schema.applicationId]) {
            diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": "
                << "\nNo creatives have been put into a prediction query for model " << this->encodedModelId << " of type " << applicationTypeStrings[this->schema.applicationId] << " which needs at least one creative to function.";
        }
        if (this->misplacedCreativeId) {
            diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": "
                << "\n`putCreative` was called after some feature putting function (for instance `putNumeric`)."
                << "\nCreatives should be put *before* features because of the way queries are serialized to optimize performance.";
        }
        if (this->tooManyCreatives) {
            diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": "
                << "\nToo many creatives were put into the prediction query.";
        }
        if ((this->schema.featureTypeList[0] ^ this->featureTypeList[0])
            | (this->schema.featureTypeList[1] ^ this->featureTypeList[1])
            | (this->schema.featureTypeList[2] ^ this->featureTypeList[2])
            | (this->schema.featureTypeList[3] ^ this->featureTypeList[3])
            | (this->schema.featureTypeList[4] ^ this->featureTypeList[4])
            | (this->schema.featureTypeList[5] ^ this->featureTypeList[5])
            | (this->schema.featureTypeList[6] ^ this->featureTypeList[6])
            | (this->schema.featureTypeList[7] ^ this->featureTypeList[7])) {
            const uint8_t minFeatureOrdinal = this->featureOrdinal < this->schema.featureCount ? this->featureOrdinal : this->schema.featureCount;
            const uint8_t maxFeatureOrdinal = this->featureOrdinal > this->schema.featureCount ? this->featureOrdinal : this->schema.featureCount;
            const bool hasLessFeatures = this->featureOrdinal < this->schema.featureCount;
            for (uint8_t featureOrdinal = 0; featureOrdinal < maxFeatureOrdinal; ++featureOrdinal) {
                if (minFeatureOrdinal == featureOrdinal) {
                    diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": " << (hasLessFeatures ? "Less" : "More") << " features were put than expected by model " << this->encodedModelId << " :";
                } else if (0 == featureOrdinal) {
                    diagnosticString << "\n\nError #" << std::to_string(++errorCount) << ": " << "Some feature types in the query do not match the schema for model " << this->encodedModelId << " :";
                }

                const uint8_t chunkIndex = featureOrdinal >> 5;
                const uint8_t typeIndex = (31 & featureOrdinal) << 1;
                const uint8_t expectedType = 3 & (this->schema.featureTypeList[chunkIndex] >> typeIndex);
                const uint8_t actualType = 3 & (this->featureTypeList[chunkIndex] >> typeIndex);
                if (expectedType != actualType) {
                    if (minFeatureOrdinal > featureOrdinal) {
                        diagnosticString << "\n    Feature #" << std::to_string(featureOrdinal) << " should be " << ClusterClient::Schema::featureTypeStrings[expectedType] << " instead of " << ClusterClient::Schema::featureTypeStrings[actualType];
                    } else if (hasLessFeatures) {
                        diagnosticString << "\n    Missing feature #" << std::to_string(featureOrdinal) << " with expected type " << ClusterClient::Schema::featureTypeStrings[expectedType];
                    } else {
                        diagnosticString << "\n    Extra feature #" << std::to_string(featureOrdinal) << " having type " << ClusterClient::Schema::featureTypeStrings[actualType];
                    }
                }
            }
            if (0 == this->featureOrdinal) {
                diagnosticString << "\n\nIt seems like you did not put any feature at all. If you did, did you call `" << starterFunctionName[this->schema.applicationId] << "` *before* you put the features ?";
            }
        }
    }
    this->diagnosticString = "\n##### Diagnostic for last call to `";
    this->diagnosticString += ErrorOrigin::functionName[this->errorOrigin];
    this->diagnosticString += "` : ";
    this->diagnosticString += std::to_string(errorCount);
    this->diagnosticString += " error(s) detected. #####";
    this->diagnosticString += diagnosticString.str();
    this->diagnosticString += "\n\n##### End of error diagnostic #####\n";
    return this->diagnosticString.c_str();
}
