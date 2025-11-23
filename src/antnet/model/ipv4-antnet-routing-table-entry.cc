#include "ipv4-antnet-routing-table-entry.h"

#include "ns3/log.h"
#include "ns3/object.h"

#include <vector>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("Ipv4AntNetRoutingTableEntry");

// NS_OBJECT_ENSURE_REGISTERED(Ipv4AntNetRoutingTableEntry);

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

Ipv4Address
Ipv4AntNetRoutingTableEntry::GetDest() const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

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

Ipv4Address
Ipv4AntNetRoutingTableEntry::GetDestNetwork() const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

Ipv4Mask
Ipv4AntNetRoutingTableEntry::GetDestNetworkMask() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask;
}

bool
Ipv4AntNetRoutingTableEntry::HasGateway() const
{
    NS_LOG_FUNCTION(this);
    return m_gateway != Ipv4Address::GetZero();
}

Ipv4Address
Ipv4AntNetRoutingTableEntry::GetGateway() const
{
    NS_LOG_FUNCTION(this);
    return m_gateway;
}

uint32_t
Ipv4AntNetRoutingTableEntry::GetInterface() const
{
    NS_LOG_FUNCTION(this);
    return m_interface;
}

bool
Ipv4AntNetRoutingTableEntry::IsHost() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask == Ipv4Mask::GetOnes();
}

} // namespace ns3
