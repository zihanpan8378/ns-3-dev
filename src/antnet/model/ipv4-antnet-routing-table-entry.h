#ifndef IPV4_ANTNET_ROUTING_TABLE_ENTRY_H
#define IPV4_ANTNET_ROUTING_TABLE_ENTRY_H

#include "ns3/ipv4-address.h"
#include "ns3/net-device.h"
#include "ipv4-antnet-local-traffic-statistics-entry.h"

#include <list>
#include <map>
#include <ostream>
#include <vector>

namespace ns3
{

class Ipv4AntNetRoutingTableEntry
{

    // Similar to Ipv4RoutingTableEntry but has a list of pheromone values for different next hops

    public:
        typedef std::pair<Ipv4Address, uint32_t> PheromoneKey; // Destination address and interface index
        typedef std::list<std::pair<PheromoneKey, double>> PheromoneList;
        typedef std::map<PheromoneKey, bool> VisitedNextHopMap;

        /**
         * @brief This constructor does nothing
         */
        Ipv4AntNetRoutingTableEntry();
        /**
         * @brief Copy Constructor
         * @param route The route to copy
         */
        Ipv4AntNetRoutingTableEntry(const Ipv4AntNetRoutingTableEntry& route);
        /**
         * @brief Copy Constructor
         * @param route The route to copy
         */
        Ipv4AntNetRoutingTableEntry(const Ipv4AntNetRoutingTableEntry* route);
        /**
         * @brief Constructor.
         * @param network network address
         * @param mask network mask
         * @param pheromoneList list of possible next hops with pheromone values
         */
        Ipv4AntNetRoutingTableEntry(Ipv4Address dest, 
                                    Ipv4Mask destNetworkMask, 
                                    PheromoneList pheromoneList);


        /**
         * @return String representation of this route
         */
        std::string ToString() const;

        
        /**
         * @return The IPv4 address of the destination of this route
         */
        Ipv4Address GetDestAddr() const;
        /**
         * @return The IPv4 network number of the destination of this route
         */
        Ipv4Mask GetDestMask() const;
        /**
         * @return The pheromone list of this route
         */
        PheromoneList GetPheromoneList() const;


        /**
         * @return True if this route is a gateway route; false otherwise
         */
        bool HasNextHop() const;
        /**
         * @brief Get the next hop with the highest pheromone value
         * The pheromone value is adjusted based on the current queue lengths at each next hop if provided
         * @param queueLengthMap map of next hop addresses to their current queue lengths
         * @param oif requested output interface if any (put nullptr otherwise)
         */
        PheromoneKey GetNextHop(std::map<Ipv4Address, double> queueLengthMap = std::map<Ipv4Address, double>(), Ptr<NetDevice> oif = nullptr) const;
        /**
         * @brief Get the next hop deterministically based on the given next hop address
         * @param nextHopAddr The address of the next hop
         * @return The next hop (address and out interface) with the highest pheromone value
         */
        PheromoneKey GetDeterministicNextHop(Ipv4Address nextHopAddr) const;
        /**
         * @brief Update pheromone values for the given destination and nextHop address.
         * @param nextHop The address of the next hop the packet was sent to
         * @param delayMillisecond The delay experienced to reach the destination in milliseconds
         * @param trafficStat The local traffic statistics entry for this destination
         */
        void UpdatePheromone(Ipv4Address nextHop, double delayMillisecond, Ipv4AntNetLocalTrafficStatisticsEntry trafficStat);

        /**
         * @brief Evaporate pheromone values for the given next hop address.
         * @param nextHop The address of the next hop
         * @param evaporationFactor The evaporation factor (between D - 1 and 1), where D is 1.35
         * @return The effect of evaporation on the pheromone value (used for failure message propagation)
         */
        double EvaporatePheromone(Ipv4Address nextHop, double evaporationFactor);
        

        /**
         * @return True if this route is a host route (mask of all ones); false otherwise
         */
        bool IsHost() const;
        /**
         * @return True if this route is not a host route (mask is not all ones); false otherwise
         *
         * This method is implemented as !IsHost ().
         */
        bool IsNetwork() const;
        /**
         * @return True if this route is a default route; false otherwise
         */
        bool IsDefault() const;


        /**
         * @return An Ipv4AntNetRoutingTableEntry object corresponding to the input
         * parameters.  This route is distinguished; it will match any
         * destination for which a more specific route does not exist.
         * This route will have a single next hop with the given parameters
         * that has pheromone value of 1.0
         * @param nextHop Ipv4Address of the next hop
         * @param interface Outgoing interface
         */
        static Ipv4AntNetRoutingTableEntry CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface);
  
        /**
         * @return An Ipv4AntNetRoutingTableEntry object corresponding to the input parameters.
         * A list of possible next hops is provided, will be assigned equal pheromone values initially
         * @param network Ipv4Address of the destination network
         * @param networkMask Ipv4Mask of the destination network mask
         * @param nextHops List of possible next hops and interfaces
         */
        static Ipv4AntNetRoutingTableEntry CreateNetworkRouteTo(Ipv4Address network,
                                                                Ipv4Mask networkMask,
                                                                std::list<PheromoneKey> nextHops);

    private:
        Ipv4Address m_dest;
        Ipv4Mask m_destNetworkMask;
        mutable PheromoneList m_pheromoneList;
        mutable VisitedNextHopMap m_visitedNextHops;

        static constexpr double ALPHA = 0.3; // Probability correction coefficient
        static constexpr double C1 = 0.7;    // Pheromone update constant 1
        static constexpr double C2 = 0.3;    // Pheromone update constant 2
};

/**
 * @brief Stream insertion operator.
 *
 * @param os the reference to the output stream
 * @param route the Ipv4 AntNet routing table entry
 * @returns the reference to the output stream
 */
std::ostream& operator<<(std::ostream& os, const Ipv4AntNetRoutingTableEntry& route);

/**
 * @brief Equality operator.
 *
 * @param a lhs
 * @param b rhs
 * @returns true if operands are equal, false otherwise
 */
bool operator==(const Ipv4AntNetRoutingTableEntry a, const Ipv4AntNetRoutingTableEntry b);


}

#endif /* IPV4_ANTNET_ROUTING_TABLE_ENTRY_H */