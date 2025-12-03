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
#include "ns3/timestamp-tag.h"
#include "ns3/ipv4-route.h"
#include "ns3/nstime.h"
#include "ns3/queue.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/queue-disc.h"

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
            .AddConstructor<Ipv4AntNetRouting>()
            .AddAttribute("ForwardAntInterval",
                          "Interval between sending forward ants from this node.",
                          TimeValue(Seconds(5)),
                          MakeTimeAccessor(&Ipv4AntNetRouting::m_forwardAntInterval),
                          MakeTimeChecker())
            .AddAttribute("BeaconInterval",
                          "Interval between sending beacon ants from this node.",
                          TimeValue(Seconds(1)),
                          MakeTimeAccessor(&Ipv4AntNetRouting::m_beaconInterval),
                          MakeTimeChecker())
            .AddAttribute("UseBeaconWindow",
                          "Whether to use beacon window for local traffic statistics.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&Ipv4AntNetRouting::m_useBeaconWindow),
                          MakeBooleanChecker())
            .AddAttribute("UseFailureMessagePropagation",
                          "Whether to use failure message propagation mechanism.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&Ipv4AntNetRouting::m_useFailureMessagePropagation),
                          MakeBooleanChecker());
    return tid;
}

Ipv4AntNetRouting::Ipv4AntNetRouting()
    : m_ipv4(nullptr),
      m_roundNumber(0),
      m_beaconSentCount(0)
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

    if (m_useBeaconWindow) {
        *stream->GetStream() << "    Received Beacons Count:" << std::endl;
        *stream->GetStream() << "        --------------------------------------------------------" << std::endl;
        *stream->GetStream() << "        Beacon sent: " << m_beaconSentCount << std::endl;
        for (const auto& entry : m_receivedBeaconsCountMap) {
            *stream->GetStream() << "        " << entry.first << " : " << entry.second << "" << std::endl;
        }
        *stream->GetStream() << "        --------------------------------------------------------" << std::endl;
    }
    
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

    // Return the route if found
    // If an output interface is specified, ensure the route uses it
    if (rtentry) {
        if (oif && rtentry->GetOutputDevice() != oif) {
            sockerr = Socket::ERROR_NOROUTETOHOST;
            NS_LOG_ERROR("RouteOutput: Output device from route does not match specified oif");
            return 0;
        }
        sockerr = Socket::ERROR_NOTERROR;
        return rtentry;
    } else {
        sockerr = Socket::ERROR_NOROUTETOHOST;
        return 0;
    }
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

        uint32_t nodeId = m_ipv4->GetObject<Node>()->GetId();
        NS_LOG_INFO("Node "<< nodeId << " Receive ant. Ant: " << antHeader.ToString());
        
        // Process based on ant type
        AntHeader::Type antType = antHeader.GetAntType();
        if (antType == AntHeader::Type::FORWARD_ANT) {
            // Process forward ant
            NS_LOG_LOGIC("Processing forward ant to " << header.GetDestination());

            // Get in-address for current hop and current time for stack in ant header. 
            // Out-address will be set when sending ant out.
            Ipv4Address inAddr = m_ipv4->GetAddress(iif, 0).GetLocal();
            Time now = Simulator::Now();

            // Detect and handle cycles in forward ant's path
            std::pair<bool, Ipv4Address> cycleResult = antHeader.DetectAndPopForwardStackCycle(nodeId, now);
            bool cycleFound = cycleResult.first;
            if (cycleFound) {
                NS_LOG_INFO("    Detected cycle in forward ant's path. Popped cycle entries.");

                // Check if cycle is long enough to drop the ant (DetectAndPopForwardStackCycle will return second as 0.0.0.0 if so)
                if (cycleResult.second == Ipv4Address("0.0.0.0")) {
                    NS_LOG_INFO("    Cycle is long enough to drop the ant.");
                    return true;
                }

                // If cycle is short, update inAddr to the address at which cycle started, so backward ant can be sent correctly
                inAddr = cycleResult.second;

                // Didn't handle the time adjustment for cycle entries here for simplicity
            }

            // Check if the current node is the destination
            if (m_ipv4->IsDestinationAddress(header.GetDestination(), iif)) {
                // Process forward ant at destination
                // Flip to backward ant and send back
                NS_LOG_LOGIC("Forward ant reached destination " << header.GetDestination() << ", converting to backward ant");
                
                // Add final hop to forward stack
                // For the final hop, inAddr and outAddr are the same since ant reached destination and reversal will start from here
                antHeader.AddForwardHop(nodeId, inAddr, inAddr, now);

                // Convert forward ant to backward ant
                antHeader.SetAntType(AntHeader::Type::BACKWARD_ANT);
                antHeader.PopForwardStackEntryToBackwardStack(); // Move destination from forward to backward stack

                // Send backward ant to next hop
                AntHeader::AntHeaderStackEntry nextHop = antHeader.PopForwardStackEntryToBackwardStack(); // Get next hop and move it to backward stack
                Ipv4Address nextHopAddr = nextHop.GetAddressOut();
                // Since backward ants are sent throught the original path
                // we lookup route to next hop (previous hop when forward) rather than the destination (source node when forward)
                // The next hop is chosen deterministically from the stack since it follows the reversed original path
                Ptr<Ipv4Route> route = LookupRoute(nextHopAddr, nullptr, true);
                if (route) {
                    // Make a new packet for backward ant
                    Ptr<Packet> back_packet = Create<Packet>();

                    // Backward ants get high priority than forward ants and normal packets
                    SocketPriorityTag priorityTag;
                    priorityTag.SetPriority(7);
                    back_packet->AddPacketTag(priorityTag);

                    // Add ant header back to packet
                    back_packet->AddHeader(antHeader);
                    NS_LOG_INFO("Node "<< nodeId << " Send backward ant to next hop " << nextHopAddr << " from interface " << route->GetOutputDevice()->GetIfIndex() << ". Ant: " << antHeader.ToString());
                    
                    // Send the backward ant packet
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
                    // Add current hop to forward stack
                    antHeader.AddForwardHop(nodeId, inAddr, route->GetSource(), now);

                    // Add ant header back to packet
                    copy->AddHeader(antHeader);

                    // Update payload size and ttl in ipv4 header
                    Ipv4Header newHeader = header;
                    newHeader.SetTtl (header.GetTtl() - 1);
                    newHeader.SetPayloadSize (copy->GetSize());

                    NS_LOG_INFO("Node "<< m_ipv4->GetObject<Node>()->GetId() << " Relay forward ant to next hop " << route->GetGateway() << " from interface " << route->GetOutputDevice()->GetIfIndex() << ". Ant: " << antHeader.ToString());
                    // Call unicast forward callback to send the packet
                    ucb(route, copy, newHeader);
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
            Ipv4Address antDestinationAddr = antDestinationEntry.GetAddressIn();
            Time antDestinationTime = antDestinationEntry.GetTime();
            Time delay = antDestinationTime - backwardStack.back().GetTime();
            
            // Update statistics for destination
            Ipv4AntNetLocalTrafficStatisticsEntry* trafficEntry = FindLocalTrafficStatisticsEntry(antDestinationAddr);
            if (trafficEntry) {
                // Call the traffic stat entry's update method
                // trafficEntry->AddDataFlowMeasure();
                trafficEntry->UpdateStatistics(delay.GetMilliSeconds());
                NS_LOG_LOGIC("Updated local traffic statistics for destination " << antDestinationAddr);
            } else {
                NS_LOG_ERROR("No local traffic statistics entry found for destination " << antDestinationAddr);
                return false;
            }
            
            // Update routing table pheromones for destination
            Ipv4Address nextForwardHopAddr = backwardStack[backwardStack.size() - 2].GetAddressIn();
            Ipv4AntNetRoutingTableEntry* routeEntry = FindRoutingTableEntry(antDestinationAddr);
            if (routeEntry) {
                // Call the routing table entry's pheromone update method
                routeEntry->UpdatePheromone(nextForwardHopAddr, delay.GetMilliSeconds(), *trafficEntry);
                NS_LOG_LOGIC("Updated routing table pheromones for destination " << antDestinationAddr);
            } else {
                NS_LOG_ERROR("No routing table entry found for destination " << antDestinationAddr);
                return false;
            }
            
            // Check if a subpath route is good enough
            // For good subpath, also update pheromones for the corresponding destination
            if (backwardStack.size() > 2) {
                for (size_t idx = backwardStack.size() - 2; idx > 0; --idx) {
                    Ipv4Address subpathDest = backwardStack[idx].GetAddressIn();
                    Time subpathDelay = backwardStack[idx].GetTime() - backwardStack.back().GetTime();
                    double delayMs = (subpathDelay).GetMilliSeconds();
                    Ipv4AntNetLocalTrafficStatisticsEntry* subPathTrafficEntry = FindLocalTrafficStatisticsEntry(subpathDest);
                    if (subPathTrafficEntry) {
                        if (delayMs <= subPathTrafficEntry->GetUpperBoundDelayFromWindow()) {
                            NS_LOG_LOGIC("Subpath to " << subpathDest << " is good enough with delay " << delayMs << " ms, updating pheromone");
                            Ipv4Address subpathNextHopAddr = backwardStack[idx - 1].GetAddressIn();
                            Ipv4AntNetRoutingTableEntry* subpathRouteEntry = FindRoutingTableEntry(subpathDest);
                            if (subpathRouteEntry) {
                                subpathRouteEntry->UpdatePheromone(subpathNextHopAddr, delayMs, *subPathTrafficEntry);
                                NS_LOG_LOGIC("Updated routing table pheromones for subpath destination " << subpathDest);
                            } else {
                                NS_LOG_ERROR("No routing table entry found for subpath destination " << subpathDest);
                                return false;
                            }
                        }
                    } else {
                        NS_LOG_ERROR("No local traffic statistics entry found for subpath destination " << subpathDest);
                        return false;
                    }
                }
            }

            // Check if reached source by looking if forward stack is empty
            if (antHeader.GetForwardStack().empty()) {
                NS_LOG_LOGIC("Backward ant reached source " << header.GetDestination());

                NS_LOG_INFO("Node "<< m_ipv4->GetObject<Node>()->GetId() << " Backward ant reached source. Ant: " << antHeader.ToString());


                Ptr<OutputStreamWrapper> stream = new OutputStreamWrapper(&std::cout);
                PrintRoutingTable(stream, Time::MS);


                return true;
            }

            // Relay backward ant to next hop
            AntHeader::AntHeaderStackEntry nextHop = antHeader.PopForwardStackEntryToBackwardStack();
            Ipv4Address nextBackwardHopAddr = nextHop.GetAddressOut();
            // Since backward ants are sent throught the original path
            // we lookup route to next hop (previous hop when forward) rather than the destination (source node when forward)
            Ptr<Ipv4Route> route = LookupRoute(nextBackwardHopAddr, nullptr, true);
            if (route) {
                // Create a new packet for next hop of backward ant
                Ptr<Packet> newPacket = Create<Packet>();
                
                // Backward ants get high priority than forward ants and normal packets
                SocketPriorityTag priorityTag;
                priorityTag.SetPriority(7);
                newPacket->AddPacketTag(priorityTag);

                // Add ant header back to packet
                newPacket->AddHeader(antHeader);

                NS_LOG_INFO("Node "<< m_ipv4->GetObject<Node>()->GetId() << " Relay backward ant to next hop " << route->GetGateway() << " from interface " << route->GetOutputDevice()->GetIfIndex() << ". Ant: " << antHeader.ToString());
                // Call unicast forward callback to send the packet
                m_ipv4->Send(newPacket, route->GetSource(), route->GetDestination(), PROTOCOL_ANTNET, route);
                
                return true;
            } else {
                NS_LOG_ERROR("No route found for backward ant to " << nextBackwardHopAddr);
                return false;
            }
        } else if (antType == AntHeader::Type::BEACON_ANT) {
            NS_LOG_INFO("Node "<< m_ipv4->GetObject<Node>()->GetId() << " Process beacon ant. Ant: " << antHeader.ToString());

            // Update received beacon count for neighbour
            m_receivedBeaconsCountMap[header.GetSource()] += 1;

            for (const auto entry : m_receivedBeaconsCountMap) {
                NS_LOG_INFO("    Received beacon count from neighbour " << entry.first << " : " << entry.second);
            }
            return true;
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
            // Call local delivery callback to deliver the packet
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
        // Ensure we are not sending back out the input interface
        uint32_t oif = route->GetOutputDevice()->GetIfIndex();
        if (oif == iif) {
            return false;
        }

        // Call unicast forward callback to send the packet
        NS_LOG_LOGIC("Found unicast destination- calling unicast callback");
        ucb(route, p, header);
        return true;
    } else {
        NS_LOG_LOGIC("Did not find unicast destination, returning false");
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

    // Reschedule the next forward ant event
    m_forwardAntEvent = Simulator::Schedule(
        m_forwardAntInterval,
        &Ipv4AntNetRouting::ScheduleForwardAnt,
        this
    );
}

void Ipv4AntNetRouting::InitializeRoutingTable(
    const std::list<std::pair<Ipv4Address, Ipv4Address>>& destList, 
    const std::list<std::pair<Ipv4Address, Ipv4Address>>& neighbourList) {
    NS_LOG_FUNCTION(this);

    // Clear existing routing table and local traffic statistics table
    m_routingTable.clear();
    m_localTrafficStatsTable.clear();

    // Get all addresses of the current node
    std::vector<Ipv4Address> nodeAddresses;
    for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
        for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); ++j) {
            Ipv4Address addr = m_ipv4->GetAddress(i, j).GetLocal();
            if (addr != Ipv4Address() && addr != Ipv4Address("127.0.0.1")) {
                nodeAddresses.push_back(addr);
            }
        }
    }

    // Initial pheromone value, equally distributed among neighbours
    double initialPheromone = 1.0 / neighbourList.size();

    // Prepare pheromone list from neighbours
    Ipv4AntNetRoutingTableEntry::PheromoneList pheromoneList;
    for (const auto& neighbourAddrAndMask : neighbourList) {
        Ipv4Address neighbourAddr = neighbourAddrAndMask.first;
        Ipv4Mask neighbourMask = Ipv4Mask(neighbourAddrAndMask.second.Get());

        // Get the interface that can reach this neighbour
        // Iterate through interfaces to find the matching one
        uint32_t interfaceToNeighbour = std::numeric_limits<uint32_t>::max();
        for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); ++i) {
            Ptr<NetDevice> device = m_ipv4->GetNetDevice(i);
            if (!device) {
                continue;
            }
            bool foundInterface = false;
            // Check if this interface's address matches the neighbour's subnet
            for (uint32_t j = 0; j < m_ipv4->GetNAddresses(i); ++j) {
                Ipv4Address ifaceAddr = m_ipv4->GetAddress(i, j).GetLocal();
                if (ifaceAddr == Ipv4Address::GetLoopback() || ifaceAddr == Ipv4Address("0.0.0.0")){
                    continue;
                }
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
        NS_ASSERT_MSG(interfaceToNeighbour != std::numeric_limits<uint32_t>::max(), "Could not find interface to neighbour " << neighbourAddr);

        // Map neighbour address to interface
        m_neighbourInterfaceMap[neighbourAddr] = interfaceToNeighbour;

        // Add this neighbour to the pheromone list with initial pheromone value
        Ipv4AntNetRoutingTableEntry::PheromoneKey pheromoneKey = std::make_pair(neighbourAddr, interfaceToNeighbour);
        pheromoneList.push_back(std::make_pair(pheromoneKey, initialPheromone));
    }

    // Initialize routing table entries
    for (const auto& destAddrAndMask : destList) {
        // Check if destination address is one of the node's own addresses, skip if so
        if (std::find(nodeAddresses.begin(), nodeAddresses.end(), destAddrAndMask.first) != nodeAddresses.end()) {
            continue;
        }

        Ipv4Address destAddr = destAddrAndMask.first;
        Ipv4Mask destMask = Ipv4Mask(destAddrAndMask.second.Get());

        // Prepare local traffic statistics entry for this destination
        Ipv4AntNetLocalTrafficStatisticsEntry trafficEntry(destAddr, destMask);
        m_localTrafficStatsTable.push_back(trafficEntry);

        // Make a copy of pheromoneList for this destination
        Ipv4AntNetRoutingTableEntry::PheromoneList pheromoneListCopy = pheromoneList;

        // Create routing table entry and add to routing table
        Ipv4AntNetRoutingTableEntry newEntry(destAddr, destMask, pheromoneListCopy);
        m_routingTable.push_back(newEntry);
    }

    // Schedule the first forward ant event
    m_forwardAntEvent = Simulator::Schedule(
        m_forwardAntInterval,
        &Ipv4AntNetRouting::ScheduleForwardAnt,
        this
    );

    // Initialize beacon mechanism if enabled
    if (m_useBeaconWindow) {
        // Initialize received beacons count map
        for (const auto& neighbour : m_neighbourInterfaceMap) {
            Ipv4Address neighbourAddr = neighbour.first;
            m_receivedBeaconsCountMap[neighbourAddr] = 0;
        }

        // Initialize beacon window for neighbour failure detection
        m_beaconEvent = Simulator::Schedule(
            m_beaconInterval,
            &Ipv4AntNetRouting::SendBeacon,
            this
        );
    }

    // Initialize failure message propagation mechanism if enabled (not implemented yet)
    if (m_useFailureMessagePropagation) {

    }
}

