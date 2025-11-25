#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "../model/ipv4-antnet-routing-table-entry.h"

using namespace ns3;

int main()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(p2p.Install(nodes));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    for (uint32_t i = 0; i < nodes.GetN(); i++)
    {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol();
        Ptr<Ipv4GlobalRouting> gr = DynamicCast<Ipv4GlobalRouting>(rp);

        std::cout << "=== Node " << i << " routing table ===\n";

        uint32_t n = gr->GetNRoutes();
        for (uint32_t k = 0; k < n; k++)
        {
            Ipv4RoutingTableEntry rte = gr->GetRoute(k);
            Ipv4Address nextHop  = rte.GetGateway();
            uint32_t    iface    = rte.GetInterface();
            // 1. Make the key
            Ipv4AntNetRoutingTableEntry::PheromoneKey key(nextHop, iface);

            // 2. Pick an initial pheromone value
            double initialPheromone = 1.0;

            // 3. Make the pheromone list
            Ipv4AntNetRoutingTableEntry::PheromoneList pherList;
            pherList.push_back(std::make_pair(key, initialPheromone));
            Ipv4AntNetRoutingTableEntry newRoutingTable = new Ipv4AntNetRoutingTableEntry(rte.GetDest(), rte.GetDestNetworkMask(), pherList);
        }
    }

    Simulator::Run();
    Simulator::Destroy();
}