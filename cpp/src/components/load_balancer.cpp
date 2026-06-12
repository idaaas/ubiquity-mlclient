#include "load_balancer.hpp"
#include <cstring>
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath> 

status_code_t send_message_with_rendezvous(
        const node_client_map_t node_clients,
        const ip_set_t *const targeted_nodes,
        const int64_t balance_key,
        const stream_id_t stream_id,
        uint8_t *const message_content,
        const size_t message_size
        ) noexcept
{
    // Build scored selection from targeted nodes
    std::vector<std::pair<double, node_client_ref_t>> scored;
    status_code_t status_code = STATUS_CODE_NO_COMPATIBLE_NODE;

    for (auto &ip : *targeted_nodes) {
        auto it = node_clients.find(ip);
        if (it == node_clients.end()) continue;  // skip unknown IPs

        const auto& node = it->second;

        uint64_t hash_value = XXH3_64bits_withSeed(&balance_key, sizeof(balance_key), ip);

        // Normalize hash to interval (0,1)
        // Extracts the 53 most significant bits of hash value
        static constexpr double MANTISSA_SCALE = 1.0 / (1ULL << 53); // 2^-53
        double u = static_cast<double>(hash_value >> 11) * MANTISSA_SCALE; // max = (2^53-1) * 2^-53 < 1
        u = std::max(u, std::numeric_limits<double>::min());

        // Weighted rendezvous score: weight / -ln(u)
        double weight = static_cast<double>(node_client_get_weight(node.get()));
        double score = weight * (1.0 / (-std::log(u)));
        scored.push_back({score, node});
    }

    // Sort descending by score — highest score = preferred node
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) {
            return a.first > b.first;
        });

    // Try nodes in order of score, fallback to next on failure
    for (const auto& entry : scored) {
        // computed_node_weight_t score = entry.first ;
        node_client_ref_t node = entry.second;
        node_client_send_message(
            node.get(),
            stream_id,
            message_content,
            message_size,
            &status_code
        );

        if (STATUS_CODE_SUCCESS == status_code) 
            break;
    }
    return status_code;  // all nodes failed
}

status_code_t shared_send_message_with_rendezvous(
        shared_state_store_t *const store,
        const ip_set_t *const targeted_nodes,
        const int64_t balance_key,
        const stream_id_t stream_id,
        uint8_t *const message_content,
        const size_t message_size
        ) noexcept
{
    node_client_map_ref_t node_clients = store_get_node_clients(store);
    return send_message_with_rendezvous(
            *node_clients,
            targeted_nodes,
            balance_key,
            stream_id,
            message_content,
            message_size
    );
}