void Ipv4AntNetRouting::SendForwardAnt(Ipv4Address dest) {
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
    AntHeader antHeader(
        AntHeader::Type::FORWARD_ANT,                                               // Ant type
        m_ipv4->GetObject<Node>()->GetId(),                                         // Source node ID
        m_ipv4->GetAddress(route->GetOutputDevice()->GetIfIndex(), 0).GetLocal(),   // Source address
        dest,                                                                       // Destination address
        m_roundNumber                                                               // Round number
    );
    Ipv4Address source = route->GetSource();
    uint32_t nodeId = m_ipv4->GetObject<Node>()->GetId();
    // Initial hop with current node and current time. No in-address for first hop
    antHeader.AddForwardHop(nodeId, Ipv4Address("0.0.0.0"), source, Simulator::Now()); 
    p->AddHeader(antHeader);

    NS_LOG_INFO("Node "<< nodeId << " Send forward ant: " << antHeader.ToString());
    // Call Ipv4 L3 protocol's Send method to send the packet
    m_ipv4->Send(p, source, dest, PROTOCOL_ANTNET, route);
    m_roundNumber++;
}

void Ipv4AntNetRouting::SendBeacon() {
    NS_LOG_FUNCTION(this);

    if (m_routingTable.empty()) {
        NS_LOG_LOGIC("Routing table is empty, don't need to send beacon");
        return;
    }

    // Iterate through all neighbours to send beacon
    for (const auto& neighbour : m_neighbourInterfaceMap) {
        Ipv4Address neighbourAddr = neighbour.first;

        // Create a beacon packet
        Ptr<Packet> beaconPacket = Create<Packet>();

        // Look up route to neighbour
        Ptr<Ipv4Route> route = LookupRoute(neighbourAddr, nullptr, true);

        // Create and init AntHeader for beacon
        AntHeader antHeader(
            AntHeader::Type::BEACON_ANT,                                                // Ant type
            m_ipv4->GetObject<Node>()->GetId(),                                         // Source node ID
            m_ipv4->GetAddress(route->GetOutputDevice()->GetIfIndex(), 0).GetLocal(),   // Source address
            neighbourAddr,                                                              // Destination address is neighbour
            0                                                                           // Round number not used for beacon
        );
        beaconPacket->AddHeader(antHeader);

        // Send the beacon packet
        NS_LOG_INFO("Node "<< m_ipv4->GetObject<Node>()->GetId() << " Send beacon to neighbour " << neighbourAddr << ". Ant: " << antHeader.ToString());
        m_ipv4->Send(beaconPacket, route->GetSource(), neighbourAddr, PROTOCOL_ANTNET, route);
    }

    // Increment beacon sent count
    m_beaconSentCount++;

    // Reschedule the next beacon sending event
    m_beaconEvent = Simulator::Schedule(
        m_beaconInterval,
        &Ipv4AntNetRouting::SendBeacon,
        this
    );
}

