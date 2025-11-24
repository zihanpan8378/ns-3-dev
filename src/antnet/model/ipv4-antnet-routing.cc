#include "ipv4-antnet-routing.h"
#include "ipv4-route.h"
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
{
    NS_LOG_FUNCTION(this);
    // Constructor implementation
    // Not impolemented yet



}

Ipv4AntNetRouting::~Ipv4AntNetRouting()
{
    NS_LOG_FUNCTION(this);
    // Destructor implementation
    // Not implemented yet

    

}

Ptr<Ipv4Route>
Ipv4AntNetRouting::RouteOutput(Ptr<Packet> p,
                               const Ipv4Header& header,
                               Ptr<NetDevice> oif,
                               Socket::SocketErrno& sockerr)
{
    NS_LOG_FUNCTION(this << p << header << oif << sockerr);
    Ipv4Address destination = header.GetDestination();
    
    // Lookup route for the destination
    Ptr<Ipv4Route> rtentry = nullptr;
    rtentry = LookupRoute(destination, oif);

    if (rtentry)
    {
        sockerr = Socket::ERROR_NOTERROR;
    }
    else
    {
        sockerr = Socket::ERROR_NOROUTETOHOST;
    }
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

    NS_ASSERT(m_ipv4);
    // Check if input device supports IP
    NS_ASSERT(m_ipv4->GetInterfaceForDevice(idev) >= 0);
    uint32_t iif = m_ipv4->GetInterfaceForDevice(idev);

    if (header.GetProtocol() == PROTOCOL_ANTNET) {
        Ptr<Packet> copy = p->Copy();
        AntHeader antHeader;
        bool ok = copy->RemoveHeader(antHeader);
        if (!ok) {
            NS_LOG_ERROR("Failed to remove AntHeader");
            return false;
        }
        
        AntHeader::Type antType = antHeader.GetAntType();
        if (antType == AntHeader::Type::FORWARD_ANT) {
            // Probably we need to move this stack append to the sending time, and use oif address
            // since we will use oif to receive the backward ant
            // Or probably we should have both oif address and iif address in the stack entry
            antHeader.AddForwardHop(m_ipv4->GetAddress(iif, 0).GetLocal(), Simulator::Now());

            if (m_ipv4->IsDestinationAddress(header.GetDestination(), iif)) {
                // Process forward ant at destination
                // Flip to backward ant and send back
                NS_LOG_LOGIC("Forward ant reached destination " << header.GetDestination() << ", converting to backward ant");
                antHeader.SetAntType(AntHeader::Type::BACKWARD_ANT);
                antHeader.PopForwardStackEntryToBackwardStack(); // Move destination from forward to backward stack
                AntHeader::AntHeaderStackEntry nextHop = antHeader.PopForwardStackEntryToBackwardStack(); // Get next hop and move it to backward stack
                Ipv4Address nextHopAddr = nextHop.GetAddress();
                Ptr<Ipv4Route> route = LookupRoute(nextHopAddr);
                if (route) {
                    // Backward ants get high priority than forward ants and normal packets
                    SocketPriorityTag priorityTag;
                    priorityTag.SetPriority(7);
                    copy->AddPacketTag(priorityTag);

                    copy->AddHeader(antHeader);
                    ucb(route, copy, header);
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
            std::vector<AntHeader::AntHeaderStackEntry> backwardStack = antHeader.GetBackwardStack();
            AntHeader::AntHeaderStackEntry antDestinationEntry = backwardStack.front();
            Ipv4Address antDestinationAddr = antDestinationEntry.GetAddress();
            Time antDestinationTime = antDestinationEntry.GetTime();
            Time delay = backwardStack.back().GetTime() - antDestinationTime;
            NS_LOG_LOGIC("Backward ant reached " << header.GetDestination() << ", delay to destination: " << delay.GetMilliSeconds() << " ms");
            // Update statistics for destination
            Ptr<Ipv4AntNetLocalTrafficStatisticsEntry> trafficEntry = nullptr;
            for (auto i = m_localTrafficStatsTable.begin(); i != m_localTrafficStatsTable.end(); ++i) {
                trafficEntry = &(*i);
                if (trafficEntry->GetDestAddr() == antDestinationAddr) {
                    trafficEntry->UpdateStatistics(delay);
                    NS_LOG_LOGIC("Updated local traffic statistics for destination " << antDestinationAddr);
                    break;
                }
            }
            // Update routing table pheromones for destination
            Ipv4Address nextHopAddr = backwardStack[backwardStack.size() - 2].GetAddress();
            for (auto i = m_routingTable.begin(); i != m_routingTable.end(); ++i) {
                Ipv4AntNetRoutingTableEntry& routeEntry = *i;
                if (routeEntry.GetDestAddr() == antDestinationAddr && routeEntry.GetNextHop().first == nextHopAddr) {
                    routeEntry.UpdatePheromone(antDestinationAddr, nextHopAddr, *trafficEntry);
                    NS_LOG_LOGIC("Updated routing table pheromones for destination " << antDestinationAddr);
                    break;
                }
            }

            // Check if a subpath route is good enough
            // For good subpath, also update pheromones for the corresponding destination
            // if (backwardStack.size() > 2) {}
            //     for (size_t idx = backwardStack.size() - 2; idx > 0: --idx) {

            //     }
            // }


            // Check if reached source
            if (antHeader.GetForwardStack().empty()) {
                NS_LOG_LOGIC("Backward ant reached source " << header.GetDestination());
                return true;
            }
            // Relay backward ant to next hop
            AntHeader::AntHeaderStackEntry nextHop = antHeader.PopForwardStackEntryToBackwardStack();
            Ipv4Address nextHopAddr = nextHop.GetAddress();
            Ptr<Ipv4Route> route = LookupRoute(nextHopAddr);
            if (route) {
                // Backward ants get high priority than forward ants and normal packets
                SocketPriorityTag priorityTag;
                priorityTag.SetPriority(7);
                copy->AddPacketTag(priorityTag);

                copy->AddHeader(antHeader);
                ucb(route, copy, header);
                return true;
            } else {
                NS_LOG_ERROR("No route found for backward ant to " << nextHopAddr);
                return false;
            }
        } else {
            NS_LOG_ERROR("Unknown Ant type");
            return false;
        }
    }

    // Handle normal packet
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
    double sum_weights = 0.0;
    for (auto i = m_localTrafficStatsTable.begin(); i != m_localTrafficStatsTable.end(); ++i) {
        Ipv4AntNetLocalTrafficStatisticsEntry entry = *i;
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

void Ipv4AntNetRouting::SendForwardAnt(Ipv4Address dest) const {
    Ptr<Packet> p = Create<Packet>();

    Ptr<Ipv4Route> route = LookupRoute(dest);
    if (!route) {
        NS_LOG_ERROR("No route to send forward ant to " << dest);
        return;
    }

    AntHeader antHeader;
    Ipv4Address source = route->GetSource();
    antHeader.AddForwardHop(source, Simulator::Now()); // Initial hop with current node and current time
    antHeader.SetAntType(AntHeader::Type::FORWARD_ANT);
    p->AddHeader(antHeader);

    m_ipv4->Send(p, source, dest, PROTOCOL_ANTNET, route);
}

Ptr<Ipv4Route> Ipv4AntNetRouting::LookupRoute(Ipv4Address dest, Ptr<NetDevice> oif) const {
    NS_LOG_FUNCTION(this << dest);
    Ptr<Ipv4Route> rtentry = nullptr;
    uint16_t longest_mask = 0;
    
    // Ignore multicast and broadcast addresses for now
    if (dest.IsMulticast() || dest.IsBroadcast()) {
        return rtentry;
    }

    for (auto i = m_routingTable.begin(); i != m_routingTable.end(); ++i) {
        Ipv4AntNetRoutingTableEntry entry = *i;
        Ipv4Address entryDest = entry.GetDestAddr();
        Ipv4Mask entryMask = entry.GetDestMask();
        uint16_t masklen = entryMask.GetPrefixLength();
        NS_LOG_LOGIC("Checking route to " << dest << ", checking against route to " << entryDest 
                                          << "/" << masklen);
        if (entryMask.IsMatch(dest, entryDest)) {
            NS_LOG_LOGIC("Found global nestwork route to " << entryDest << "/" << masklen);
            Ipv4AntNetRoutingTableEntry::PheromoneKey nextHop = entry.GetNextHop(oif);
            Ipv4Address nextHopAddr = nextHop.first;
            uint32_t nextHopInterface = nextHop.second;
            if (oif) {
                if (oif != m_ipv4->GetNetDevice(nextHopInterface)) {
                    NS_LOG_LOGIC("Not on requested interface, skipping");
                    continue;
                }
            }
            if (masklen < longest_mask) { // Not interested if got shorter mask
                NS_LOG_LOGIC("Previous match longer, skipping");
                continue;
            }
            longest_mask = masklen;
            rtentry = Create<Ipv4Route>();
            rtentry->SetDestination(entry.GetDestAddr());
            rtentry->SetSource(m_ipv4->SourceAddressSelection(nextHopInterface, entry.GetDestAddr()));
            rtentry->SetGateway(nextHopAddr);
            rtentry->SetOutputDevice(m_ipv4->GetNetDevice(nextHopInterface));
            if (masklen == 32) { // Exact match
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



}

