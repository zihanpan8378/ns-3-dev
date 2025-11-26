#include "ipv4-antnet-routing.h"
#include "ant-header.h"

#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/timestamp-tag.h"
#include "ns3/ipv4-route.h"

#include <iomanip>
#include <vector>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("Ipv4AntNetRouting");

NS_OBJECT_ENSURE_REGISTERED(Ipv4AntNetRouting);

TypeId
Ipv4AntNetRouting::GetTypeId()
{
    static TypeId tid = 
        TypeId("ns3::Ipv4AntNetRouting")
            .SetParent<Ipv4RoutingProtocol>()
            .SetGroupName("Internet")
            .AddConstructor<Ipv4AntNetRouting>();
            // Additional attributes can be added here
    return tid;
}

Ipv4AntNetRouting::Ipv4AntNetRouting()
    : m_ipv4(nullptr)
{
    NS_LOG_FUNCTION(this);
}

Ipv4AntNetRouting::~Ipv4AntNetRouting()
{
    NS_LOG_FUNCTION(this);
}

void 
Ipv4AntNetRouting::NotifyInterfaceUp(uint32_t interface) {
    NS_LOG_FUNCTION(this << interface);
    // for (uint32_t j = 0; j < m_ipv4->GetNAddresses(interface); ++j) {
    //     Ipv4Address addr = m_ipv4->GetAddress(interface, j).GetLocal();
    //     Ipv4Mask mask = m_ipv4->GetAddress(interface, j).GetMask();

    //     if (addr != Ipv4Address() && mask != Ipv4Mask() && mask != Ipv4Mask::GetOnes()) {
    //         Ipv4AntNetRoutingTableEntry* routeEntry = FindRoutingTableEntry(addr);
    //         if (routeEntry) {
    //             NS_LOG_LOGIC("Interface " << interface << " address " << addr << " already in routing table");
    //             continue;
    //         } else {
    //             NS_LOG_LOGIC("Adding interface " << interface << " address " << addr << " to routing table");
    //             Ipv4AntNetRoutingTableEntry newEntry(
    //                 addr.CombineMask(mask),
    //                 mask,
    //                 Ipv4AntNetRoutingTableEntry::PheromoneList()
    //             );
    //             m_routingTable.push_back(newEntry);
    //         }
    //     }

    //     NS_LOG_LOGIC("Interface " << interface << " is up with address " << addr);
    // }
}

void 
Ipv4AntNetRouting::NotifyInterfaceDown(uint32_t interface) {
    NS_LOG_FUNCTION(this << interface);
    



}

void 
Ipv4AntNetRouting::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) {
    NS_LOG_FUNCTION(this << interface << " " << address.GetLocal());




}

void
Ipv4AntNetRouting::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) {
    NS_LOG_FUNCTION(this << interface << " " << address.GetLocal());




}

void
Ipv4AntNetRouting::SetIpv4(Ptr<Ipv4> ipv4)
{
    NS_LOG_FUNCTION(this << ipv4);
    NS_ASSERT(!m_ipv4 && ipv4);
    m_ipv4 = ipv4;
    for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++)
    {
        if (m_ipv4->IsUp(i))
        {
            NotifyInterfaceUp(i);
        }
        else
        {
            NotifyInterfaceDown(i);
        }
    }
}

void
Ipv4AntNetRouting::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
    NS_LOG_FUNCTION(this << stream);

    *stream->GetStream() << "AntNet Routing Table and Local Traffic Stat for Node " << m_ipv4->GetObject<Node>()->GetId() << std::endl;
    *stream->GetStream() << "    Routing Table Entries:" << std::endl;
    *stream->GetStream() << "        --------------------------------------------------------" << std::endl;
    for (const auto& entry : m_routingTable) {
        *stream->GetStream() << "        " << entry.ToString();
    }
    *stream->GetStream() << "        --------------------------------------------------------" << std::endl;

    *stream->GetStream() << "    Local Traffic Statistics Entries:" << std::endl;
    *stream->GetStream() << "        --------------------------------------------------------" << std::endl;
    for (const auto& entry : m_localTrafficStatsTable) {
        *stream->GetStream() << "        " << entry.ToString();
    }
    *stream->GetStream() << "        --------------------------------------------------------" << std::endl;
    *stream->GetStream() << std::endl;
}


