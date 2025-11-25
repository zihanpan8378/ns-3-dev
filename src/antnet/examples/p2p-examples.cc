#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-list-routing.h"
#include "../model/ipv4-antnet-routing-table-entry.h"

using namespace ns3;

int
main (int argc, char *argv[]) {
  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  Ipv4GlobalRoutingHelper globalRouting;
  InternetStackHelper stack;
  stack.SetRoutingHelper (globalRouting);
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ipv4.Assign (devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  for (uint32_t i = 0; i < nodes.GetN (); i++) {
    Ptr<Node> node = nodes.Get (i);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol ();

    Ptr<Ipv4GlobalRouting> gr;
    Ptr<Ipv4ListRouting> list = DynamicCast<Ipv4ListRouting> (rp);
    if (list) {
      int16_t priority;
      for (uint32_t j = 0; j < list->GetNRoutingProtocols (); ++j) {
        Ptr<Ipv4RoutingProtocol> subRp = list->GetRoutingProtocol (j, priority);
        Ptr<Ipv4GlobalRouting> tmp = DynamicCast<Ipv4GlobalRouting> (subRp);
        if (tmp) {
            gr = tmp;
            break;
        }
      }
    } else {
      gr = DynamicCast<Ipv4GlobalRouting> (rp);
    }
    std::cout << "=== Node " << i << " routing table ===\n";
    if (!gr) {
      std::cout << "  (no Ipv4GlobalRouting on this node)\n";
      continue;
    }

    uint32_t n = gr->GetNRoutes ();
    for (uint32_t k = 0; k < n; k++) {
      Ipv4RoutingTableEntry rte = gr->GetRoute (k);

      Ipv4Address nextHop = rte.GetGateway ();
      uint32_t iface = rte.GetInterface ();

      Ipv4AntNetRoutingTableEntry::PheromoneKey key (nextHop, iface);
      double initialPheromone = 1.0;

      Ipv4AntNetRoutingTableEntry::PheromoneList pherList;
      pherList.push_back (std::make_pair (key, initialPheromone));

      Ipv4AntNetRoutingTableEntry newRoutingTable (
        rte.GetDest (),
        rte.GetDestNetworkMask (),
        pherList);
      }
  }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}