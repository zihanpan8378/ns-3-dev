#ifndef IPV4_ANTNET_ROUTING_H
#define IPV4_ANTNET_ROUTING_H

#include "ipv4-antnet-routing-table-entry.h"
#include "ipv4-antnet-local-traffic-statistics-entry.h"

#include "ns3/ipv4-address.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"

#include <list>
#include <stdint.h>

namespace ns3
{

static const uint8_t PROTOCOL_ANTNET = 253; // Custom protocol number for AntNet

class Packet;
class NetDevice;
class Ipv4Interface;
class Ipv4Address;
class Ipv4Header;
class Node;

class Ipv4AntNetRouting : public Ipv4RoutingProtocol
{
    public:
        typedef std::list<Ipv4AntNetRoutingTableEntry> RoutingTable;
        typedef std::list<Ipv4AntNetLocalTrafficStatisticsEntry> LocalTrafficStatisticsTable;
        /**
         * @brief Get the type ID.
         * @return the object TypeId
         */
        static TypeId GetTypeId();

        Ipv4AntNetRouting();
        ~Ipv4AntNetRouting() override;

        // These methods inherited from base class
        Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p,
                                   const Ipv4Header& header,
                                   Ptr<NetDevice> oif,
                                   Socket::SocketErrno& sockerr) override;
            
        bool RouteInput(Ptr<const Packet> p,
                        const Ipv4Header& header,
                        Ptr<const NetDevice> idev,
                        const UnicastForwardCallback& ucb,
                        const MulticastForwardCallback& mcb,
                        const LocalDeliverCallback& lcb,
                        const ErrorCallback& ecb) override;

        // These functions are not implemented yet
        void AddLocalTrafficStat (Ipv4Address dest, double dataFlowMeasure);
        void NotifyInterfaceUp(uint32_t interface) override;
        void NotifyInterfaceDown(uint32_t interface) override;
        void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
        void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
        void SetIpv4(Ptr<Ipv4> ipv4) override;
        void PrintRoutingTable(Ptr<OutputStreamWrapper> stream,
                               Time::Unit unit = Time::S) const override;

        bool VerifyRoutingTable(RoutingTable testRoutingTable,
                                LocalTrafficStatisticsTable testLocalTrafficStatsTable);

        void PrintPheromonesCsvHeader(Ptr<OutputStreamWrapper> stream) const;
        void PrintPheromonesCsv(Ptr<OutputStreamWrapper> stream) const;

        /**
         * @brief Initialize the routing table with the given list of possible destinations and their neighbours
         * Initial pheromone values will be set equally for all neighbours
         * @param destList List of all possible destination addresses and masks in the network
         * @param neighbourList List of neighbour addresses and masks for this node
         */
        void InitializeRoutingTable(
            const std::list<std::pair<Ipv4Address, Ipv4Address>>& destList, 
            const std::list<std::pair<Ipv4Address, Ipv4Address>>& neighbourList,
            int ant_send_mode,
            Ipv4Address specificDestination = Ipv4Address()
        );

        void EnableAnts();
        void DisableAnts();
        void SetDeterministicAnts(bool deterministic);
        bool GetDeterministicAnts() const;
        void SetSpecificDestination(Ipv4Address dest);
        Ipv4Address GetSpecificDestination() const;
        void SetForwardAntInterval(Time interval);

        /**
         * @brief Send a forward ant. The destination will be chosen based on local traffic statistics.
         * This will be called periodically to send forward ants (don't know how to call this yet)
         */
        void ScheduleForwardAnt();

        /**
         * @brief Send a forward ant to the specified destination
         * @param dest The destination address
         */
        virtual void SendForwardAnt(Ipv4Address dest);

    private:

        /**
         * @brief Send beacon ants to all neighbours periodically
         * The period is defined by m_beaconInterval
         */
        void SendBeacon();

