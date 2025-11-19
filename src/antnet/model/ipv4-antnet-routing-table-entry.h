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
        Ipv4Address m_destNetworkMask;
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

    
    
};

}

#endif /* IPV4_ANTNET_ROUTING_TABLE_ENTRY_H */