#include "ns3/ipv4-antnet-routing-helper.h"
#include "ns3/ipv4-antnet-routing.h"
#include "ns3/ipv4-antnet-routing-table-entry.h"

#include "ns3/global-router-interface.h"
#include "ns3/log.h"
#include "ns3/node-list.h"


namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Ipv4AntNetRoutingHelper");

std::map<ns3::Ptr<ns3::Node>, std::list<std::pair<Ipv4Address, Ipv4Address>>> Ipv4AntNetRoutingHelper::m_neighbourAdjList;
std::list<std::pair<Ipv4Address, Ipv4Address>> Ipv4AntNetRoutingHelper::m_nodeList;

Ipv4AntNetRoutingHelper::Ipv4AntNetRoutingHelper()
{
}

Ipv4AntNetRoutingHelper::Ipv4AntNetRoutingHelper(const Ipv4AntNetRoutingHelper& o) 
{
}

Ipv4AntNetRoutingHelper* 
Ipv4AntNetRoutingHelper::Copy() const
{
    return new Ipv4AntNetRoutingHelper(*this);
}

Ptr<Ipv4RoutingProtocol> Ipv4AntNetRoutingHelper::Create(Ptr<Node> node) const
{
    NS_LOG_LOGIC("Adding GlobalRouter interface to node " << node->GetId());

    Ptr<GlobalRouter> globalRouter = CreateObject<GlobalRouter>();
    node->AggregateObject(globalRouter);

    NS_LOG_LOGIC("Creating Ipv4AntNetRouting for node " << node->GetId());
    Ptr<Ipv4AntNetRouting> routing = CreateObject<Ipv4AntNetRouting>();
    return routing;
}

