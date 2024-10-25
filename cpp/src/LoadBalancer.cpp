#include "LoadBalancer.hpp"
#include "ClientHandleImpl.hpp"
#include "debug.hpp"

#include <xxh3.h>

LoadBalancer::NodeRing::NodeRing() noexcept :
    nodeCount{0},
    nodeList{nullptr}
{
}

LoadBalancer::NodeRing::NodeRing(
        const size_t nodeCount
        ) noexcept :
    nodeCount{nodeCount},
    nodeList{new Node[nodeCount]}
{
}

LoadBalancer::LoadBalancer(void) noexcept :
    nodeRingByModel{new NodeRingMap()}
{
}

void LoadBalancer::addNodeForOneModel(
        const int64_t &modelId,
        const uint32_t &ip,
        std::shared_ptr<aeron::Publication> &predictPub,
        std::shared_ptr<aeron::Publication> &feedbackPub,
        const uint16_t &clientId
        ) noexcept
{
    auto nodeRingByModel = this->nodeRingByModel;
    if (nodeRingByModel->end() == nodeRingByModel->find(modelId)) {
        nodeRingByModel = std::shared_ptr<NodeRingMap>(new NodeRingMap(*nodeRingByModel));
        nodeRingByModel->emplace(modelId, std::make_shared<NodeRing>());
        std::atomic_store(&this->nodeRingByModel, nodeRingByModel);
    }
    std::shared_ptr<NodeRing> &nodeRing = (*nodeRingByModel)[modelId];
    for (size_t index = 0; index < nodeRing->nodeCount; ++index) {
        if (nodeRing->nodeList[index].ip == ip) {
            return;
        }
    }

    std::array<int64_t, VNODES_PER_NODE> keys;
    for (size_t i = 0; i < VNODES_PER_NODE; i++) {
        keys[i] = XXH3_64bits_withSeed(&ip, sizeof(ip), this->seeds[i]);
    }
    std::sort(keys.begin(), keys.end());

    size_t iKeys = 0;
    size_t iNodes = 0;
    size_t iTotal = 0;
    auto newRing = std::make_shared<NodeRing>(VNODES_PER_NODE + nodeRing->nodeCount);
    while (iTotal < newRing->nodeCount) {
        if (iNodes < nodeRing->nodeCount && (iKeys == VNODES_PER_NODE || nodeRing->nodeList[iNodes].key < keys[iKeys])) {
            newRing->nodeList[iTotal++] = nodeRing->nodeList[iNodes++];
        } else {
            auto &node = newRing->nodeList[iTotal++];
            node.key = keys[iKeys++];
            node.ip = ip;
            node.pubs[0] = predictPub;
            node.pubs[1] = feedbackPub;
            node.clientId = clientId;
        }
    }
    std::atomic_store(&nodeRing, newRing);
}

inline void LoadBalancer::removeNodeFromOneRing(
        std::shared_ptr<NodeRing> &nodeRing,
        const uint32_t &ip
        ) noexcept
{
    size_t newNodeCount = 0;
    for (size_t index = 0; index < nodeRing->nodeCount; ++index) {
        if (nodeRing->nodeList[index].ip != ip) {
            ++newNodeCount;
        }
    }
    if (newNodeCount != nodeRing->nodeCount) {
        auto newRing = std::make_shared<NodeRing>(newNodeCount);
        for (size_t index = 0, addedCount = 0; index < nodeRing->nodeCount; ++index) {
            if (nodeRing->nodeList[index].ip != ip) {
                newRing->nodeList[addedCount++] = nodeRing->nodeList[index];
            }
        }
        std::atomic_store(&nodeRing, newRing);
    }
}

void LoadBalancer::removeNodeForOneModel(
        const int64_t &modelId,
        const uint32_t &ip
        ) noexcept
{
    const auto &nodeRingByModel = this->nodeRingByModel;
    if (nodeRingByModel->end() == nodeRingByModel->find(modelId)) {
        return;
    }
    auto &nodeRing = (*nodeRingByModel)[modelId];

    removeNodeFromOneRing(nodeRing, ip);
}

void LoadBalancer::removeNodeForAllModels(
        const uint32_t &ip
        ) noexcept
{
    const auto &nodeRingByModel = this->nodeRingByModel;
    for (auto &it : *nodeRingByModel) {
        removeNodeFromOneRing(it.second, ip);
    }
}

Ubiquity::StatusCode LoadBalancer::sendApplicationMessage(
        const int64_t modelId,
        const int64_t queryId,
        aeron::concurrent::AtomicBuffer buffer,
        const size_t messageSize,
        const bool isFeedback
        ) noexcept
{
    auto nodeRingByModel = std::atomic_load(&this->nodeRingByModel);
    if (__glibc_unlikely(nodeRingByModel->end() == nodeRingByModel->find(modelId))) {
        return Ubiquity::NO_COMPATIBLE_NODE;
    }
    std::shared_ptr<NodeRing> nodeRing = std::atomic_load(&(*nodeRingByModel)[modelId]);
    const uint64_t balancingIndex = static_cast<uint64_t>(XXH3_64bits(&queryId, sizeof(queryId)));
    Ubiquity::StatusCode statusCode = Ubiquity::NO_COMPATIBLE_NODE;
    for (size_t i = 0; i < nodeRing->nodeCount; ++i) {
        NodeRing::Node &node = nodeRing->nodeList[(balancingIndex + i) % nodeRing->nodeCount];
        while (true) {
            if (__glibc_likely(node.pubs[isFeedback]->isConnected())) {
                buffer.putUInt16(offsetof(Ubiquity::ClientHandle::Impl::ApplicationMessage, clientId), node.clientId);
                const auto res = node.pubs[isFeedback]->offer(buffer, 0, messageSize);
                switch (res) {
                    case aeron::BACK_PRESSURED:
                        statusCode = Ubiquity::BACK_PRESSURED;
                        break;
                    case aeron::PUBLICATION_CLOSED:
                        break;
                    case aeron::ADMIN_ACTION:
                        continue;
                    default:
                        return Ubiquity::SUCCESS;
                }
            }
            break;
        }
    }
    return statusCode;
}
