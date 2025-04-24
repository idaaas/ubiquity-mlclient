#include "NodeClient.hpp"
#include "ClusterClient.hpp"
#include "debug.hpp"

#include <ChannelUriStringBuilder.h>
#include <arpa/inet.h>
/*
 * Values and types used only in this file
 */

namespace Period {
    static constexpr auto CON_TIMEOUT = std::chrono::milliseconds(3000);
    static constexpr auto RETRY_COOLDOWN = std::chrono::milliseconds(1000);
    static constexpr auto DC_TIMEOUT = std::chrono::milliseconds(1000);
    static constexpr auto REQUEST = std::chrono::milliseconds(200);
    static constexpr auto HEARTBEAT = std::chrono::milliseconds(1000);
}

namespace Port {
    static constexpr uint16_t ENDPOINT = 2000;
    static constexpr uint16_t CONTROL = 2001;
}

namespace StreamId
{
    static constexpr int32_t PROTOCOL = 1;
    static constexpr int32_t PROTOCOL_MDC = 1;
    static constexpr int32_t PRED = 2;
    static constexpr int32_t PRED_MDC = 1;
    static constexpr int32_t FEEDBACK = 3;
}

namespace State {
    enum Value : uint8_t {
        UNBOUND = 0,
        DISCONNECTED,
        REQUESTING_TOPOLOGY,
        CONNECTING_PREDICTION,
        WAITING_BEFORE_RETRY,
        REGISTERED,
        AWAITING_PRED_CONNECTION,
        PRED_CONNECTED,
        CLIENT_READY,
        LOSING_CONNECTION,
        DISCONNECTING,
        ANY,
        COUNT,
    };
    static const char *const strings[COUNT] = {
        "UNBOUND",
        "DISCONNECTED",
        "REQUESTING_TOPOLOGY",
        "CONNECTING_PREDICTION",
        "WAITING_BEFORE_RETRY",
        "REGISTERED",
        "AWAITING_PRED_CONNECTION",
        "PRED_CONNECTED",
        "CLIENT_READY",
        "LOSING_CONNECTION",
        "DISCONNECTING",
        "ANY",
    };
}

namespace Output {
    enum Value : uint32_t {
        REGISTRATION_REQUEST = 0x1,
        DISCONNECTION_REQUEST = 0x5,
        HEARTBEAT = 0x6,
        SERVER_OPEN_REQUEST = 0x7,
        SERVER_READY_REQUEST = 0x8,
        TOPOLOGY_REQUEST = 0xf, // TODO move this elsewhere, it should be in an other type
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
        "SERVER_OPEN_REQUEST",
        "SERVER_READY_REQUEST",
        "UNDEFINED",
        "UNDEFINED",
        "UNDEFINED",
        "UNDEFINED",
        "UNDEFINED",
        "UNDEFINED",
        "TOPOLOGY_REQUEST",
    };
}

namespace MessageType {
    enum Value : uint8_t {
        UNDEFINED = 0x00,
        REGISTRATION_REQUEST = 0x01,
        REGISTRATION_CONFIRMATION = 0x02,
        REGISTRATION_ERROR = 0x03,
        SCHEMA = 0x04,
        QUERY = 0x05,
        PREDICTION = 0x06,
        FEEDBACK = 0x07,
        SERVER_READY = 0x0a,
        SERVER_OPENED = 0x0c,
        TOPOLOGY_UPDATE = 0x0d,
        TOPOLOGY_REQUEST = 0x0f,
        TOPOLOGY_RESPONSE = 0x10,
        DISCONNECTION_CONFIRMATION = 0x11,
        MODEL_ADD = 0x14,
        MODEL_REMOVE = 0x15,
        NONE,
        COUNT,
    };
    static const char *const strings[COUNT] = {
        "UNDEFINED",
        "REGISTRATION_REQUEST",
        "REGISTRATION_CONFIRMATION",
        "REGISTRATION_ERROR",
        "SCHEMA",
        "QUERY",
        "PREDICTION",
        "FEEDBACK",
        "RESERVED_08",
        "RESERVED_09",
        "SERVER_READY",
        "RESERVED_0B",
        "SERVER_OPENED",
        "TOPOLOGY_UPDATE",
        "RESERVED_0E",
        "TOPOLOGY_REQUEST",
        "TOPOLOGY_RESPONSE",
        "DISCONNECTION_CONFIRMATION",
        "RESERVED_12",
        "RESERVED_13",
        "MODEL_ADD",
        "MODEL_REMOVE",
        "NONE",
    };
    static bool isGlobalMessage[COUNT] {
        false, // UNDEFINED,
        false, // REGISTRATION_REQUEST,
        false, // REGISTRATION_CONFIRMATION,
        false, // REGISTRATION_ERROR,
        false, // SCHEMA,
        false, // QUERY,
        false, // PREDICTION,
        false, // FEEDBACK,
        false, // UNDEFINED,
        false, // UNDEFINED,
        false, // SERVER_READY,
        false, // UNDEFINED,
        false, // SERVER_OPENED,
        true, // TOPOLOGY_UPDATE,
        false, // UNDEFINED,
        false, // TOPOLOGY_REQUEST,
        true, // TOPOLOGY_RESPONSE,
        false, // DISCONNECTION_CONFIRMATION,
        false, // UNDEFINED,
        false, // UNDEFINED,
        true, // MODEL_ADD,
        true, // MODEL_REMOVE,
        false, // NONE,
    };
}

