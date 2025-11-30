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

    std::set<std::pair<Ipv4Address, Ipv4Address>> allDestAddrs;

    for (auto i = NodeList::Begin(); i != NodeList::End(); ++i) 
    {
        Ptr<Node> node = *i;

        Ptr<GlobalRouter> rtr = node->GetObject<GlobalRouter>();
        if (!rtr) {
            continue;
        }

        uint32_t numLSAs = rtr->DiscoverLSAs();
        NS_LOG_LOGIC("Found " << numLSAs << " LSAs for node " << node->GetId());

        std::list<std::pair<Ipv4Address, Ipv4Address>> neighbourAddrs = {};

        for (uint32_t j = 0; j < numLSAs; ++j) {
            GlobalRoutingLSA* lsa = new GlobalRoutingLSA();
            rtr->GetLSA(j, *lsa);

            // For Router LSAs that are not originated by this router, skip them
            if (lsa->GetLSType () != GlobalRoutingLSA::RouterLSA || lsa->GetAdvertisingRouter () != rtr->GetRouterId ()) {
                continue;
            }

            uint32_t nLinks = lsa->GetNLinkRecords ();
            for (uint32_t k = 0; k < nLinks; ++k) {
                GlobalRoutingLinkRecord *lr = lsa->GetLinkRecord(k);
                if (!lr) {
                    continue;
                }

                if (lr->GetLinkType () == GlobalRoutingLinkRecord::StubNetwork) {
                    allDestAddrs.insert(std::make_pair(lr->GetLinkId(), lr->GetLinkData()));
                }

                if (lr->GetLinkType () == GlobalRoutingLinkRecord::PointToPoint) {
                    if (k + 1 < nLinks) {
                        GlobalRoutingLinkRecord *stub = lsa->GetLinkRecord (k + 1);
                        if (stub && stub->GetLinkType() == GlobalRoutingLinkRecord::StubNetwork) {
                            neighbourAddrs.push_back(std::make_pair(stub->GetLinkId(), stub->GetLinkData()));
                        }
                    }
                }
            }
        }

        m_neighbourAdjList[node] = neighbourAddrs;
    }
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

        antRouting->InitializeRoutingTable(m_nodeList, m_neighbourAdjList[node]);
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
