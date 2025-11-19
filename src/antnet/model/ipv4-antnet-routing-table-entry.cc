#include "ipv4-antnet-routing-table-entry.h"

#include "ns3/log.h"
#include "ns3/object.h"

#include <vector>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("Ipv4AntNetRoutingTableEntry");

NS_OBJECT_ENSURE_REGISTERED(Ipv4AntNetRoutingTableEntry);

Ipv4AntNetRoutingTableEntry::PheromoneKey 
Ipv4AntNetRoutingTableEntry::GetNextHop()
{
    // Select the next hop with random proportional to pheromone values
}

} // namespace ns3
