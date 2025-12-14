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
Ipv4AntNetRoutingHelper::BuildAntNetTopology(std::string prefix) 
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
        // NS_LOG_LOGIC("Found " << numLSAs << " LSAs for node " << node->GetId());
        NS_LOG_INFO("Found " << numLSAs << " LSAs for node " << node->GetId());


        // Iterate over LSAs to extract link records
        for (uint32_t j = 0; j < numLSAs; ++j) {
            // Get LSA
            GlobalRoutingLSA* lsa = new GlobalRoutingLSA();
            rtr->GetLSA(j, *lsa);

            // lsa->Print(std::cout);

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

    // for visualizing
    std::map<std::string, uint32_t> ipToNode; 

    NS_LOG_LOGIC("Topology adjacency list:");
    for (const auto &node : m_neighbourAdjList) {
        NS_LOG_LOGIC("    Node " << node.first->GetId() << " neighbours: ");
        for (const auto &nbr : node.second) {
            NS_LOG_LOGIC("        " << nbr.first << "/" << Ipv4Mask(nbr.second.Get()).GetPrefixLength());
        }
        
        // for visualizing
        uint32_t nodeId = node.first->GetId();
        for (const auto &nbr : node.second) {
            std::ostringstream oss;
            oss << nbr.first;  // Ipv4Address → string
            ipToNode[oss.str()] = nodeId;
        }
    }

    // for visualizing
    std::string fname_ip_to_node = prefix + "ip_to_node.json";
    std::string fname_node_to_ip = prefix + "node_to_ip.json";
    std::string fname_txt = prefix + "edges.txt";
    SaveIpToNodeJson(ipToNode, fname_ip_to_node);
    SaveNodeToIPJson(fname_node_to_ip);
    SaveEdges(fname_txt);

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
Ipv4AntNetRoutingHelper::InitializeNodeRoutingTablesForSpecificSourceAndDestination(uint32_t SourceID, Ipv4Address DestinationAddress)
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
            antRouting->InitializeRoutingTable(m_nodeList, m_neighbourAdjList[node], 1, DestinationAddress);
        } else {
            NS_LOG_LOGIC("    Initializing normal routing table for node ID " << node->GetId());
            NS_LOG_LOGIC("    This node is not sending ants.");
            antRouting->InitializeRoutingTable(m_nodeList, m_neighbourAdjList[node], 0);
        }
    }
}

