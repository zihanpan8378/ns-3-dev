#include "ipv4-antnet-routing-table-entry.h"

#include "ns3/log.h"
#include "ns3/object.h"

#include <vector>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("Ipv4AntNetRoutingTableEntry");

Ipv4AntNetRoutingTableEntry::Ipv4AntNetRoutingTableEntry()
{
    NS_LOG_FUNCTION(this);
}

Ipv4AntNetRoutingTableEntry::Ipv4AntNetRoutingTableEntry(const Ipv4AntNetRoutingTableEntry& route)
    : m_dest(route.m_dest),
      m_destNetworkMask(route.m_destNetworkMask),
      m_pheromoneList(route.m_pheromoneList)
{
    NS_LOG_FUNCTION(this << route);
}

Ipv4AntNetRoutingTableEntry::Ipv4AntNetRoutingTableEntry(const Ipv4AntNetRoutingTableEntry* route)
    : m_dest(route->m_dest),
      m_destNetworkMask(route->m_destNetworkMask),
      m_pheromoneList(route -> m_pheromoneList)
{
    NS_LOG_FUNCTION(this << route);
}

Ipv4AntNetRoutingTableEntry::Ipv4AntNetRoutingTableEntry(Ipv4Address dest,
                                                         Ipv4Mask destNetworkMask,
                                                         PheromoneList pheromoneList)
    : m_dest(dest),
      m_destNetworkMask(destNetworkMask),
      m_pheromoneList(pheromoneList)
{
    NS_LOG_FUNCTION(this << dest << destNetworkMask);
}

Ipv4Address
Ipv4AntNetRoutingTableEntry::GetDestAddr() const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

Ipv4Mask
Ipv4AntNetRoutingTableEntry::GetDestMask() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask;
}

Ipv4AntNetRoutingTableEntry::PheromoneList
Ipv4AntNetRoutingTableEntry::GetPheromoneList() const
{
    NS_LOG_FUNCTION(this);
    return m_pheromoneList;
}

bool
Ipv4AntNetRoutingTableEntry::HasNextHop() const
{
    NS_LOG_FUNCTION(this);
    for (const auto& entry : m_pheromoneList) {
        if (entry.first.first != Ipv4Address::GetZero()) {
            return true;
        }
    }
    return false;
}

Ipv4AntNetRoutingTableEntry::PheromoneKey
Ipv4AntNetRoutingTableEntry::GetNextHop(Ptr<NetDevice> oif) const
{
    // The current implementation does not follow exactly the AntNet algorithm
    // Please check the original paper and correct this function


    NS_LOG_FUNCTION(this);

    // If no next hops available, return zero address and interface 0 (should not happen)
    if (m_pheromoneList.empty()) {
        return Ipv4AntNetRoutingTableEntry::PheromoneKey(Ipv4Address::GetZero(), 0);
    }

    PheromoneList searchList = m_pheromoneList;
    if (oif) {
        // Filter pheromone list by output interface
        PheromoneList filteredList = {};
        for (const auto& entry : m_pheromoneList) {
            if (entry.first.second == oif->GetIfIndex()) {
                filteredList.push_back(entry);
            }
        }
        // If no entries found for the requested interface, use all entries
        if (filteredList.empty()) {
            NS_LOG_WARN("No next hops found for the requested output interface. Using all next hops.");
        } else {
            searchList = filteredList;
        }
    }
    
    // Calculate total probability
    double totalProb = 0.0;
    for (const auto& entry : searchList) {
        totalProb += entry.second;
    }
    // Generate random number between 0 and totalProb
    double randomValue = static_cast<double>(rand()) / RAND_MAX * totalProb;
    // Select based on cumulative probability
    double cumulativeProb = 0.0;
    for (const auto& entry : searchList) {
        cumulativeProb += entry.second;
        if (randomValue <= cumulativeProb) {
            return entry.first;
        }
    }
    // Return the last entry if none selected (should not happen)
    return searchList.back().first;
}

// void 
// Ipv4AntNetRoutingTableEntry::UpdatePheromone(Ipv4Address dest, Ipv4Address nextHop, Ipv4AntNetLocalTrafficStatisticsEntry trafficStat) const
// {
//     NS_LOG_FUNCTION(this << nextHop);
//     // Placeholder for pheromone update logic





// }

bool
Ipv4AntNetRoutingTableEntry::IsNetwork() const
{
    NS_LOG_FUNCTION(this);
    return !IsHost();
}

bool
Ipv4AntNetRoutingTableEntry::IsDefault() const
{
    NS_LOG_FUNCTION(this);
    return m_dest == Ipv4Address::GetZero();
}

bool
Ipv4AntNetRoutingTableEntry::IsHost() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask == Ipv4Mask::GetOnes();
}

Ipv4AntNetRoutingTableEntry
Ipv4AntNetRoutingTableEntry::CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(nextHop << interface);
    return Ipv4AntNetRoutingTableEntry(
        Ipv4Address::GetZero(), 
        Ipv4Mask::GetZero(), 
        std::list<std::pair<PheromoneKey, double>>{ 
            { PheromoneKey(nextHop, interface), 1.0 } 
        }
    );
}

Ipv4AntNetRoutingTableEntry
Ipv4AntNetRoutingTableEntry::CreateNetworkRouteTo(Ipv4Address network,
                                                  Ipv4Mask networkMask,
                                                  std::list<PheromoneKey> nextHops)
{
    NS_LOG_FUNCTION(network << networkMask);
    Ipv4AntNetRoutingTableEntry::PheromoneList pheromoneList;
    double initialPheromone = 1.0 / static_cast<double>(nextHops.size());
    for (const auto& nextHop : nextHops) {
        pheromoneList.push_back(std::make_pair(nextHop, initialPheromone));
    }
    return Ipv4AntNetRoutingTableEntry(network, networkMask, pheromoneList);
}

std::ostream&
operator<<(std::ostream& os, const Ipv4AntNetRoutingTableEntry& route) {
    if (route.IsDefault()) {
        Ipv4AntNetRoutingTableEntry::PheromoneKey nextHop = route.GetNextHop();
        os << "default out=" << nextHop.first << ", next hop=" << nextHop.second << "\n";
    } else if (route.IsHost()) {
        os << "host=" << route.GetDestAddr() << ":\n";
        for (const auto& entry : route.GetPheromoneList()) {
            os << "   next hop=" << entry.first.first << ", interface=" << entry.first.second
               << ", pheromone=" << entry.second << "\n";
        }
    } else if (route.IsNetwork()) {
        os << "network=" << route.GetDestAddr() << ", mask=" << route.GetDestMask()
           << ":\n";
        for (const auto& entry : route.GetPheromoneList()) {
            os << "   next hop=" << entry.first.first << ", interface=" << entry.first.second
               << ", pheromone=" << entry.second << "\n";
        }
    } else {
        NS_ASSERT(false);
    }
    return os;
}

bool
operator==(const Ipv4AntNetRoutingTableEntry a, const Ipv4AntNetRoutingTableEntry b)
{
    return (a.GetDestAddr() == b.GetDestAddr() && a.GetDestMask() == b.GetDestMask());
}

} // namespace ns3