void
Ipv4AntNetRoutingHelper::BuildAntNetTopology() 
{
    // Clear previous topology data
    m_neighbourAdjList.clear();
    m_nodeList.clear();

    // Set to store all destination addresses
    std::set<std::pair<Ipv4Address, Ipv4Address>> allDestAddrs;

    // Iterate over all nodes to build destination list and neighbour list
    for (auto i = NodeList::Begin(); i != NodeList::End(); ++i) 
    {
        // Current node
        Ptr<Node> node = *i;

        // List to store neighbour addresses
        // (first: IP address, second: subnet mask)
        std::list<std::pair<Ipv4Address, Ipv4Address>> neighbourAddrs = {};

        // List to store subnet DR addresses if the node is in a transit network
        // (first: DR address, second: node address)
        std::list<std::pair<Ipv4Address, Ipv4Address>> subnetDRAddrs = {};

        // Cast to GlobalRouter
        Ptr<GlobalRouter> rtr = node->GetObject<GlobalRouter>();
        if (!rtr) {
            continue;
        }

        // Discover LSAs for this router
        uint32_t numLSAs = rtr->DiscoverLSAs();
        NS_LOG_LOGIC("Found " << numLSAs << " LSAs for node " << node->GetId());

        // Iterate over LSAs to extract link records
        for (uint32_t j = 0; j < numLSAs; ++j) {
            // Get LSA
            GlobalRoutingLSA* lsa = new GlobalRoutingLSA();
            rtr->GetLSA(j, *lsa);

            lsa->Print(std::cout);

            // For Router LSAs that are not originated by this router and other types of LSAs, skip them
            if (lsa->GetLSType () != GlobalRoutingLSA::RouterLSA || lsa->GetAdvertisingRouter () != rtr->GetRouterId ()) {
                continue;
            }

            // Iterate over link records in this LSA
            uint32_t nLinks = lsa->GetNLinkRecords ();
            for (uint32_t k = 0; k < nLinks; ++k) {
                // Get link record
                GlobalRoutingLinkRecord *lr = lsa->GetLinkRecord(k);
                if (!lr) {
                    continue;
                }

                // If link is a Stub Network, add to allDestAddrs
                if (lr->GetLinkType () == GlobalRoutingLinkRecord::StubNetwork) {
                    allDestAddrs.insert(std::make_pair(lr->GetLinkId(), lr->GetLinkData()));
                }

                // If link is PointToPoint, add neighbour address
                if (lr->GetLinkType () == GlobalRoutingLinkRecord::PointToPoint) {
                    if (k + 1 < nLinks) {
                        GlobalRoutingLinkRecord *stub = lsa->GetLinkRecord (k + 1);
                        if (stub && stub->GetLinkType() == GlobalRoutingLinkRecord::StubNetwork) {
                            neighbourAddrs.push_back(std::make_pair(stub->GetLinkId(), stub->GetLinkData()));
                        }
                    }
                }

                // If link is Transit Network, add to allDestAddrs and store DR address for later neighbour discovery
                if (lr->GetLinkType () == GlobalRoutingLinkRecord::TransitNetwork) {
                    allDestAddrs.insert(std::make_pair(lr->GetLinkData(), Ipv4Address(Ipv4Mask::GetOnes().Get())));
                    subnetDRAddrs.push_back(std::make_pair(lr->GetLinkId(), lr->GetLinkData()));
                }
            }
        }

        // For transit networks, find other routers connected to the same network via the DR
        if (subnetDRAddrs.size() > 0) {
            // Iterate over all nodes to find DRs
            for (auto j = NodeList::Begin(); j != NodeList::End(); ++j) {
                Ptr<Node> node2 = *j;
                Ptr<GlobalRouter> rtr2 = node2->GetObject<GlobalRouter>();
                uint32_t numLSAs = rtr2->DiscoverLSAs();
                for (uint32_t k = 0; k < numLSAs; ++k) {
                    GlobalRoutingLSA* lsa = new GlobalRoutingLSA();
                    rtr2->GetLSA(k, *lsa);

                    // Consider only Network LSAs
                    if (lsa->GetLSType () != GlobalRoutingLSA::NetworkLSA) {
                        continue;
                    }

                    // Check if this LSA corresponds to any of the subnets this node is connected to
                    Ipv4Address netLsaId = lsa->GetLinkStateId();
                    for (const auto &addrPair : subnetDRAddrs) {
                        // Check if LSA Link State ID matches the DR address
                        if (netLsaId == addrPair.first) {
                            // Iterate over attached routers for this transit network to find neighbours
                            uint32_t nAttached = lsa->GetNAttachedRouters();
                            for (uint32_t l = 0; l < nAttached; ++l) {
                                Ipv4Address attachedRtrAddr = lsa->GetAttachedRouter(l);
                                // Skip the address of the current node
                                if (attachedRtrAddr == addrPair.second) {
                                    continue;
                                }
                                neighbourAddrs.push_back(std::make_pair(attachedRtrAddr, Ipv4Address(lsa->GetNetworkLSANetworkMask().Get())));
                            }
                        }
                    }
                }
            }
        }
        // Store neighbour list for this node
        m_neighbourAdjList[node] = neighbourAddrs;
    }

    // Convert set of all destination addresses to list
    for (const auto &addr : allDestAddrs) {
        m_nodeList.push_back(addr);
    }

    NS_LOG_LOGIC("Topology adjacency list:");
    for (const auto &node : m_neighbourAdjList) {
        NS_LOG_LOGIC("    Node " << node.first->GetId() << " neighbours: ");
        for (const auto &nbr : node.second) {
            NS_LOG_LOGIC("        " << nbr.first << "/" << Ipv4Mask(nbr.second.Get()).GetPrefixLength());
        }
    }

    NS_LOG_LOGIC("All destination addresses: ");
    for (const auto &addr : m_nodeList) {
        NS_LOG_LOGIC("    " << addr.first << "/" << Ipv4Mask(addr.second.Get()).GetPrefixLength());
    }
}

void 
Ipv4AntNetRoutingHelper::InitializeNodeRoutingTables() 
{
    int num_nodes = NodeList::GetNNodes();
    Ipv4AntNetRoutingTableEntry::Nk = num_nodes;
    for (auto i = NodeList::Begin(); i != NodeList::End(); ++i) {
        Ptr<Node> node = *i;

        NS_LOG_LOGIC("Initializing routing table for node " << node->GetId());

        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        if (!ipv4) {
            NS_LOG_WARN("Node " << node->GetId() << " has no Ipv4 object, skipping...");
            continue;
        }

        Ptr<Ipv4RoutingProtocol> routing = ipv4->GetRoutingProtocol();
        if (!routing) {
            NS_LOG_WARN("Node " << node->GetId() << " has no Ipv4RoutingProtocol object, skipping...");
            continue;
        }

        Ptr<Ipv4AntNetRouting> antRouting = routing->GetObject<Ipv4AntNetRouting>();
        if (!antRouting) {
            NS_LOG_WARN("Node " << node->GetId() << " has no Ipv4AntNetRouting object, skipping...");
            continue;
        }

        antRouting->InitializeRoutingTable(m_nodeList, m_neighbourAdjList[node], 2);
    }
}

