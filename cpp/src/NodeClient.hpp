#pragma once

#include "ubiquity_client/Ubiquity.hpp"

#include <Aeron.h>

class ClusterClient;

class NodeClient {
    public:
        static void compileLookupTables(void) noexcept;

        ClusterClient *clusterClient;

        uint32_t ip = 0;
        std::string ipString;

        std::uint16_t clientId = __UINT16_MAX__;
        std::uint64_t correlationIdHigh;
        std::uint64_t correlationIdLow;
        std::uint16_t port = 0;

        int64_t controlPubId = 0;
        int64_t controlSubId = 0;
        int64_t queryPubId = 0;
        int64_t feedbackPubId = 0;
        int64_t predSubId = 0;

        std::shared_ptr<aeron::ExclusivePublication> controlPub = nullptr;
        std::shared_ptr<aeron::Subscription> controlSub = nullptr;
        std::shared_ptr<aeron::Publication> queryPub = nullptr;
        std::shared_ptr<aeron::Publication> feedbackPub = nullptr;
        std::shared_ptr<aeron::Subscription> predSub = nullptr;

    protected:
        friend ClusterClient;
        bool refreshed = false;

        void initialize(ClusterClient *clusterClient) noexcept;
        void bind(uint32_t ip, size_t slotId) noexcept;
        void unbind(void) noexcept;
        bool isUnbound(void) noexcept;
        void invoke(void) noexcept;
        std::string getStateString(void) const noexcept;


    private:
        size_t slotId;

        void switchStateAfterCheck(uint8_t nextState) noexcept;
        bool switchStateBasedOnInput(void) noexcept;
        void switchStateIfPeriodPassed() noexcept;
        void sendOutputBasedOnState() noexcept;

        uint8_t state;
        std::chrono::steady_clock::time_point lastStateChangeTime;
        std::chrono::steady_clock::time_point lastOutputTime;

};
