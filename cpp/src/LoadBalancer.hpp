#pragma once 

#include "ubiquity_client/Ubiquity.hpp"
#include "NodeClient.hpp"

#include <Aeron.h>

#include <memory>
#include <unordered_map>

class LoadBalancer  {
    public:
        LoadBalancer() noexcept;

        void addNodeForOneModel(
                const int64_t &modelId,
                const NodeClient *const nodeClient
                ) noexcept;

        void removeNodeForOneModel(
                const int64_t &modelId,
                const uint32_t &ip
                ) noexcept;

        void removeNodeForAllModels(
                const uint32_t &ip
                ) noexcept;

        Ubiquity::StatusCode sendApplicationMessage(
                const int64_t modelId,
                const int64_t queryId,
                aeron::concurrent::AtomicBuffer buffer,
                const size_t messageSize,
                const bool isFeedback
                ) noexcept;

        class NodeRing {
            public:
                struct Node {
                    int64_t key;
                    const NodeClient *nodeClient;
                };
                size_t nodeCount;
                std::shared_ptr<Node[]> nodeList;
                NodeRing() noexcept;
                NodeRing(size_t nodeCount) noexcept;
                ~NodeRing() = default;
        };
        using NodeRingMap = std::unordered_map<size_t, std::shared_ptr<const NodeRing>>;

    private:
        static constexpr size_t VNODES_PER_NODE = 8;

        std::shared_ptr<const NodeRingMap> nodeRingByModel;

        static constexpr uint64_t seeds[] = {
            0X1E5B5EDF522B8E5CULL,
            0XA65D900D872E9B3BULL,
            0X30ECF019A3F85CA1ULL,
            0X3886F33A81AD063EULL,
            0X6381DB9477A42B55ULL,
            0XEB349C3D77107FF2ULL,
            0X1FBED9254108014CULL,
            0XCBF9F891662632D8ULL
        };

        inline NodeRing *copyRingWithAnAdditionalNodeInside(
                const std::shared_ptr<const NodeRing> &oldRing,
                const NodeClient *const nodeClient
                ) noexcept;

        inline std::shared_ptr<const NodeRing> copyRingWithSpecificNodeRemoved(
                const std::shared_ptr<const NodeRing> &oldRing,
                const uint32_t &ip
                ) noexcept;
};
