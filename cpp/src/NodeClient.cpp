#include "NodeClient.hpp"
#include "ClusterClient.hpp"
#include "debug.hpp"

#include <ChannelUriStringBuilder.h>
#include <arpa/inet.h>
/*
 * Values and types used only in this file
 */

namespace Period {
    static constexpr auto CON_TIMEOUT = std::chrono::milliseconds(200);
    static constexpr auto RETRY_COOLDOWN = std::chrono::milliseconds(800);
    static constexpr auto DC_TIMEOUT = std::chrono::milliseconds(1000);
    static constexpr auto REQUEST = std::chrono::milliseconds(10);
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
        CONNECTING_CONTROL,
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
        "CONNECTING_CONTROL",
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

enum class Output : uint32_t {
    REGISTRATION_REQUEST = 0x01,
    DISCONNECTION_REQUEST = 0x5,
    HEARTBEAT = 0x6,
    SERVER_OPEN_REQUEST = 0x7,
    SERVER_READY_REQUEST = 0x8,
    TOPOLOGY_REQUEST = 0x0f, // TODO move this elsewhere, it should be in an other type
};

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
        MODEL_UPDATE = 0x0e,
        TOPOLOGY_REQUEST = 0x0f,
        TOPOLOGY_RESPONSE = 0x10,
        DISCONNECTION_CONFIRMATION = 0x11,
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
        "UNUSED1",
        "UNUSED2",
        "SERVER_READY",
        "UNUSED3",
        "SERVER_OPENED",
        "TOPOLOGY_UPDATE",
        "MODEL_UPDATE",
        "TOPOLOGY_REQUEST",
        "TOPOLOGY_RESPONSE",
        "DISCONNECTION_CONFIRMATION",
        "NONE",
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

struct ModelInventoryUpdate {
    ControlMessageBase _;
    UpdateOperation operation;
    uint16_t modelIdCount;
    uint32_t nodeIp;
    int64_t modelIdList[1];
} __attribute__((packed));

struct RegistrationMessage {
    ControlMessageBase _;
    uint64_t correlationIdHigh;
    uint64_t correlationIdLow;
    Output output;
} __attribute__((packed));

union ControlMessage {
    uint8_t buffer[1];
    ControlMessageBase common;
    RegistrationErrorMessage registrationError;
    RegistrationResponseMessage registrationResponse;
    TopologyUpdateMessage topologyUpdate;
    ModelInventoryUpdate modelInventoryUpdate;
    RegistrationMessage registrationMessage;
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
    MessageType::MODEL_UPDATE,                  sizeof(ModelInventoryUpdate),
    MessageType::TOPOLOGY_REQUEST,              sizeof(ControlMessageBase),
    MessageType::DISCONNECTION_CONFIRMATION,    sizeof(ControlMessageBase),
};

/*
 * Finite State Machine controling connection of a NodeClient to its remote Node
 */

static bool beforeUnbound(NodeClient &nc) noexcept {
    --nc.clusterClient->boundNcCount;
    return true;
}

static bool beforeDisconnected(NodeClient &nc) noexcept {
    nc.clusterClient->loadBalancer->removeNodeForAllModels(nc.ip);  
    if (nc.controlPub) {
        nc.controlPub->close();
        nc.controlPub = nullptr;
    }
    if (nc.controlSub) {
        nc.controlSub->closeAndRemoveImages();
        nc.controlSub = nullptr;
    }
    if (nc.queryPub) {
        nc.queryPub->close();
        nc.queryPub = nullptr;
    }
    if (nc.feedbackPub) {
        nc.feedbackPub->close();
        nc.feedbackPub = nullptr;
    }
    if (nc.predSub) {
        nc.predSub->closeAndRemoveImages();
        nc.predSub = nullptr;
    }
    return 0 != nc.ip;
}

static bool whileDisconnected(NodeClient &nc, ControlMessage &message) noexcept
{
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

static bool waitingForControlConnection(NodeClient &nc, ControlMessage &message) noexcept {
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
    return !nc.clusterClient->topologyAlreadyReceived;
}

static bool onRegistrationError(NodeClient &nc, ControlMessage &message) noexcept {
    constexpr uint32_t CORRELATION_CONFLICT = -2;
    if (CORRELATION_CONFLICT == message.registrationError.code) {
        nc.correlationIdHigh = Rng::generateUint64();
        nc.correlationIdLow = Rng::generateUint64();
        return false;
    };
    return true;
}

static bool onRegistrationConfirmation(NodeClient &nc, ControlMessage &message) noexcept {
    nc.port = message.registrationResponse.controlPort;
    nc.clientId = message.registrationResponse.clientId;
    return true;
}

static bool onServerOpened(NodeClient &nc, ControlMessage &message) noexcept {
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
    nc.queryPub = nullptr;
    nc.feedbackPub = nullptr;
    nc.predSub = nullptr;
    return true;
}

static bool waitingForPredConnection(NodeClient &nc, ControlMessage &message) noexcept {
    if (nullptr == nc.queryPub) {
        nc.queryPub = ClusterClient::s_aeronClient->findPublication(nc.queryPubId);
    }
    if (nullptr == nc.feedbackPub) {
        nc.feedbackPub = ClusterClient::s_aeronClient->findPublication(nc.feedbackPubId);
    }
    if (nullptr == nc.predSub) {
        nc.predSub = ClusterClient::s_aeronClient->findSubscription(nc.predSubId);
    }
    const bool isConnected =
           nullptr != nc.predSub
        && nullptr != nc.queryPub
        && nullptr != nc.feedbackPub
        && nc.predSub->isConnected()
        && nc.queryPub->isConnected()
        && nc.feedbackPub->isConnected();
    return isConnected;
}

static bool onServerReady(NodeClient &nc, ControlMessage &message) noexcept {
    auto newPredSubList = std::make_shared<std::vector<std::shared_ptr<aeron::Subscription>>>();
    for (const auto &predSub : *nc.clusterClient->predSubList) {
        newPredSubList->emplace_back(predSub);
    }
    newPredSubList->emplace_back(nc.predSub);
    atomic_store(&nc.clusterClient->predSubList, newPredSubList);
    return true;
}

static bool whileReady(NodeClient &nc, ControlMessage &message) noexcept {
    const bool ready =
           nc.predSub->isConnected()
        && nc.queryPub->isConnected()
        && nc.feedbackPub->isConnected()
        && nc.controlSub->isConnected()
        && nc.controlPub->isConnected();
    return !ready;
}

static bool whileLosingConnection(NodeClient &nc, ControlMessage &message) noexcept {
    return
           nc.predSub->isConnected()
        && nc.queryPub->isConnected()
        && nc.feedbackPub->isConnected()
        && nc.controlSub->isConnected()
        && nc.controlPub->isConnected();
}

static bool onDisconnectionConfirmation(NodeClient &nc, ControlMessage &message) noexcept {
    return true;
}

static bool onTopologyUpdate(NodeClient &nc, ControlMessage &message) noexcept {
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

static bool onModelUpdate(NodeClient &nc, ControlMessage &message) noexcept {
    const auto modelIdList = message.modelInventoryUpdate.modelIdList;
    const auto modelIdCount = message.modelInventoryUpdate.modelIdCount;
    switch (message.modelInventoryUpdate.operation) {
        case UpdateOperation::REMOVE:
            for (size_t i = 0; i < modelIdCount; ++i) {
                const auto modelId = modelIdList[i];
                nc.clusterClient->loadBalancer->removeNodeForOneModel(modelId, nc.ip);
            }
            break;
        case UpdateOperation::ADD:
            for (size_t i = 0; i < modelIdCount; ++i) {
                const auto modelId = modelIdList[i];
                nc.clusterClient->ensureSchemaForOneModelIsInCache(modelId);
                nc.clusterClient->loadBalancer->addNodeForOneModel(modelId, nc.ip, nc.queryPub, nc.feedbackPub, nc.clientId);
            }
            break;
        case UpdateOperation::SET:
            nc.clusterClient->loadBalancer->removeNodeForAllModels(nc.ip);
            for (size_t i = 0; i < modelIdCount; ++i) {
                const auto modelId = modelIdList[i];
                nc.clusterClient->ensureSchemaForOneModelIsInCache(modelId);
                nc.clusterClient->loadBalancer->addNodeForOneModel(modelId, nc.ip, nc.queryPub, nc.feedbackPub, nc.clientId);
            }
            break;
    }
    return false;
}

static bool ignore(NodeClient &nc, ControlMessage &message) noexcept {
    return false;
}

struct TransitionPerInput {
    State::Value state;
    MessageType::Value inputType;
    bool (*function)(NodeClient &nodeClient, ControlMessage &message);
    State::Value nextState;
};

// This table is checked first. If the state and input (or absence of) match, the function is called and if it returns true, the state changes as indicated.
static const TransitionPerInput transitionsPerInput[] = {
    // State                            Input Message Type                          Function to call                Next state when resolved as true
    State::DISCONNECTED,                MessageType::NONE,                          whileDisconnected,              State::CONNECTING_CONTROL,
    State::CONNECTING_CONTROL,          MessageType::NONE,                          waitingForControlConnection,    State::REQUESTING_TOPOLOGY,
    State::ANY,                         MessageType::TOPOLOGY_UPDATE,               onTopologyUpdate,               State::ANY,
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
    State::DISCONNECTING,               MessageType::DISCONNECTION_CONFIRMATION,    onDisconnectionConfirmation,    State::DISCONNECTED,
    State::ANY,                         MessageType::MODEL_UPDATE,                  onModelUpdate,                  State::ANY,
    State::DISCONNECTING,               MessageType::MODEL_UPDATE,                  ignore,                         State::ANY,
    State::UNBOUND,                     MessageType::MODEL_UPDATE,                  ignore,                         State::ANY,
};

struct TransitionPerPeriod {
    State::Value state;
    std::chrono::steady_clock::duration period;
    State::Value nextState;
};

// Then this table is checked and if the period matching the (possibly new) current state is exceeded, the state changes as indicated. 
static const TransitionPerPeriod transitionsPerPeriod[] = {
    //State                                 Period                          Next state
    State::CONNECTING_CONTROL,              Period::CON_TIMEOUT,            State::DISCONNECTED,
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
    Output output;
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

static bool alwaysTrueWithInput(NodeClient &nc, ControlMessage &message) noexcept {
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
    DBG("NodeClient " << this->slotId << " (" << this->ipString << ") : Switched to state " << State::strings[nextState] << " from state " << State::strings[this->state]);
    this->state = nextState;
    this->lastStateChangeTime = std::chrono::steady_clock::now();
}

static constexpr uint16_t CLIENT_ID_BROADCAST = __SHRT_MAX__;
static constexpr uint16_t CLIENT_ID_UNSET = __UINT16_MAX__;

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
    auto messageReceiver = [this, &message](
            aeron::concurrent::AtomicBuffer &atomicBuffer,
            aeron::util::index_t offset,
            aeron::util::index_t length,
            aeron::concurrent::logbuffer::Header &header
        ) {
            atomicBuffer.getBytes(offset, message->buffer, length);
            const int type = message->common.type;
            if (type >= MessageType::COUNT) {
                DBG("NodeClient " << this->slotId << " (" << this->ipString << ") : Received message with invalid type " << type);
                return;
            }
            if (ctExpectedInputMessageSize[type] != 0) {
                const size_t uLength = static_cast<size_t>(length);
                DBG("NodeClient " << this->slotId << " (" << this->ipString << ") : Received " << MessageType::strings[type] << " (" << uLength << " bytes)");
                hexDump(message->buffer, uLength);
                if (ctExpectedInputMessageSize[type] > uLength) {
                    DBG("NodeClient " << this->slotId << " (" << this->ipString << ") : Invalid length " << uLength << " for message type " << MessageType::strings[type]);
                    return;
                }
            }
        };

    bool messageReceived = false;
    if (nullptr != this->controlSub && 0 < this->controlSub->poll(messageReceiver, 1)) {
        messageReceived = CLIENT_ID_BROADCAST == message->common.clientId || 
            this->clientId == message->common.clientId;

        if (this->clientId == CLIENT_ID_UNSET) {
            messageReceived = messageReceived || isMyCorrelationId(message, this->correlationIdHigh, this->correlationIdLow);
        }
    }

    const uint8_t inputType = messageReceived ? message->common.type : MessageType::NONE;

    State::Value nextState = State::ANY;
    if (ctTransitionsPerInput[this->state][inputType].function(*this, *message)) {
        nextState = ctTransitionsPerInput[this->state][inputType].nextState;
    }

    if (State::ANY != nextState) {
        this->switchStateAfterCheck(nextState);
        return true;
    }
    return messageReceived;
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
        const Output output = ctPeriodicOutputs[this->state].output;
        auto *const message = reinterpret_cast<ControlMessage *>(this->clusterClient->inputBuffer);
        switch (output) {
            case Output::TOPOLOGY_REQUEST:
                message->common.type = MessageType::TOPOLOGY_REQUEST;
                message->common.clientId = clientId;
                this->controlPub->offer({message->buffer, sizeof(ControlMessageBase)});
                break;
            default:
                message->common.type = MessageType::REGISTRATION_REQUEST;
                message->common.version = 0x01; // new version for v0.7
                message->common.clientId = clientId;
                message->registrationMessage.correlationIdHigh = this->correlationIdHigh;
                message->registrationMessage.correlationIdLow = this->correlationIdLow;
                message->registrationMessage.output = output;
                this->controlPub->offer({message->buffer, sizeof(RegistrationMessage)});
                break;
        }
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
    ++this->clusterClient->boundNcCount;
    this->switchStateAfterCheck(State::DISCONNECTED);
}

void NodeClient::unbind(void) noexcept {
    this->ip = 0;
    this->switchStateAfterCheck(State::DISCONNECTING);
}

bool NodeClient::isUnbound(void) noexcept {
    return State::UNBOUND == this->state;
}

void NodeClient::initialize(ClusterClient *clusterClient) noexcept {
    this->clusterClient = clusterClient;
    this->state = State::UNBOUND;
}

std::string NodeClient::getStateString(void) const noexcept {
    return std::string(State::strings[this->state]);
}