struct __attribute__((packed)) ControlMessageBase {
    uint8_t version;
    MessageType::Value type;
    uint16_t clientId;
} __attribute__((packed));

struct RegistrationErrorMessage {
    ControlMessageBase _;
    uint64_t correlationIdHigh;
    uint64_t correlationIdLow;
    uint32_t code;
} __attribute__((packed));

struct RegistrationResponseMessage {
    ControlMessageBase _;
    uint64_t correlationIdHigh;
    uint64_t correlationIdLow;
    uint16_t controlPort;
    uint16_t clientId;
} __attribute__((packed));

enum class UpdateOperation : uint16_t {
    REMOVE = 0,
    ADD,
    SET,
};

struct __attribute__((packed)) TopologyUpdateMessage {
    ControlMessageBase _;
    UpdateOperation operation;
    uint16_t nodeCount;
    uint32_t nodeIpList[1];
} __attribute__((packed));

struct ModelAddMessage {
    ControlMessageBase _;
    uint32_t nodeIp;
    int64_t modelId;
    int16_t applicationId;
    uint16_t feedbackDelaysCount;
    int32_t feedbackDelays[1];
} __attribute__((packed));

struct ModelRemoveMessage {
    ControlMessageBase _;
    uint32_t nodeIp;
    int64_t modelId;
} __attribute__((packed));

struct RegistrationMessage {
    ControlMessageBase _;
    uint64_t correlationIdHigh;
    uint64_t correlationIdLow;
    Output::Value output;
} __attribute__((packed));

union ControlMessage {
    uint8_t buffer[1];
    ControlMessageBase common;
    RegistrationErrorMessage registrationError;
    RegistrationResponseMessage registrationResponse;
    TopologyUpdateMessage topologyUpdate;
    ModelAddMessage modelAdd;
    ModelRemoveMessage modelRemove;
    RegistrationMessage registration;
};

struct {
    MessageType::Value type;
    size_t size;
} expectedInputMessageSize[] = {
    MessageType::REGISTRATION_REQUEST,          sizeof(RegistrationMessage),
    MessageType::REGISTRATION_CONFIRMATION,     sizeof(RegistrationResponseMessage),
    MessageType::REGISTRATION_ERROR,            sizeof(RegistrationErrorMessage),
    MessageType::SERVER_READY,                  sizeof(ControlMessageBase),
    MessageType::SERVER_OPENED,                 sizeof(ControlMessageBase),
    MessageType::TOPOLOGY_UPDATE,               sizeof(TopologyUpdateMessage),
    MessageType::MODEL_ADD,                     sizeof(ModelAddMessage),
    MessageType::MODEL_REMOVE,                  sizeof(ModelRemoveMessage),
    MessageType::TOPOLOGY_REQUEST,              sizeof(ControlMessageBase),
    MessageType::DISCONNECTION_CONFIRMATION,    sizeof(ControlMessageBase),
};

/*
 * Finite State Machine controling connection of a NodeClient to its remote Node
 */

static bool beforeUnbound(NodeClient &nc) noexcept {
    return true;
}

static constexpr uint16_t CLIENT_ID_BROADCAST = __SHRT_MAX__;
static constexpr uint16_t CLIENT_ID_UNSET = __UINT16_MAX__;