Ptr<Ipv4Route>
Ipv4AntNetRouting::RouteOutput(Ptr<Packet> p,
                               const Ipv4Header& header,
                               Ptr<NetDevice> oif,
                               Socket::SocketErrno& sockerr)
{
    NS_LOG_FUNCTION(this << p << header << oif << sockerr);

    // Get the destination address from the header
    Ipv4Address destination = header.GetDestination();
    // Lookup route for the destination
    Ptr<Ipv4Route> rtentry = LookupRoute(destination, oif);
    // Set socket error based on route lookup result
    sockerr = rtentry ? Socket::ERROR_NOTERROR : Socket::ERROR_NOROUTETOHOST;

    return rtentry;
}

bool
Ipv4AntNetRouting::RouteInput(Ptr<const Packet> p,
                               const Ipv4Header& header,
                               Ptr<const NetDevice> idev,
                               const UnicastForwardCallback& ucb,
                               const MulticastForwardCallback& mcb,
                               const LocalDeliverCallback& lcb,
                               const ErrorCallback& ecb)
{
    NS_LOG_FUNCTION(this << header);

    // Check that we have an Ipv4 object
    NS_ASSERT(m_ipv4);
    // Check if input device supports IP
    NS_ASSERT(m_ipv4->GetInterfaceForDevice(idev) >= 0);
    // Get the interface for the input device
    uint32_t iif = m_ipv4->GetInterfaceForDevice(idev);

    // Handle AntNet protocol packets
    // Protocol number is 253 is used for AntNet for testing purposes
    if (header.GetProtocol() == PROTOCOL_ANTNET) {
        // Handle AntNet packet

        // Make a copy of the packet to remove header
        Ptr<Packet> copy = p->Copy();
        AntHeader antHeader;
        bool ok = copy->RemoveHeader(antHeader);
        if (!ok) {
            NS_LOG_ERROR("Failed to remove AntHeader");
            return false;
        }
        
        // Process based on ant type
        AntHeader::Type antType = antHeader.GetAntType();
        if (antType == AntHeader::Type::FORWARD_ANT) {
            // Process forward ant
            NS_LOG_LOGIC("Processing forward ant to " << header.GetDestination());

            // Probably we need to move this stack append to the sending time, and use oif address
            // since we will use oif to receive the backward ant
            // Or probably we should have both oif address and iif address in the stack entry
            antHeader.AddForwardHop(m_ipv4->GetAddress(iif, 0).GetLocal(), Simulator::Now());

            // Check if the current node is the destination
            if (m_ipv4->IsDestinationAddress(header.GetDestination(), iif)) {
                // Process forward ant at destination
                // Flip to backward ant and send back
                NS_LOG_LOGIC("Forward ant reached destination " << header.GetDestination() << ", converting to backward ant");
                
                // Convert forward ant to backward ant
                antHeader.SetAntType(AntHeader::Type::BACKWARD_ANT);
                antHeader.PopForwardStackEntryToBackwardStack(); // Move destination from forward to backward stack
                
                // Send backward ant to next hop
                AntHeader::AntHeaderStackEntry nextHop = antHeader.PopForwardStackEntryToBackwardStack(); // Get next hop and move it to backward stack
                Ipv4Address nextHopAddr = nextHop.GetAddress();
                // Since backward ants are sent throught the original path
                // we lookup route to next hop (previous hop when forward) rather than the destination (source node when forward)
                Ptr<Ipv4Route> route = LookupRoute(nextHopAddr);
                if (route) {
                    // Make a new packet for backward ant
                    Ptr<Packet> back_packet = Create<Packet>();

                    // Backward ants get high priority than forward ants and normal packets
                    SocketPriorityTag priorityTag;
                    priorityTag.SetPriority(7);
                    back_packet->AddPacketTag(priorityTag);

                    back_packet->AddHeader(antHeader);
                    m_ipv4->Send(back_packet, route->GetSource(), nextHopAddr, PROTOCOL_ANTNET, route);
                    return true;
                } else {
                    NS_LOG_ERROR("No route found for backward ant to " << nextHopAddr);
                    return false;
                }
            } else {
                // Relay forward ant
                Ptr<Ipv4Route> route = LookupRoute(header.GetDestination());
                if (route) {
                    copy->AddHeader(antHeader);
                    ucb(route, copy, header);
                    return true;
                } else {
                    NS_LOG_ERROR("No route found for forward ant");
                    return false;
                }
            }
        } else if (antType == AntHeader::Type::BACKWARD_ANT) {
            // Process backward ant
            NS_LOG_LOGIC("Processing backward ant to " << header.GetDestination());

            // Update local traffic statistics and routing table pheromones
            std::vector<AntHeader::AntHeaderStackEntry> backwardStack = antHeader.GetBackwardStack();
            AntHeader::AntHeaderStackEntry antDestinationEntry = backwardStack.front();
            Ipv4Address antDestinationAddr = antDestinationEntry.GetAddress();
            Time antDestinationTime = antDestinationEntry.GetTime();
            Time delay = antDestinationTime - backwardStack.back().GetTime();
            
            // Update statistics for destination
            Ipv4AntNetLocalTrafficStatisticsEntry* trafficEntry = FindLocalTrafficStatisticsEntry(antDestinationAddr);
            if (trafficEntry) {
                // Call the traffic stat entry's update method
                trafficEntry->UpdateStatistics(delay);
                NS_LOG_LOGIC("Updated local traffic statistics for destination " << antDestinationAddr);
            } else {
                NS_LOG_ERROR("No local traffic statistics entry found for destination " << antDestinationAddr);
                return false;
            }
            
            // Update routing table pheromones for destination
            Ipv4Address nextForwardHopAddr = backwardStack[backwardStack.size() - 2].GetAddress();
            Ipv4AntNetRoutingTableEntry* routeEntry = FindRoutingTableEntry(antDestinationAddr);
            if (routeEntry) {
                // Call the routing table entry's pheromone update method
                routeEntry->UpdatePheromone(antDestinationAddr, nextForwardHopAddr, *trafficEntry);
                NS_LOG_LOGIC("Updated routing table pheromones for destination " << antDestinationAddr);
                return true;
            } else {
                NS_LOG_ERROR("No routing table entry found for destination " << antDestinationAddr);
                return false;
            }
            
            // This part is commented out for now for simplicity and can be enabled later if needed
            // // Check if a subpath route is good enough
            // // For good subpath, also update pheromones for the corresponding destination
            // if (backwardStack.size() > 2) {
            //     for (size_t idx = backwardStack.size() - 2; idx > 0; --idx) {
            //         Ipv4Address subpathDest = backwardStack[idx].GetAddress();
            //         Time subpathDelay = backwardStack[idx].GetTime() - backwardStack.back().GetTime();
            //         double delayMs = (subpathDelay).GetMilliSeconds();
            //         Ipv4AntNetLocalTrafficStatisticsEntry* subPathTrafficEntry = FindLocalTrafficStatisticsEntry(subpathDest);
            //         if (subPathTrafficEntry) {
            //             if (delayMs <= subPathTrafficEntry->GetUpperBoundDelayFromWindow()) {
            //                 NS_LOG_LOGIC("Subpath to " << subpathDest << " is good enough with delay " << delayMs << " ms, updating pheromone");
            //                 Ipv4Address subpathNextHopAddr = backwardStack[idx - 1].GetAddress();
            //                 Ipv4AntNetRoutingTableEntry* subpathRouteEntry = FindRoutingTableEntry(subpathDest);
            //                 if (subpathRouteEntry) {
            //                     subpathRouteEntry->UpdatePheromone(subpathDest, subpathNextHopAddr, *subPathTrafficEntry);
            //                     NS_LOG_LOGIC("Updated routing table pheromones for subpath destination " << subpathDest);
            //                 } else {
            //                     NS_LOG_ERROR("No routing table entry found for subpath destination " << subpathDest);
            //                     return false;
            //                 }
            //             }
            //         } else {
            //             NS_LOG_ERROR("No local traffic statistics entry found for subpath destination " << subpathDest);
            //             return false;
            //         }
            //     }
            // }

            // Check if reached source by looking if forward stack is empty
            if (antHeader.GetForwardStack().empty()) {
                NS_LOG_LOGIC("Backward ant reached source " << header.GetDestination());
                return true;
            }

            // Relay backward ant to next hop
            AntHeader::AntHeaderStackEntry nextHop = antHeader.PopForwardStackEntryToBackwardStack();
            Ipv4Address nextBackwardHopAddr = nextHop.GetAddress();
            // Since backward ants are sent throught the original path
            // we lookup route to next hop (previous hop when forward) rather than the destination (source node when forward)
            Ptr<Ipv4Route> route = LookupRoute(nextBackwardHopAddr);
            if (route) {
                // Backward ants get high priority than forward ants and normal packets
                SocketPriorityTag priorityTag;
                priorityTag.SetPriority(7);
                copy->AddPacketTag(priorityTag);

                copy->AddHeader(antHeader);
                ucb(route, copy, header);
                return true;
            } else {
                NS_LOG_ERROR("No route found for backward ant to " << nextBackwardHopAddr);
                return false;
            }
        } else {
            NS_LOG_ERROR("Unknown Ant type");
            return false;
        }
    }

    // Handle normal packet
    NS_LOG_LOGIC("Processing normal packet to " << header.GetDestination());

    // Check for local delivery
    if (m_ipv4->IsDestinationAddress(header.GetDestination(), iif)) {
        if (!lcb.IsNull()) {
            NS_LOG_LOGIC("Local delivery to " << header.GetDestination());
            lcb(p, header, iif);
            return true;
        } else {
            return false;
        }
    }

    // Check if input device supports IP forwarding
    if (!m_ipv4->IsForwarding(iif))
    {
        NS_LOG_LOGIC("Forwarding disabled for this interface");
        ecb(p, header, Socket::ERROR_NOROUTETOHOST);
        return true;
    }

    // Add data flow measure for the destination in the local traffic statistics table
    Ipv4AntNetLocalTrafficStatisticsEntry* entry = FindLocalTrafficStatisticsEntry(header.GetDestination());
    if (entry) {
        entry->AddDataFlowMeasure();
    } else {
        NS_LOG_ERROR("No local traffic statistics entry found for destination " << header.GetDestination());
        return false;
    }

    // Try find a route by calling LookupRoute and forward using ucb
    Ptr<Ipv4Route> route = LookupRoute(header.GetDestination());
    if (route) {
        NS_LOG_LOGIC("Found unicast destination- calling unicast callback");
        ucb(route, p, header);
        return true;
    } else {
       NS_LOG_LOGIC("Did not find unicast destination- returning false");
        return false;
    }
}