void 
Ipv4AntNetRoutingHelper::InitializeNodeRoutingTablesForSpecificSourceAndDestination(int SourceID, Ipv4Address DestinationAddress)
{
    int num_nodes = NodeList::GetNNodes();
    Ipv4AntNetRoutingTableEntry::Nk = num_nodes;
    for (auto i = NodeList::Begin(); i != NodeList::End(); ++i) {
        Ptr<Node> node = *i;

        NS_LOG_LOGIC("Initializing routing table for node " << node->GetId());

        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        if (!ipv4) {
            NS_LOG_WARN("Node " << node->GetId() << " has no Ipv4 object, skipping...");
            continue;
        }

        Ptr<Ipv4RoutingProtocol> routing = ipv4->GetRoutingProtocol();
        if (!routing) {
            NS_LOG_WARN("Node " << node->GetId() << " has no Ipv4RoutingProtocol object, skipping...");
            continue;
        }

        Ptr<Ipv4AntNetRouting> antRouting = routing->GetObject<Ipv4AntNetRouting>();
        if (!antRouting) {
            NS_LOG_WARN("Node " << node->GetId() << " has no Ipv4AntNetRouting object, skipping...");
            continue;
        }

        if (node->GetId() == SourceID) {
            NS_LOG_LOGIC("    Initializing routing table for node ID " << node->GetId() << " to send ants to specific destination ID " << DestinationAddress);
            NS_LOG_LOGIC("    This node is sending ants.");
            // Find destination address
            // Ipv4Address destAddr;
            // for (const auto &addr : m_nodeList) {
            //     if (addr.first == DestinationAddress) {
            //         continue;
            //     }
            // }
            // for (const auto& destAddrAndMask : m_nodeList) {
            //     // nodeがdestinationIDにの時のみ、destAddrにアドレスをセットしてループを抜ける
            //     if (destAddrAndMask.first == Ipv4Address::GetLoopback()) {
            //         continue;
            //     }
            //         Ipv4Address destAddr = destAddrAndMask.first;
            // }
            // bool found = false;
            // for (NodeList::Iterator j = NodeList::Begin(); j != NodeList::End(); ++j) {
            //     Ptr<Node> dest_node = *j;
            //     if (dest_node->GetId() == DestinationID) {
            //         Ptr<Ipv4> destAddr = dest_node->GetObject<Ipv4>();
            //         // destAddr = rtr->GetRouterId();
            //     }
            // }
            // for (const auto &addrPair : m_nodeList) {
            //     Ptr<Node> node = *i;
            //     Ptr<GlobalRouter> rtr = node->GetObject<GlobalRouter>();
            //     if (rtr && rtr->GetRouterId() == DestinationID) {
            //         destAddr = addrPair.first;
            //         found = true;
            //         NS_LOG_LOGIC("    Found destination address " << destAddr << " for node ID " << DestinationID);
            //         break;
            //     }
            // }
            // if (!found) {
            //     NS_LOG_WARN("Destination node ID " << DestinationID << " not found in node list, skipping...");
            //     continue;
            // }
            antRouting->InitializeRoutingTable(m_nodeList, m_neighbourAdjList[node], 1, DestinationAddress);
        } else {
            NS_LOG_LOGIC("    Initializing normal routing table for node ID " << node->GetId());
            NS_LOG_LOGIC("    This node is not sending ants.");
            antRouting->InitializeRoutingTable(m_nodeList, m_neighbourAdjList[node], 0);
        }
    }
}

void 
Ipv4AntNetRoutingHelper::PrintRoutingTables() 
{
    for (auto i = NodeList::Begin(); i != NodeList::End(); ++i) {
        Ptr<Node> node = *i;

        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        if (!ipv4) {
            NS_LOG_WARN("Node " << node->GetId() << " has no Ipv4 object, skipping...");
            continue;
        }

        Ptr<Ipv4RoutingProtocol> routing = ipv4->GetRoutingProtocol();
        if (!routing) {
            NS_LOG_WARN("Node " << node->GetId() << " has no Ipv4RoutingProtocol object, skipping...");
            continue;
        }

        Ptr<Ipv4AntNetRouting> antRouting = routing->GetObject<Ipv4AntNetRouting>();
        if (!antRouting) {
            NS_LOG_WARN("Node " << node->GetId() << " has no Ipv4AntNetRouting object, skipping...");
            continue;
        }

        Ptr<OutputStreamWrapper> stream = new OutputStreamWrapper(&std::cout);
        antRouting->PrintRoutingTable(stream, Time::S);
    }
}

} // namespace ns3