static inline std::shared_ptr<ClusterClient::PredSubList> getPredSubListCopy(NodeClient &nc, const bool removeNode) noexcept {
    auto newPredSubList = std::make_shared<std::vector<std::shared_ptr<aeron::Subscription>>>();
    for (const auto &predSub : *nc.clusterClient->predSubList) {
        if (!removeNode || nc.predSub != predSub) {
            newPredSubList->emplace_back(predSub);
        }
    }
    return newPredSubList;
}

static bool beforeDisconnected(NodeClient &nc) noexcept {
    nc.clusterClient->loadBalancer->removeNodeForAllModels(nc.ip);
    if (State::DISCONNECTED != nc.state && State::UNBOUND != nc.state) {
        --nc.clusterClient->nonDisconnectedNcCount;
    }

    // New list of predSubs without current NodeClient.
    const auto &newPredSubList = getPredSubListCopy(nc, true);
    std::atomic_store(&nc.clusterClient->predSubList, newPredSubList);

    if (nc.controlPub) {
        nc.controlPub->close();
        nc.controlPub = nullptr;
    }
    if (nc.controlSub) {
        nc.controlSub->closeAndRemoveImages();
        nc.controlSub = nullptr;
    }
    if (nc.predPubs[0]) {
        nc.predPubs[0]->close();
        std::atomic_store(&nc.predPubs[0], std::shared_ptr<aeron::Publication>(nullptr));
    }
    if (nc.predPubs[1]) {
        nc.predPubs[1]->close();
        std::atomic_store(&nc.predPubs[1], std::shared_ptr<aeron::Publication>(nullptr));
    }
    if (nc.predSub) {
        nc.predSub->closeAndRemoveImages();
        nc.predSub = nullptr; //clean AFTER removing it from the list
    }

    nc.clientId = CLIENT_ID_UNSET;

    // If there's an IP address, prepare to connect.
    if (0 != nc.ip) {
        nc.controlPubId = ClusterClient::s_aeronClient->addExclusivePublication(
                aeron::ChannelUriStringBuilder()
                    .media("udp")
                    .endpoint(nc.ipString + ":" + std::to_string(Port::ENDPOINT))
                    .build(),
                StreamId::PROTOCOL);
        nc.controlSubId = ClusterClient::s_aeronClient->addSubscription(
                aeron::ChannelUriStringBuilder()
                    .media("udp")
                    .endpoint("0.0.0.0:0")
                    .controlEndpoint(nc.ipString + ":" + std::to_string(Port::CONTROL))
                    .controlMode("dynamic")
                    .build(),
                StreamId::PROTOCOL_MDC);
        nc.correlationIdHigh = Rng::generateUint64();
        nc.correlationIdLow = Rng::generateUint64();
        return true;
    }
    return false;
}