void Ipv4AntNetRouting::ScheduleForwardAnt() {
    NS_LOG_FUNCTION(this);
    // Select destination based on local traffic statistics using weighted random selection
    // The weight is the data flow measure
    double sum_weights = 0.0;
    for (const auto& entry : m_localTrafficStatsTable) {
        sum_weights += entry.GetDataFlowMeasure();
    }
    double randomValue = static_cast<double>(rand()) / RAND_MAX * sum_weights;
    double cumulative_weight = 0.0;
    for (const auto& entry : m_localTrafficStatsTable) {
        cumulative_weight += entry.GetDataFlowMeasure();
        if (randomValue <= cumulative_weight) {
            Ipv4Address dest = entry.GetDestAddr();
            SendForwardAnt(dest);
            break;
        }
    }
}

void Ipv4AntNetRouting::InitializeRoutingTable(
    const std::list<std::pair<Ipv4Address, Ipv4Address>>& destList, 
    const std::list<std::pair<Ipv4Address, Ipv4Address>>& neighbourList) {
    NS_LOG_FUNCTION(this);

    // Clear existing routing table and local traffic statistics table
    m_routingTable.clear();
    m_localTrafficStatsTable.clear();

    // Initial pheromone value, equally distributed among neighbours
    double initialPheromone = 1.0 / neighbourList.size();

    // Initialize routing table entries
    for (const auto& destAddrAndMask : destList) {
        Ipv4Address destAddr = destAddrAndMask.first;
        Ipv4Mask destMask = Ipv4Mask(destAddrAndMask.second.Get());

        // Prepare local traffic statistics entry for this destination
        Ipv4AntNetLocalTrafficStatisticsEntry trafficEntry(destAddr, destMask);
        m_localTrafficStatsTable.push_back(trafficEntry);

        // Prepare pheromone list for this node
        Ipv4AntNetRoutingTableEntry::PheromoneList pheromoneList;
        for (const auto& neighbourAddrAndMask : neighbourList) {
            Ipv4Address neighbourAddr = neighbourAddrAndMask.first;
            Ipv4Mask neighbourMask = Ipv4Mask(neighbourAddrAndMask.second.Get());

            // Get the interface that can reach this neighbour
            // Iterate through interfaces to find the matching one
            uint32_t interfaceToNeighbour = 0;
            for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
                Ptr<NetDevice> device = m_ipv4->GetNetDevice(i);
                if (device && device->IsPointToPoint()) {
                    bool foundInterface = false;
                    // Check if this interface's address matches the neighbour's subnet
                    for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); ++j) {
                        Ipv4Address ifaceAddr = m_ipv4->GetAddress(i, j).GetLocal();
                        if (neighbourMask.IsMatch(ifaceAddr, neighbourAddr)) {
                            interfaceToNeighbour = i;
                            foundInterface = true;
                            break;
                        }
                    }
                    if (foundInterface) {
                        break;
                    }
                }
            }
            // Add this neighbour to the pheromone list with initial pheromone value
            Ipv4AntNetRoutingTableEntry::PheromoneKey pheromoneKey = std::make_pair(neighbourAddr, interfaceToNeighbour);
            pheromoneList.push_back(std::make_pair(pheromoneKey, initialPheromone));
        }
        // Create routing table entry and add to routing table
        Ipv4AntNetRoutingTableEntry newEntry(destAddr, destMask, pheromoneList);
        m_routingTable.push_back(newEntry);
    }
}

