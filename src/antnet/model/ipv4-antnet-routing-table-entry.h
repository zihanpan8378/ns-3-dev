#ifndef IPV4_ANTNET_ROUTING_TABLE_ENTRY_H
#define IPV4_ANTNET_ROUTING_TABLE_ENTRY_H

#include "ns3/ipv4-address.h"

#include <list>
#include <ostream>
#include <vector>

namespace ns3
{

class Ipv4AntNetRoutingTableEntry
{

    // Similar to Ipv4RoutingTableEntry but has a list of pheromone values for different next hops
    // Check Ipv4RoutingTableEntry on how to implement basic functions

    private:

        typedef std::pair<Ipv4Address, uint32_t> PheromoneKey; // Destination address and interface index
        typedef std::list<std::pair<PheromoneKey, double>> PheromoneList;

        Ipv4Address m_dest;
        Ipv4Mask m_destNetworkMask;
        PheromoneList m_pheromoneList;

    public:
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
         * @brief Get the next hop with the highest pheromone value
         */
        PheromoneKey GetNextHop();
    
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
         * @return True if this route is a gateway route; false otherwise
         */
        bool HasGateway() const;
        /**
         * @return address of the gateway stored in this entry
         */
        Ipv4Address GetGateway() const;
        /**
         * @return The IPv4 address of the destination of this route
         */
        Ipv4Address GetDest() const;
        /**
         * @return The IPv4 network number of the destination of this route
         */
        Ipv4Address GetDestNetwork() const;
        /**
         * @return The IPv4 network mask of the destination of this route
         */
        Ipv4Mask GetDestNetworkMask() const;
        /**
         * @return The Ipv4 interface number used for sending outgoing packets
         */
        uint32_t GetInterface() const;

    
    
};

}

#endif /* IPV4_ANTNET_ROUTING_TABLE_ENTRY_H */