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



}

Ipv4AntNetRouting::~Ipv4AntNetRouting()
{
    NS_LOG_FUNCTION(this);
    // Destructor implementation

    

}

Ptr<Ipv4Route>
Ipv4AntNetRouting::RouteOutput(Ptr<Packet> p,
                               const Ipv4Header& header,
                               Ptr<NetDevice> oif,
                               Socket::SocketErrno& sockerr)
{
    NS_LOG_FUNCTION(this << p << header << oif << sockerr);
    Ipv4Address destination = header.GetDestination();
    Ptr<Ipv4Route> rtentry = nullptr;

    rtentry = LookupRoute(destination); // probably need oif here
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
            if (m_ipv4->IsDestinationAddress(header.GetDestination(), iif)) {
                // Process forward ant at destination
                // Flip to backward ant and send back


            } else {
                // Relay forward ant, similar with normal packet forwarding
                Ptr<Ipv4Route> route = LookupRoute(header.GetDestination());
                if (route) {
                    antHeader.AddHop(m_ipv4->GetAddress(iif, 0).GetLocal(), Simulator::Now()); // add current node and time, The second parameter is placeholder for delay
                    copy->AddHeader(antHeader); // Re-add the header
                    ucb(route, copy, header);
                    return true;
                } else {
                    NS_LOG_ERROR("No route found for forward ant");
                    return false;
                }
            }
        } else if (antType == AntHeader::Type::BACKWARD_ANT) {
            // Process backward ant


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
    // This will be called periodically to send forward ants (don't know how to yet)
    
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

void Ipv4AntNetRouting::SendForwardAnt(Ipv4Address dest) {
    Ptr<Packet> p = Create<Packet>();

    AntHeader antHeader;
    Ipv4Address source = this->m_ipv4->GetAddress(1, 0).GetLocal(); // Assuming interface 1 and address 0 for now, will change later
    
    antHeader.AddHop(source, Simulator::Now()); // Initial hop with current node and current time
    antHeader.SetAntType(AntHeader::Type::FORWARD_ANT);
    p->AddHeader(antHeader);

    m_ipv4->Send(p, source, dest, PROTOCOL_ANTNET, LookupRoute(dest));
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

