#pragma once 

#include "ubiquity_client/Ubiquity.hpp"

#include <Aeron.h>

#include <memory>
#include <unordered_map>

class LoadBalancer  {
    public:
        LoadBalancer() noexcept;

        void addNodeForOneModel(
                const int64_t &modelId,
                const uint32_t &ip,
                std::shared_ptr<aeron::Publication> &predictPub,
                std::shared_ptr<aeron::Publication> &feedbackPub,
                const uint16_t &clientId
                ) noexcept;

        void removeNodeForOneModel(
                const int64_t &modelId,
                const uint32_t &ip
                ) noexcept;

        void removeNodeForAllModels(
                const uint32_t &ip
                ) noexcept;

        Ubiquity::ErrorCode::Type sendApplicationMessage(
                const int64_t modelId,
                const int64_t queryId,
                aeron::concurrent::AtomicBuffer buffer,
                const size_t messageSize,
                const bool isFeedback
                ) noexcept;
    private:
        static constexpr size_t VNODES_PER_NODE = 8;

        class NodeRing {
            public:
                struct Node {
                    int64_t key;
                    uint32_t ip;
                    std::shared_ptr<aeron::Publication> pubs[2];
                    uint16_t clientId;
                };
                size_t nodeCount;
                std::shared_ptr<Node[]> nodeList;
                NodeRing() noexcept;
                NodeRing(size_t nodeCount) noexcept;
                ~NodeRing() = default;
        };

        using NodeRingMap = std::unordered_map<size_t, std::shared_ptr<NodeRing>>;
        std::shared_ptr<NodeRingMap> nodeRingByModel;

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

        static inline void removeNodeFromOneRing(
                std::shared_ptr<NodeRing> &ring,
                const uint32_t &ip) noexcept;

};