        /**
         * @brief Lookup a route in the routing table for the given destination and output interface (if any)
         * Will find the route with the most matching prefix (highest mask length)
         * @param dest The destination address
         * @param oif The output interface (if any)
         * @param backwardAntLookup True if looking up route for backward ant (next hop is given deterministically), false otherwise
         * @return Pointer of the route if found, nullptr otherwise
         */
        Ptr<Ipv4Route> LookupRoute(Ipv4Address dest, Ptr<NetDevice> oif = nullptr, bool backwardAntLookup = false) const;

        /**
         * @brief Evaporate pheromone values for the given neighbour based on the evaporation entries
         * This function is only used when m_useBeaconWindow is on
         * If m_useFailureMessagePropagation is also on, this function will prepare and send failure messages to neighbours
         * @param neighbourAddr The address of the neighbour
         * @param m_evaporationEntries List of destination addresses and their evaporation factors
         */
        void EvaporateNeighbourPheromones(Ipv4Address neighbourAddr, std::list<std::pair<Ipv4Address, double>> m_evaporationEntries);

        /**
         * @brief Find routing table entry for the given destination
         * @param dest The destination address
         * @return Pointer to the routing table entry if found, nullptr otherwise
         */
        Ipv4AntNetRoutingTableEntry* FindRoutingTableEntry(Ipv4Address dest);
        
        /**
         * @brief Find traffic statistics table entry for the given destination
         * @param dest The destination address
         * @return Pointer to the traffic statistics entry if found, nullptr otherwise
         */
        Ipv4AntNetLocalTrafficStatisticsEntry* FindLocalTrafficStatisticsEntry(Ipv4Address dest);

        // Routing table
        RoutingTable m_routingTable;
        // Local traffic statistics table
        LocalTrafficStatisticsTable m_localTrafficStatsTable;
        // Pointer to the Ipv4 instance
        Ptr<Ipv4> m_ipv4;
        // Interval to send forward ants
        Time m_forwardAntInterval;
        // Event ID for scheduled forward ant sending
        EventId m_forwardAntEvent;
        // Current round number for forward ants
        uint32_t m_roundNumber;
        // Map of neighbour address to interface index
        std::map<Ipv4Address, uint32_t> m_neighbourInterfaceMap;

        // -- additional variables for AntNet routing protocol --
        // enable ants
        bool m_enableAnts = true;
        // Specific destination for deterministic forward ants (used only if deterministic mode is on)
        Ipv4Address m_specificDestination;
        // Whether to send forward ants deterministically to a specific destination
        bool m_deterministicAnts = false;
        /// -- additional variables for AntNet routing protocol --

        // Variables for beacon mechanism
        // Whether to use beacon window to detect neighbour failures
        bool m_useBeaconWindow;
        // Beacon interval for sending beacons to neighbours
        Time m_beaconInterval;
        // Event ID for scheduled beacon sending
        EventId m_beaconEvent;
        // Count of beacons sent
        uint32_t m_beaconSentCount;
        // Map of neighbour address to count of received beacons
        // The key is neighbour address
        // The value is the count of received beacons from that neighbour at the current round
        std::map<Ipv4Address, uint32_t> m_receivedBeaconsCountMap;
        // Size of the sliding window for received beacons
        uint32_t m_beaconWindowSize;
        // Map of neighbour address to sliding window of received beacon counts
        // The key is neighbour address
        // The value is a list of counts of received beacons from that neighbour in the past rounds
        // Each list has size at most m_beaconWindowSize
        std::map<Ipv4Address, std::list<uint32_t>> m_receivedBeaconsCountWindowMap;
        // A constant factor for calculation evaporation factor
        static constexpr double D = 1.35;

        // Variables for failure message propagation mechanism
        // Whether to use failure message propagation mechanism
        bool m_useFailureMessagePropagation;
        // Threshold for sending failure messages to neighbours (between 0 and 1)
        // Lower value means nodes are more likely to propogate failure messages to neighbours
        double m_failureMessageThreshold;
};

} // Namespace ns3

#endif /* IPV4_ANTNET_ROUTING_H */
