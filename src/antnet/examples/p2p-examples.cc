#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"

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

    // ====== 读路由表 ======
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

            std::cout << rte.GetDest() << "  "
                      << rte.GetGateway() << "  "
                      << rte.GetDestNetwork() << "/"
                      << rte.GetDestNetworkMask() 
                      << "  OutIf=" << rte.GetInterface()
                      << std::endl;
        }
    }

    Simulator::Run();
    Simulator::Destroy();
}