Ptr<Ipv4Route> Ipv4AntNetRouting::LookupRoute(Ipv4Address dest, Ptr<NetDevice> oif, bool backwardAntLookup) const {
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
            Ipv4AntNetRoutingTableEntry::PheromoneKey nextHop; // A pair of next hop address and interface index
            if (backwardAntLookup) {
                // For backward ant lookup, use deterministic next hop selection
                nextHop = entry.GetDeterministicNextHop(dest);
            } else {
                // For normal lookup, prepare queue length list for probabilistic next hop selection
                Ipv4AntNetRoutingTableEntry::PheromoneList pheromoneList = entry.GetPheromoneList();
                // Get the queue length for each neighbor
                std::map<Ipv4Address, double> queueLengthMap; // A map of neighbor address to its queue length
                for (const auto& pheromonePair : pheromoneList) {
                    Ipv4Address neighborAddr = pheromonePair.first.first;
                    uint32_t neighborInterface = pheromonePair.first.second;

                    // Get queue length on the interface to this neighbor
                    Ptr<TrafficControlLayer> tc = m_ipv4->GetObject<Node>()->GetObject<TrafficControlLayer>();
                    if (tc) {
                        Ptr<QueueDisc> qd = tc->GetRootQueueDiscOnDevice(m_ipv4->GetNetDevice(neighborInterface));
                        if (qd) {
                            uint32_t queueBytes = qd->GetNBytes();
                            queueLengthMap[neighborAddr] = static_cast<double>(queueBytes);
                            continue;
                        }
                    } else {
                        NS_LOG_ERROR("TrafficControlLayer not found on node " << m_ipv4->GetObject<Node>()->GetId());
                    }
                }
                nextHop = entry.GetNextHop(queueLengthMap);
            }
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
