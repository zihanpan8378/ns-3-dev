#ifndef IPV4_ANTNET_ROUTING_H
#define IPV4_ANTNET_ROUTING_H

#include "ipv4-header.h"
#include "ipv4-routing-protocol.h"
#include "ipv4.h"
#include "ipv4-antnet-routing-table-entry.h"
#include "ipv4-antnet-local-traffic-statistics-entry.h"

#include "ns3/ipv4-address.h"
#include "ns3/ptr.h"

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
        void NotifyInterfaceUp(uint32_t interface);
        void NotifyInterfaceDown(uint32_t interface);
        void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address);
        void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address);
        void SetIpv4(Ptr<Ipv4> ipv4);
        void PrintRoutingTable(Ptr<OutputStreamWrapper> stream,
                               Time::Unit unit = Time::S) const override;

        /**
         * @brief Send a forward ant. The destination will be chosen based on local traffic statistics.
         * This will be called periodically to send forward ants (don't know how to call this yet)
         */
        void ScheduleForwardAnt();

    private:
        /**
         * @brief Send a forward ant to the specified destination
         * @param dest The destination address
         */
        void SendForwardAnt(Ipv4Address dest) const;

        /**
         * @brief Lookup a route in the routing table for the given destination and output interface (if any)
         * Will find the route with the most matching prefix (highest mask length)
         * @param dest The destination address
         * @param oif The output interface (if any)
         * @return Pointer of the route if found, nullptr otherwise
         */
        Ptr<Ipv4Route> LookupRoute(Ipv4Address dest, Ptr<NetDevice> oif = nullptr) const;

        /**
         * @brief Find routing table entry for the given destination
         * @param dest The destination address
         * @return Pointer to the routing table entry if found, nullptr otherwise
         */
        Ptr<Ipv4AntNetRoutingTableEntry> FindRoutingTableEntry(Ipv4Address dest) const;
        
        /**
         * @brief Find traffic statistics table entry for the given destination
         * @param dest The destination address
         * @return Pointer to the traffic statistics entry if found, nullptr otherwise
         */
        Ptr<Ipv4AntNetLocalTrafficStatisticsEntry> FindLocalTrafficStatisticsEntry(Ipv4Address dest) const;


        typedef std::list<Ipv4AntNetRoutingTableEntry> RoutingTable;
        typedef std::list<Ipv4AntNetLocalTrafficStatisticsEntry> LocalTrafficStatisticsTable;

        RoutingTable m_routingTable;
        LocalTrafficStatisticsTable m_localTrafficStatsTable;
        Ptr<Ipv4> m_ipv4;
};

} // Namespace ns3

#endif /* IPV4_ANTNET_ROUTING_H */