static bool whileDisconnected(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept
{
    if (nullptr == nc.controlPub) {
        nc.controlPub = ClusterClient::s_aeronClient->findExclusivePublication(nc.controlPubId);
    }
    if (nullptr == nc.controlSub) {
        nc.controlSub = ClusterClient::s_aeronClient->findSubscription(nc.controlSubId);
    }
    const bool isConnected =
           nullptr != nc.controlSub
        && nullptr != nc.controlPub
        && nc.controlSub->isConnected()
        && nc.controlPub->isConnected();
    return isConnected;
}

static bool beforeRequestingTopology(NodeClient &nc) noexcept {
    ++nc.clusterClient->nonDisconnectedNcCount;
    return !nc.clusterClient->topologyAlreadyReceived;
}

static bool onRegistrationError(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    constexpr uint32_t CORRELATION_CONFLICT = -2;
    if (CORRELATION_CONFLICT == message.registrationError.code) {
        nc.correlationIdHigh = Rng::generateUint64();
        nc.correlationIdLow = Rng::generateUint64();
        return false;
    };
    return true;
}

static bool onRegistrationConfirmation(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    nc.port = message.registrationResponse.controlPort;
    nc.clientId = message.registrationResponse.clientId;
    return true;
}

static bool onServerOpened(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    nc.queryPubId = ClusterClient::s_aeronClient->addPublication(
            aeron::ChannelUriStringBuilder()
                .media("udp")
                .endpoint(nc.ipString + ":" + std::to_string(Port::ENDPOINT))
                .build(),
            StreamId::PRED
            );
    nc.feedbackPubId = ClusterClient::s_aeronClient->addPublication(
            aeron::ChannelUriStringBuilder()
                .media("udp")
                .endpoint(nc.ipString + ":" + std::to_string(Port::ENDPOINT))
                .build(),
            StreamId::FEEDBACK);
    nc.predSubId = ClusterClient::s_aeronClient->addSubscription(
            aeron::ChannelUriStringBuilder()
                .media("udp")
                .endpoint("0.0.0.0:0")
                .controlEndpoint(nc.ipString + ":" + std::to_string(nc.port))
                .controlMode("dynamic")
                .build(),
            StreamId::PRED_MDC);
    std::atomic_store(&nc.predPubs[0],  std::shared_ptr<aeron::Publication>(nullptr));
    std::atomic_store(&nc.predPubs[1],  std::shared_ptr<aeron::Publication>(nullptr));
    nc.predSub = nullptr;
    return true;
}

static bool waitingForPredConnection(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    if (nullptr == nc.predPubs[0]) {
        std::atomic_store(&nc.predPubs[0],  ClusterClient::s_aeronClient->findPublication(nc.queryPubId));
    }
    if (nullptr == nc.predPubs[1]) {
        std::atomic_store(&nc.predPubs[1],  ClusterClient::s_aeronClient->findPublication(nc.feedbackPubId));
    }
    if (nullptr == nc.predSub) {
        nc.predSub = ClusterClient::s_aeronClient->findSubscription(nc.predSubId);
    }
    const bool isConnected =
           nullptr != nc.predSub
        && nullptr != nc.predPubs[0]
        && nullptr != nc.predPubs[1]
        && nc.predSub->isConnected()
        && nc.predPubs[0]->isConnected()
        && nc.predPubs[1]->isConnected();
    return isConnected;
}

static bool onServerReady(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    const auto &newPredSubList = getPredSubListCopy(nc, false);
    newPredSubList->emplace_back(nc.predSub);
    std::atomic_store(&nc.clusterClient->predSubList, newPredSubList);
    return true;
}

static bool whileReady(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    const bool ready =
           nc.predSub->isConnected()
        && nc.predPubs[0]->isConnected()
        && nc.predPubs[1]->isConnected()
        && nc.controlSub->isConnected()
        && nc.controlPub->isConnected();
    return !ready;
}

static bool whileLosingConnection(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    return
           nc.predSub->isConnected()
        && nc.predPubs[0]->isConnected()
        && nc.predPubs[1]->isConnected()
        && nc.controlSub->isConnected()
        && nc.controlPub->isConnected();
}

static bool onDisconnectionConfirmation(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    return true;
}

static bool onTopologyUpdate(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    const auto operation = message.topologyUpdate.operation;
    const auto nodeIpList = message.topologyUpdate.nodeIpList;
    const auto nodeCount = message.topologyUpdate.nodeCount;
    switch (operation) {
        case UpdateOperation::REMOVE:
            nc.clusterClient->removeNodesFromTopology(nodeIpList, nodeCount);
            break;
        case UpdateOperation::ADD:
            nc.clusterClient->addNodesToTopology(nodeIpList, nodeCount);
            if (nodeCount > 1) {
                nc.clusterClient->topologyAlreadyReceived = true;
                nc.clusterClient->statusCode = Ubiquity::SUCCESS;
            }
            break;
        case UpdateOperation::SET:
            nc.clusterClient->refreshTopology(nodeIpList, nodeCount);
            nc.clusterClient->topologyAlreadyReceived = true;
            nc.clusterClient->statusCode = Ubiquity::SUCCESS;
            break;
    }
    return true;
}

static bool onModelAdd(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    const auto modelId = message.modelAdd.modelId;
    const auto feedbackDelays = message.modelAdd.feedbackDelays;
    const auto feedbackDelaysCount = message.modelAdd.feedbackDelaysCount;
    const int8_t *features = reinterpret_cast<const int8_t *>(feedbackDelays + feedbackDelaysCount);
    const size_t baseSize = 1 + offsetof(ModelAddMessage, feedbackDelays) + sizeof(int32_t) * feedbackDelaysCount;
    if (messageLength < baseSize || messageLength < baseSize + features[0]) {
        return false;
    }
    nc.clusterClient->ensureSchemaForOneModelIsInCache(
            modelId,
            message.modelAdd.applicationId,
            feedbackDelays,
            feedbackDelaysCount,
            features
            );
    nc.clusterClient->loadBalancer->addNodeForOneModel(modelId, &nc);
    return false;
}

static bool onModelRemove(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    const auto modelId = message.modelRemove.modelId;
    nc.clusterClient->loadBalancer->removeNodeForOneModel(modelId, nc.ip);
    return false;
}

static bool ignore(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    return false;
}

struct TransitionPerInput {
    State::Value state;
    MessageType::Value inputType;
    bool (*function)(NodeClient &nodeClient, const ControlMessage &message, const size_t messageLength);
    State::Value nextState;
};

// This table is checked first. If the state and input (or absence of) match, the function is called and if it returns true, the state changes as indicated.
static const TransitionPerInput transitionsPerInput[] = {
    // State                            Input Message Type                          Function to call                Next state when resolved as true
    State::DISCONNECTED,                MessageType::NONE,                          whileDisconnected,              State::REQUESTING_TOPOLOGY,
    State::ANY,                         MessageType::TOPOLOGY_UPDATE,               onTopologyUpdate,               State::ANY, // ANY rule must be declared before more restrictive rules, otherwise it overrides it.
    State::REQUESTING_TOPOLOGY,         MessageType::TOPOLOGY_UPDATE,               onTopologyUpdate,               State::CONNECTING_PREDICTION,
    State::DISCONNECTING,               MessageType::TOPOLOGY_UPDATE,               ignore,                         State::ANY,
    State::UNBOUND,                     MessageType::TOPOLOGY_UPDATE,               ignore,                         State::ANY,
    State::CONNECTING_PREDICTION,       MessageType::REGISTRATION_ERROR,            onRegistrationError,            State::WAITING_BEFORE_RETRY,
    State::CONNECTING_PREDICTION,       MessageType::REGISTRATION_CONFIRMATION,     onRegistrationConfirmation,     State::REGISTERED,
    State::REGISTERED,                  MessageType::SERVER_OPENED,                 onServerOpened,                 State::AWAITING_PRED_CONNECTION,
    State::AWAITING_PRED_CONNECTION,    MessageType::NONE,                          waitingForPredConnection,       State::PRED_CONNECTED,
    State::PRED_CONNECTED,              MessageType::SERVER_READY,                  onServerReady,                  State::CLIENT_READY,
    State::CLIENT_READY,                MessageType::NONE,                          whileReady,                     State::LOSING_CONNECTION,
    State::LOSING_CONNECTION,           MessageType::NONE,                          whileLosingConnection,          State::CLIENT_READY,
    State::ANY,                         MessageType::MODEL_ADD,                     onModelAdd,                     State::ANY,
    State::UNBOUND,                     MessageType::MODEL_ADD,                     ignore,                         State::ANY,
    State::DISCONNECTING,               MessageType::MODEL_ADD,                     ignore,                         State::ANY,
    State::ANY,                         MessageType::MODEL_REMOVE,                  onModelRemove,                  State::ANY,
    State::DISCONNECTING,               MessageType::DISCONNECTION_CONFIRMATION,    onDisconnectionConfirmation,    State::DISCONNECTED,
};

struct TransitionPerPeriod {
    State::Value state;
    std::chrono::steady_clock::duration period;
    State::Value nextState;
};

// Then this table is checked and if the period matching the (possibly new) current state is exceeded, the state changes as indicated. 
static const TransitionPerPeriod transitionsPerPeriod[] = {
    //State                                 Period                          Next state
    State::DISCONNECTED,                    Period::CON_TIMEOUT,            State::DISCONNECTED,
    State::REQUESTING_TOPOLOGY,             Period::CON_TIMEOUT,            State::DISCONNECTED,
    State::CONNECTING_PREDICTION,           Period::CON_TIMEOUT,            State::DISCONNECTED,
    State::WAITING_BEFORE_RETRY,            Period::RETRY_COOLDOWN,         State::CONNECTING_PREDICTION,
    State::REGISTERED,                      Period::CON_TIMEOUT,            State::DISCONNECTED,
    State::AWAITING_PRED_CONNECTION,        Period::CON_TIMEOUT,            State::DISCONNECTED,
    State::PRED_CONNECTED,                  Period::CON_TIMEOUT,            State::DISCONNECTED,
    State::LOSING_CONNECTION,               Period::CON_TIMEOUT,            State::DISCONNECTED,
    State::DISCONNECTING,                   Period::DC_TIMEOUT,             State::DISCONNECTED,
};

struct TransitionCheck {
    State::Value nextState;
    bool (*function)(NodeClient &nodeClient);
    State::Value alternativeState;
};

// This table is checked before every transition caused by the other tables, to possibly alter it, in which case it is checked again recursively.
static const TransitionCheck transitionChecks[] = {
    //Next state if check passes            Check function                  Next state if check resolves as false
    State::DISCONNECTED,                    beforeDisconnected,             State::UNBOUND,
    State::UNBOUND,                         beforeUnbound,                  State::UNBOUND,
    State::REQUESTING_TOPOLOGY,             beforeRequestingTopology,       State::CONNECTING_PREDICTION,
};

struct PeriodicOutput {
    State::Value state;
    std::chrono::steady_clock::duration period;
    Output::Value output;
}; 

// Finally, this table is checked to periodically send an output message based on current state.
static const PeriodicOutput periodicOutputs[] = {
    //State                         Output period               Output value
    State::CONNECTING_PREDICTION,   Period::REQUEST,            Output::REGISTRATION_REQUEST,
    State::REQUESTING_TOPOLOGY,     Period::REQUEST,            Output::TOPOLOGY_REQUEST,
    State::REGISTERED,              Period::REQUEST,            Output::SERVER_OPEN_REQUEST,
    State::PRED_CONNECTED,          Period::REQUEST,            Output::SERVER_READY_REQUEST,
    State::CLIENT_READY,            Period::HEARTBEAT,          Output::HEARTBEAT,
    State::DISCONNECTING,           Period::REQUEST,            Output::DISCONNECTION_REQUEST,
};

// Compiled lookup tables for constant time access
static TransitionPerInput ctTransitionsPerInput[State::COUNT][MessageType::COUNT];
static TransitionPerPeriod ctTransitionsPerPeriod[State::COUNT];
static TransitionCheck ctTransitionChecks[State::COUNT];
static PeriodicOutput ctPeriodicOutputs[State::COUNT];

static size_t ctExpectedInputMessageSize[MessageType::COUNT];

static bool alwaysTrueWithInput(NodeClient &nc, const ControlMessage &message, const size_t messageLength) noexcept {
    return true;
}

static bool alwaysTrue(NodeClient &nc) noexcept {
    return true;
}

static constexpr std::chrono::steady_clock::duration VERY_LONG_PERIOD = std::chrono::hours(30*366*24);

void NodeClient::compileLookupTables(void) noexcept {
    for (auto &entries : ctTransitionsPerInput) {
        for (auto &entry : entries) {
            entry.function = alwaysTrueWithInput;
            entry.nextState = State::ANY;
        }
    }
    for (auto &entry : ctTransitionsPerPeriod) {
        entry.period = VERY_LONG_PERIOD;
    }
    for (auto &entry : ctTransitionChecks) {
        entry.function = alwaysTrue;
    }
    for (auto &entry : ctPeriodicOutputs) {
        entry.period = VERY_LONG_PERIOD;
    }
    for (auto &entry : transitionsPerInput) {
        if (State::ANY == entry.state) {
            for (size_t i = 0; i < State::COUNT; ++i) {
                ctTransitionsPerInput[i][entry.inputType] = entry;
            }
        } else {
            ctTransitionsPerInput[entry.state][entry.inputType] = entry;
        }
    }
    for (auto &entry : transitionsPerPeriod) {
        ctTransitionsPerPeriod[entry.state] = entry;
    }
    for (auto &entry : transitionChecks) {
        ctTransitionChecks[entry.nextState] = entry;
    }
    for (auto &entry : periodicOutputs) {
        ctPeriodicOutputs[entry.state] = entry;
    }
    for (auto &entry : expectedInputMessageSize) {
        ctExpectedInputMessageSize[entry.type] = entry.size;
    }
}

void NodeClient::switchStateAfterCheck(uint8_t nextState) noexcept {
    while (!ctTransitionChecks[nextState].function(*this)) {
        nextState = ctTransitionChecks[nextState].alternativeState;
    } 
    DBG("NodeClient " << this->slotId << " (" << this->ipString << ") [" << std::setw(4) << std::setfill('0') << std::hex << this->clientId << std::dec << "] [" << State::strings[this->state] << "] : Switched to state " << State::strings[nextState]);
    this->state = nextState;
    this->lastStateChangeTime = std::chrono::steady_clock::now();
}

inline bool isMyCorrelationId(ControlMessage* const message, uint64_t corrHigh, uint64_t corrLow) {
    if (message->common.type == MessageType::REGISTRATION_CONFIRMATION) {
        return message->registrationResponse.correlationIdHigh == corrHigh && message->registrationResponse.correlationIdLow == corrLow;
    }

    if (message->common.type == MessageType::REGISTRATION_ERROR) {
        return message->registrationError.correlationIdHigh == corrHigh && message->registrationError.correlationIdLow == corrLow;
    }

    return false;
}

bool NodeClient::switchStateBasedOnInput(void) noexcept
{
    auto *const message = reinterpret_cast<ControlMessage *>(this->clusterClient->inputBuffer);
    size_t messageLength = 0;
    bool isGlobalMessage = false;
    auto messageReceiver = [this, &message, &messageLength, &isGlobalMessage](
            aeron::concurrent::AtomicBuffer &atomicBuffer,
            aeron::util::index_t offset,
            aeron::util::index_t length,
            aeron::concurrent::logbuffer::Header &header
        ) {
            atomicBuffer.getBytes(offset, message->buffer, length);
            messageLength = static_cast<size_t>(length);
            const int type = message->common.type;
            if (type >= MessageType::COUNT) {
                DBG("NodeClient " << this->slotId << " (" << this->ipString << ") [" << std::setw(4) << std::setfill('0') << std::hex << this->clientId << std::dec << "] [" << State::strings[this->state] << "] : Received message with invalid type " << type);
                return;
            }
            isGlobalMessage = MessageType::isGlobalMessage[type];

            if (ctExpectedInputMessageSize[type] == 0) return;

            DBG("NodeClient " << this->slotId << " (" << this->ipString << ") [" << std::setw(4) << std::setfill('0') << std::hex << this->clientId << std::dec << "] [" << State::strings[this->state] << "] : Received " << MessageType::strings[type] << " (" << messageLength << " bytes)");
            if (MessageType::MODEL_ADD != type) {
                hexDump(message->buffer, messageLength);
            }
            if (ctExpectedInputMessageSize[type] > messageLength) {
                DBG("NodeClient " << this->slotId << " (" << this->ipString << ") [" << std::setw(4) << std::setfill('0') << std::hex << this->clientId << std::dec << "] [" << State::strings[this->state] << "] : Invalid length " << messageLength << " for message type " << MessageType::strings[type]);
                return;
            }
        };

    bool messageProcessed = false;
    bool messageReceived = false;
    if (nullptr != this->controlSub && 0 < this->controlSub->poll(messageReceiver, 1)) {
      messageReceived = true;
        messageProcessed = this->clientId == message->common.clientId
            || isGlobalMessage
            || (this->clientId == CLIENT_ID_UNSET && CLIENT_ID_BROADCAST == message->common.clientId
                    && isMyCorrelationId(message, this->correlationIdHigh, this->correlationIdLow));
    }

    const uint8_t inputType = messageProcessed ? message->common.type : MessageType::NONE;
    if (messageReceived && !messageProcessed) {
      DBG("NodeClient " << this->slotId << " (" << this->ipString << ") [" << std::setw(4) << std::setfill('0') << std::hex << this->clientId << std::dec << "] [" << State::strings[this->state] << "] : Ignoring next message...");
    }

    State::Value nextState = State::ANY;
    if (ctTransitionsPerInput[this->state][inputType].function(*this, *message, messageLength)) {
        nextState = ctTransitionsPerInput[this->state][inputType].nextState;
    }

    if (State::ANY != nextState) {
        this->switchStateAfterCheck(nextState);
        return true;
    }
    return messageProcessed;
}

void NodeClient::switchStateIfPeriodPassed() noexcept {
    const auto now = std::chrono::steady_clock::now();

    const auto transition = ctTransitionsPerPeriod[this->state];
    if (now > this->lastStateChangeTime + transition.period) {
        this->switchStateAfterCheck(transition.nextState);
    }
}

void NodeClient::sendOutputBasedOnState() noexcept {
    const int isConnected = nullptr != this->controlPub && this->controlPub->isConnected();
    const auto now = std::chrono::steady_clock::now();
    if (isConnected && now > this->lastOutputTime + ctPeriodicOutputs[this->state].period) {
        const Output::Value output = ctPeriodicOutputs[this->state].output;
        auto *const message = reinterpret_cast<ControlMessage *>(this->clusterClient->inputBuffer);
        if (Output::HEARTBEAT != output) {
            DBG("NodeClient " << this->slotId << " (" << this->ipString << ") [" << std::setw(4) << std::setfill('0') << std::hex << this->clientId << std::dec << "] [" << State::strings[this->state] << "] : Sending " << Output::strings[output]);
        }
        switch (output) {
            case Output::TOPOLOGY_REQUEST:
                message->common.type = MessageType::TOPOLOGY_REQUEST;
                message->common.clientId = clientId;
                this->controlPub->offer({message->buffer, sizeof(ControlMessageBase)});
                break;
            case Output::DISCONNECTION_REQUEST:
                if (this->clientId == CLIENT_ID_UNSET) {
                  DBG("NodeClient " << this->slotId << " (" << this->ipString << ") [" << std::setw(4) << std::setfill('0') << std::hex << this->clientId << std::dec << "] [" << State::strings[this->state] << "] : Not sending message as client ID is not set");
                  break;
                }
                message->common.type = MessageType::REGISTRATION_REQUEST;
                message->common.version = 0x01; // new version for v0.7
                message->common.clientId = clientId;
                message->registration.correlationIdHigh = this->correlationIdHigh;
                message->registration.correlationIdLow = this->correlationIdLow;
                message->registration.output = output;
                this->controlPub->offer({message->buffer, sizeof(RegistrationMessage)});
                break;
            case Output::REGISTRATION_REQUEST:
                // Hack to send registration request only once, must be fixed by a proper FSM
                if (this->isRegistrationRequestSent) {
                  DBG("NodeClient " << this->slotId << " (" << this->ipString << ") [" << std::setw(4) << std::setfill('0') << std::hex << this->clientId << std::dec << "] [" << State::strings[this->state] << "] : Not sending message as already sent");
                  break;
                }
                message->common.type = MessageType::REGISTRATION_REQUEST;
                message->common.version = 0x01; // new version for v0.7
                message->common.clientId = clientId;
                message->registration.correlationIdHigh = this->correlationIdHigh;
                message->registration.correlationIdLow = this->correlationIdLow;
                message->registration.output = output;
                this->controlPub->offer({message->buffer, sizeof(RegistrationMessage)});
                this->isRegistrationRequestSent=true;
                break;
            default:
                message->common.type = MessageType::REGISTRATION_REQUEST;
                message->common.version = 0x01; // new version for v0.7
                message->common.clientId = clientId;
                message->registration.correlationIdHigh = this->correlationIdHigh;
                message->registration.correlationIdLow = this->correlationIdLow;
                message->registration.output = output;
                this->controlPub->offer({message->buffer, sizeof(RegistrationMessage)});
                break;
        }
        if (output != Output::REGISTRATION_REQUEST) this->isRegistrationRequestSent=false;
        this->lastOutputTime = now;
    }
}

/*
 * Methods that can be called from outside this file
 */

void NodeClient::invoke(void) noexcept {
    Rng::seed();
    while (this->switchStateBasedOnInput());
    this->switchStateIfPeriodPassed();
    this->sendOutputBasedOnState();
}

void NodeClient::bind(uint32_t ip, size_t slotId) noexcept {
    this->slotId = slotId;
    this->ip = ip;
    char ipCString[INET_ADDRSTRLEN];
    this->ipString = std::string(inet_ntop(AF_INET, &ip, ipCString, INET_ADDRSTRLEN));
    DBG("ClusterClient(" << this->clusterClient->clusterId << ") : Binding new NodeClient " << slotId << " to ip " << this->ipString);
    this->switchStateAfterCheck(State::DISCONNECTED);
}

void NodeClient::unbind(void) noexcept {
    DBG("ClusterClient(" << this->clusterClient->clusterId << ") : Unbinding NodeClient " << this->slotId);
    this->ip = 0;
    this->switchStateAfterCheck(State::DISCONNECTED == this->state ? State::DISCONNECTED : State::DISCONNECTING);
}

bool NodeClient::isUnbound(void) noexcept {
    return this->ip == 0;
}

void NodeClient::initialize(ClusterClient *clusterClient) noexcept {
    this->clusterClient = clusterClient;
    this->state = State::UNBOUND;
    this->ip = 0;
}

std::string NodeClient::getStateString(void) const noexcept {
    return std::string(State::strings[this->state]);
}