void 
Ipv4AntNetRoutingHelper::InitializeNodeRoutingTablesForSpecificSourcesAndDestinations(
    const std::map<int, std::vector<Ipv4Address>>& sourceDestMap)
{
    int num_nodes = NodeList::GetNNodes();
    Ipv4AntNetRoutingTableEntry::Nk = num_nodes;

    // ソースノードセットを作る（検索高速化用）
    std::set<int> sourceSet;
    for (const auto& pair : sourceDestMap) {
        sourceSet.insert(pair.first);
    }

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

        if (sourceSet.find(node->GetId()) != sourceSet.end()) {
            NS_LOG_LOGIC("    Initializing routing table for node ID " << node->GetId() << " to send ants.");

            const std::vector<Ipv4Address>& destList = sourceDestMap.at(node->GetId());
            for (auto dest : destList) {
                NS_LOG_LOGIC("        Destination: " << dest);
                // ここでは1つずつ初期化する。必要に応じて antRouting 内で複数宛先をまとめて初期化可能
                antRouting->InitializeRoutingTable(m_nodeList, m_neighbourAdjList[node], 1, dest);
            }
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


void
Ipv4AntNetRoutingHelper::ScheduleCsvLogging(double interval, std::string prefix)
{
    NS_LOG_INFO("Scheduling CSV logging every " << interval << " seconds, prefix: " << prefix);

    // Remove all files matching the prefix pattern
    std::string rmCmd = "rm -f " + prefix + "*.csv";
    int result = system(rmCmd.c_str());
    (void)result; // Suppress unused variable warning

    Simulator::Schedule(
        Seconds(interval),
        &Ipv4AntNetRoutingHelper::DoCsvLogging,
        interval,
        prefix);
}

void
Ipv4AntNetRoutingHelper::DoCsvLogging(double interval, std::string prefix)
{
    for (auto it = NodeList::Begin(); it != NodeList::End(); ++it)
    {
        Ptr<Node> node = *it;

        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        if (!ipv4)
            continue;

        Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol();
        if (!rp)
            continue;

        Ptr<Ipv4AntNetRouting> ant = rp->GetObject<Ipv4AntNetRouting>();
        if (!ant)
            continue;

        // CSVファイル名
        std::ostringstream fname;
        fname << prefix << "node_" << node->GetId() << ".csv";

        // Extract directory path and create it if it doesn't exist
        std::string filePath = fname.str();
        size_t lastSlash = filePath.find_last_of("/");
        if (lastSlash != std::string::npos)
        {
            std::string dirPath = filePath.substr(0, lastSlash);
            std::string mkdirCmd = "mkdir -p " + dirPath;
            int result = system(mkdirCmd.c_str());
            (void)result; // Suppress unused variable warning
        }

        bool needHeader = !std::ifstream(fname.str()).good();

        Ptr<OutputStreamWrapper> stream =
            ns3::Create<OutputStreamWrapper>(fname.str(), std::ios::app);

        if (needHeader)
        {
            ant->PrintPheromonesCsvHeader(stream);
        }

        ant->PrintPheromonesCsv(stream);
    }

    // 再スケジュール
    Simulator::Schedule(
        Seconds(interval),
        &Ipv4AntNetRoutingHelper::DoCsvLogging,
        interval,
        prefix);
}


void
Ipv4AntNetRoutingHelper::SaveIpToNodeJson (
    const std::map<std::string, uint32_t> &ipToNode,
    const std::string &filePath)
{
    std::ofstream ofs (filePath, std::ofstream::out | std::ofstream::trunc);
    if (!ofs.is_open ())
    {
        NS_LOG_ERROR ("Failed to open file: " << filePath);
        return;
    }

    ofs << "{\n";

    for (auto it = ipToNode.begin (); it != ipToNode.end (); ++it)
    {
        ofs << "  \"" << it->first << "\": " << it->second;
        if (std::next (it) != ipToNode.end ())
        {
            ofs << ",";
        }
        ofs << "\n";
    }

    ofs << "}\n";
    ofs.close ();

    NS_LOG_INFO("ipToNode JSON saved to " << filePath);
}

void
Ipv4AntNetRoutingHelper::SaveNodeToIPJson (const std::string &filePath)
{
    std::map<uint32_t, std::vector<std::string>> nodeToIps;

    for (auto it = NodeList::Begin(); it != NodeList::End(); ++it)
    {
        Ptr<Node> node = *it;
        uint32_t nodeId = node->GetId();

        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        if (!ipv4) continue;

        std::vector<std::string> ips;

        for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i)
        {
            // interface 0 は loopback
            if (i == 0) continue;

            for (uint32_t a = 0; a < ipv4->GetNAddresses(i); ++a)
            {
                Ipv4InterfaceAddress addr = ipv4->GetAddress(i, a);
                Ipv4Address ip = addr.GetLocal();

                // 0.0.0.0 を除外
                if (ip == Ipv4Address("0.0.0.0"))
                    continue;

                std::ostringstream oss;
                oss << ip;   // ← ToString() の代わり
                ips.push_back(oss.str());
            }
        }

        nodeToIps[nodeId] = ips;
    }

    std::ofstream ofs(filePath);
    ofs << "{\n";

    bool firstNode = true;
    for (auto &p : nodeToIps)
    {
        if (!firstNode) ofs << ",\n";
        firstNode = false;

        ofs << "  \"" << p.first << "\": [";

        for (size_t i = 0; i < p.second.size(); ++i)
        {
            ofs << "\"" << p.second[i] << "\"";
            if (i + 1 < p.second.size()) ofs << ", ";
        }
        ofs << "]";
    }

    ofs << "\n}\n";
    ofs.close();
}

void
Ipv4AntNetRoutingHelper::SaveEdges (const std::string &filePath)
{
    std::set<std::pair<uint32_t, uint32_t>> edgeSet;

    for (auto it = NodeList::Begin(); it != NodeList::End(); ++it)
    {
        Ptr<Node> node = *it;
        uint32_t u = node->GetId();

        for (uint32_t d = 0; d < node->GetNDevices(); ++d)
        {
            Ptr<NetDevice> dev = node->GetDevice(d);
            Ptr<Channel> ch = dev->GetChannel();
            if (!ch) continue;

            for (uint32_t c = 0; c < ch->GetNDevices(); ++c)
            {
                Ptr<NetDevice> peerDev = ch->GetDevice(c);
                Ptr<Node> peerNode = peerDev->GetNode();
                uint32_t v = peerNode->GetId();

                if (u != v)
                {
                    edgeSet.insert({u, v}); // 有向 edge
                }
            }
        }
    }

    std::ofstream ofs(filePath);
    ofs << "edges = [\n";

    for (auto &e : edgeSet)
    {
        ofs << "        (" << e.first << "," << e.second << "),\n";
    }

    ofs << "]\n";
    ofs.close();
    NS_LOG_INFO("edges saved to " << filePath);
}

} // namespace ns3
