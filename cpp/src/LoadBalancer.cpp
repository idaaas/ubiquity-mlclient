#include "LoadBalancer.hpp"
#include "ClientHandleImpl.hpp"
#include "debug.hpp"

#include <xxh3.h>

LoadBalancer::NodeRing::NodeRing() noexcept :
    nodeCount{0},
    nodeList{new Node[1]}
{
}

LoadBalancer::NodeRing::NodeRing(
        const size_t nodeCount
        ) noexcept :
    nodeCount{nodeCount},
    nodeList{new Node[0 == nodeCount ? 1 : nodeCount]}
{
}

LoadBalancer::LoadBalancer(void) noexcept :
    nodeRingByModel{new NodeRingMap()}
{
}

inline LoadBalancer::NodeRing *LoadBalancer::copyRingWithAnAdditionalNodeInside(
        const std::shared_ptr<const NodeRing> &oldRing,
        const uint32_t &ip,
        const std::shared_ptr<aeron::Publication> &predictPub,
        const std::shared_ptr<aeron::Publication> &feedbackPub,
        const uint16_t &clientId
        ) noexcept
{
    // Create keys to place the virtual nodes into the ring.
    std::array<int64_t, VNODES_PER_NODE> keys;
    for (size_t i = 0; i < VNODES_PER_NODE; i++) {
        keys[i] = XXH3_64bits_withSeed(&ip, sizeof(ip), this->seeds[i]);
    }
    std::sort(keys.begin(), keys.end());

    // Insert virtual nodes from the old ring version into the new one along with the additional virtual nodes.
    size_t iKeys = 0;
    size_t iNodes = 0;
    size_t iTotal = 0;
    auto newRing = new NodeRing(oldRing->nodeCount + VNODES_PER_NODE);
    while (iTotal < newRing->nodeCount) {
        if (iNodes < oldRing->nodeCount && (iKeys == VNODES_PER_NODE || oldRing->nodeList[iNodes].key < keys[iKeys])) {
            newRing->nodeList[iTotal++] = oldRing->nodeList[iNodes++];
        } else {
            auto &node = newRing->nodeList[iTotal++];
            node.key = keys[iKeys++];
            node.ip = ip;
            node.pubs[0] = predictPub;
            node.pubs[1] = feedbackPub;
            node.clientId = clientId;
        }
    }
    return newRing;
}

void LoadBalancer::addNodeForOneModel(
        const int64_t &modelId,
        const uint32_t &ip,
        std::shared_ptr<aeron::Publication> &predictPub,
        std::shared_ptr<aeron::Publication> &feedbackPub,
        const uint16_t &clientId
        ) noexcept
{
    const NodeRing *newRing;
    try {
        const auto &oldRing = this->nodeRingByModel->at(modelId);
        for (size_t index = 0; index < oldRing->nodeCount; ++index) {
            if (oldRing->nodeList[index].ip == ip) {
                // Current version of the NodeRing already contains that ip.
                return;
            }
        }
        newRing = copyRingWithAnAdditionalNodeInside(oldRing, ip, predictPub, feedbackPub, clientId);
    } catch (const std::out_of_range &e) {
        newRing = copyRingWithAnAdditionalNodeInside(std::make_shared<const NodeRing>(), ip, predictPub, feedbackPub, clientId);
    }
    auto newNodeRingByModel = new NodeRingMap(*this->nodeRingByModel);
    (*newNodeRingByModel)[modelId].reset(newRing);
    std::atomic_store(&this->nodeRingByModel, std::shared_ptr<const NodeRingMap>(newNodeRingByModel));
}

inline std::shared_ptr<const LoadBalancer::NodeRing> LoadBalancer::copyRingWithSpecificNodeRemoved(
        const std::shared_ptr<const NodeRing> &oldRing,
        const uint32_t &ip
        ) noexcept
{
    size_t vNodesToRemove = 0;
    for (size_t index = 0; index < oldRing->nodeCount; ++index) {
        if (ip == oldRing->nodeList[index].ip) {
            ++vNodesToRemove;
        }
    }
    if (0 == vNodesToRemove) {
        return oldRing;
    }
    auto newRing = new NodeRing(oldRing->nodeCount - vNodesToRemove);
    for (size_t iRead = 0, iWrite = 0; iRead < oldRing->nodeCount; ++iRead) {
        if (ip != oldRing->nodeList[iRead].ip) {
            newRing->nodeList[iWrite++] = oldRing->nodeList[iRead];
        }
    }
    return std::shared_ptr<const NodeRing>(newRing);
}

void LoadBalancer::removeNodeForOneModel(
        const int64_t &modelId,
        const uint32_t &ip
        ) noexcept
{
    const auto &nodeRingByModel = this->nodeRingByModel;
    const auto &it = nodeRingByModel->find(modelId);
    if (nodeRingByModel->end() == it) {
        return;
    }
    for (size_t index = 0; index < it->second->nodeCount; ++index) {
        if (ip == it->second->nodeList[index].ip) {
            auto newNodeRingByModel = new NodeRingMap(*this->nodeRingByModel);
            const auto newRing = copyRingWithSpecificNodeRemoved(it->second, ip);
            (*newNodeRingByModel)[modelId] = newRing;
            std::atomic_store(&this->nodeRingByModel, std::shared_ptr<const NodeRingMap>(newNodeRingByModel));
            return;
        }
    }
}

void LoadBalancer::removeNodeForAllModels(
        const uint32_t &ip
        ) noexcept
{
    auto newNodeRingByModel = new NodeRingMap(*this->nodeRingByModel);
    for (const auto &it : *this->nodeRingByModel) {
        const auto newRing = copyRingWithSpecificNodeRemoved(it.second, ip);
        (*newNodeRingByModel)[it.first] = newRing;
    }
    std::atomic_store(&this->nodeRingByModel, std::shared_ptr<const NodeRingMap>(newNodeRingByModel));
}

Ubiquity::StatusCode LoadBalancer::sendApplicationMessage(
        const int64_t modelId,
        const int64_t queryId,
        aeron::concurrent::AtomicBuffer buffer,
        const size_t messageSize,
        const bool isFeedback
        ) noexcept
{
    const auto &nodeRingByModel = std::atomic_load(&this->nodeRingByModel);
    const auto &it = nodeRingByModel->find(modelId);
    if (__glibc_unlikely(nodeRingByModel->end() == it)) {
        return Ubiquity::NO_COMPATIBLE_NODE;
    }
    const std::shared_ptr<const NodeRing> &nodeRing = it->second;
    const uint64_t balancingIndex = static_cast<uint64_t>(XXH3_64bits(&queryId, sizeof(queryId)));
    Ubiquity::StatusCode statusCode = Ubiquity::NO_COMPATIBLE_NODE;
    for (size_t i = 0; i < nodeRing->nodeCount; ++i) {
        const NodeRing::Node &node = nodeRing->nodeList[(balancingIndex + i) % nodeRing->nodeCount];
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