void Ipv4AntNetRouting::SendForwardAnt(Ipv4Address dest) const {
    NS_LOG_FUNCTION(this << dest);

    // Create a new packet for the forward ant
    Ptr<Packet> p = Create<Packet>();

    // Lookup route to thedestination
    Ptr<Ipv4Route> route = LookupRoute(dest);
    if (!route) {
        NS_LOG_ERROR("No route to send forward ant to " << dest);
        return;
    }

    // Create and init AntHeader
    AntHeader antHeader;
    Ipv4Address source = route->GetSource();
    antHeader.AddForwardHop(source, Simulator::Now()); // Initial hop with current node and current time
    antHeader.SetAntType(AntHeader::Type::FORWARD_ANT);
    p->AddHeader(antHeader);

    // Call Ipv4 L3 protocol's Send method to send the packet
    m_ipv4->Send(p, source, dest, PROTOCOL_ANTNET, route);
}

Ptr<Ipv4Route> Ipv4AntNetRouting::LookupRoute(Ipv4Address dest, Ptr<NetDevice> oif) const {
    NS_LOG_FUNCTION(this << dest);

    // Initialize route entry pointer to null
    Ptr<Ipv4Route> rtentry = nullptr;

    // The longest prefix match is used to find the route entry with the most specific destination address
    uint16_t longest_mask = 0;
    
    // Ignore multicast and broadcast addresses for now
    if (dest.IsMulticast() || dest.IsBroadcast()) {
        return rtentry;
    }

    // For unicast, search routing table for the best matching route
    for (auto i = m_routingTable.begin(); i != m_routingTable.end(); ++i) {
        Ipv4AntNetRoutingTableEntry entry = *i;
        Ipv4Address entryDest = entry.GetDestAddr();
        Ipv4Mask entryMask = entry.GetDestMask();
        uint16_t masklen = entryMask.GetPrefixLength();
        NS_LOG_LOGIC("Checking route to " << dest << ", checking against route to " << entryDest 
                                          << "/" << masklen);

        // Check if this entry matches the destination
        if (entryMask.IsMatch(dest, entryDest)) {
            NS_LOG_LOGIC("Found global nestwork route to " << entryDest << "/" << masklen);

            // Call the entry's GetNextHop method to get the next hop for this destination
            Ipv4AntNetRoutingTableEntry::PheromoneKey nextHop = entry.GetNextHop(oif); // A pair of next hop address and interface index
            Ipv4Address nextHopAddr = nextHop.first;
            uint32_t nextHopInterface = nextHop.second;
            
            // If oif is specified, check if it matches the selected interface
            if (oif) {
                if (oif != m_ipv4->GetNetDevice(nextHopInterface)) {
                    NS_LOG_LOGIC("Not on requested interface, skipping");
                    continue;
                }
            }

            // Check if this route has the longest mask so far
            if (masklen < longest_mask) { // Not interested if got shorter mask
                NS_LOG_LOGIC("Previous match longer, skipping");
                continue;
            }
            longest_mask = masklen;

            // Create the Ipv4Route object for this route entry
            rtentry = Create<Ipv4Route>();
            rtentry->SetDestination(entry.GetDestAddr());
            rtentry->SetSource(m_ipv4->SourceAddressSelection(nextHopInterface, entry.GetDestAddr()));
            rtentry->SetGateway(nextHopAddr);
            rtentry->SetOutputDevice(m_ipv4->GetNetDevice(nextHopInterface));
            
            // Break the loop if the current route is an exact match
            if (masklen == 32) { 
                break;
            }
        }
    }
    if (rtentry)
    {
        NS_LOG_LOGIC("Matching route via " << rtentry->GetGateway() << " at the end");
    }
    else
    {
        NS_LOG_LOGIC("No matching route to " << dest << " found");
    }
    return rtentry;
}

Ipv4AntNetRoutingTableEntry*
Ipv4AntNetRouting::FindRoutingTableEntry(Ipv4Address dest)
{
    for (auto i = m_routingTable.begin(); i != m_routingTable.end(); ++i) {
        if (i->GetDestAddr() == dest) {
            return &(*i);
        }
    }
    return nullptr;
}

Ipv4AntNetLocalTrafficStatisticsEntry*
Ipv4AntNetRouting::FindLocalTrafficStatisticsEntry(Ipv4Address dest)
{
    for (auto i = m_localTrafficStatsTable.begin(); i != m_localTrafficStatsTable.end(); ++i) {
        if (i->GetDestAddr() == dest) {
            return &(*i);
        }
    }
    return nullptr;
}

}
