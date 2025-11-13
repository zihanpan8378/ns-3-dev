// src/antnet/examples/antnet-csma-chain.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/antnet-helper.h"
#include "ns3/antnet-routing-protocol.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AntNetCsmaChain");

int main (int argc, char *argv[])
{
  LogComponentEnable("AntNetRoutingProtocol", LOG_LEVEL_INFO);
  LogComponentEnable("PheromoneTable", LOG_LEVEL_INFO);

  LogComponentEnableAll(LOG_PREFIX_TIME);
  LogComponentEnableAll(LOG_PREFIX_NODE);
  LogComponentEnableAll(LOG_PREFIX_LEVEL);

  double simTime = 7.0;     // Simulation time
  bool   enablePcap = false; // Switch to true if PCAP capture is desired

  CommandLine cmd;
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.AddValue("enablePcap", "Enable CSMA PCAP tracing", enablePcap);
  cmd.Parse(argc, argv);

  //
  // Topology (5 subnets forming a 4-hop chain):
  //
  //   H0 --(LAN0)-- R0 ==(CSMA01)== R1 ==(CSMA12)== R2 ==(CSMA23)== R3 --(LAN4)-- H4
  //
  // Each "==CSMAxx==" represents a CSMA link directly connecting two routers (broadcast-friendly for Hellos).
  //
  NodeContainer routers; routers.Create(4); // R0..R3
  Ptr<Node> R0 = routers.Get(0), R1 = routers.Get(1), R2 = routers.Get(2), R3 = routers.Get(3);

  Ptr<Node> H0 = CreateObject<Node>();  // Left host
  Ptr<Node> H4 = CreateObject<Node>();  // Right host

  // Create each CSMA channel segment
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(5)));

  // LAN0: H0 <-> R0
  NetDeviceContainer lan0 = csma.Install(NodeContainer(H0, R0));

  // R0-R1
  NetDeviceContainer csma01 = csma.Install(NodeContainer(R0, R1));
  // R1-R2
  NetDeviceContainer csma12 = csma.Install(NodeContainer(R1, R2));
  // R2-R3
  NetDeviceContainer csma23 = csma.Install(NodeContainer(R2, R3));

  // LAN4: R3 <-> H4
  NetDeviceContainer lan4 = csma.Install(NodeContainer(R3, H4));

  // Install IPv4 stack and configure AntNet as the routing protocol
  InternetStackHelper stack;
  Ipv4ListRoutingHelper list;
  AntNetHelper antnet;
  list.Add(antnet, 10);            // Assign higher priority to AntNet
  stack.SetRoutingHelper(list);
  stack.Install(NodeContainer(routers, H0, H4));

  // Use distinct subnets for every segment
  Ipv4AddressHelper addr;

  addr.SetBase("10.0.0.0",  "255.255.255.0");  // LAN0
  Ipv4InterfaceContainer if_lan0 = addr.Assign(lan0);

  addr.SetBase("10.0.1.0",  "255.255.255.0");  // R0-R1
  Ipv4InterfaceContainer if_01   = addr.Assign(csma01);

  addr.SetBase("10.0.2.0",  "255.255.255.0");  // R1-R2
  Ipv4InterfaceContainer if_12   = addr.Assign(csma12);

  addr.SetBase("10.0.3.0",  "255.255.255.0");  // R2-R3
  Ipv4InterfaceContainer if_23   = addr.Assign(csma23);

  addr.SetBase("10.0.4.0",  "255.255.255.0");  // LAN4
  Ipv4InterfaceContainer if_lan4 = addr.Assign(lan4);

  auto getAntnet = [] (Ptr<Node> node) -> Ptr<AntNetRoutingProtocol> {
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    NS_ABORT_MSG_IF(ipv4 == nullptr, "Node missing Ipv4 object");
    Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol();
    Ptr<AntNetRoutingProtocol> ant = DynamicCast<AntNetRoutingProtocol>(proto);
    if (ant) {
      return ant;
    }
    Ptr<Ipv4ListRouting> list = DynamicCast<Ipv4ListRouting>(proto);
    NS_ABORT_MSG_IF(list == nullptr, "Ipv4ListRouting not found on node");
    for (uint32_t i = 0; i < list->GetNRoutingProtocols(); ++i) {
      int16_t priority;
      Ptr<Ipv4RoutingProtocol> rp = list->GetRoutingProtocol(i, priority);
      ant = DynamicCast<AntNetRoutingProtocol>(rp);
      if (ant) {
        return ant;
      }
    }
    NS_ABORT_MSG("AntNetRoutingProtocol not found on node");
  };

  Ptr<AntNetRoutingProtocol> antH0 = getAntnet(H0);
  Ptr<AntNetRoutingProtocol> antR0 = getAntnet(R0);
  Ptr<AntNetRoutingProtocol> antR1 = getAntnet(R1);
  Ptr<AntNetRoutingProtocol> antR2 = getAntnet(R2);
  Ptr<AntNetRoutingProtocol> antR3 = getAntnet(R3);
  Ptr<AntNetRoutingProtocol> antH4 = getAntnet(H4);

  antH0->AddStaticNeighbor(if_lan0.GetAddress(1)); // H0 -> R0
  antR0->AddStaticNeighbor(if_lan0.GetAddress(0)); // R0 -> H0
  antR0->AddStaticNeighbor(if_01.GetAddress(1)); // R0 -> R1
  antR1->AddStaticNeighbor(if_01.GetAddress(0)); // R1 -> R0
  antR1->AddStaticNeighbor(if_12.GetAddress(1)); // R1 -> R2
  antR2->AddStaticNeighbor(if_12.GetAddress(0)); // R2 -> R1
  antR2->AddStaticNeighbor(if_23.GetAddress(1)); // R2 -> R3
  antR3->AddStaticNeighbor(if_23.GetAddress(0)); // R3 -> R2
  antR3->AddStaticNeighbor(if_lan4.GetAddress(1)); // R3 -> H4
  antH4->AddStaticNeighbor(if_lan4.GetAddress(0)); // H4 -> R3

  // print node ID mappings
  std::cout << "\n=== Node ID Mapping ===" << std::endl;
  std::cout << "H0 ID: " << H0->GetId() << " (" << if_lan0.GetAddress(0) << ")" << std::endl;
  std::cout << "R0 ID: " << R0->GetId() << " (" << if_lan0.GetAddress(1) << ")" << std::endl;
  std::cout << "R1 ID: " << R1->GetId() << " (" << if_01.GetAddress(1) << ")" << std::endl;
  std::cout << "R2 ID: " << R2->GetId() << " (" << if_12.GetAddress(1) << ")" << std::endl;
  std::cout << "R3 ID: " << R3->GetId() << " (" << if_23.GetAddress(1) << ")" << std::endl;
  std::cout << "H4 ID: " << H4->GetId() << " (" << if_lan4.GetAddress(1) << ")" << std::endl;
  std::cout << "========================\n" << std::endl;

  // // Application: H0 sends UDP traffic to H4; H4 runs a UDP sink
  // uint16_t port = 9000;
  // ApplicationContainer sinkApp;
  // {
  //   PacketSinkHelper sink("ns3::UdpSocketFactory",
  //                         InetSocketAddress(if_lan4.GetAddress(1), port)); // Address of H4
  //   sinkApp = sink.Install(H4);
  //   sinkApp.Start(Seconds(0.5));
  // }
  // {
  //   OnOffHelper onoff("ns3::UdpSocketFactory",
  //                     InetSocketAddress(if_lan4.GetAddress(1), port));
  //   onoff.SetAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  //   onoff.SetAttribute("PacketSize", UintegerValue(512));
  //   onoff.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  //   onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  //   ApplicationContainer src = onoff.Install(H0);
  //   src.Start(Seconds(1.0));
  //   src.Stop(Seconds(simTime - 1));
  // }

  if (enablePcap) {
    csma.EnablePcapAll("antnet-csma", true);
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  antH0->DumpPheromoneTable();
  antR0->DumpPheromoneTable();
  antR1->DumpPheromoneTable();
  antR2->DumpPheromoneTable();
  antR3->DumpPheromoneTable();
  antH4->DumpPheromoneTable();

  // // Print simple stats: total received bytes and average throughput
  // uint64_t rxBytes = DynamicCast<PacketSink>(sinkApp.Get(0))->GetTotalRx();
  // double throughputMbps = (rxBytes * 8.0) / (simTime * 1e6);
  // std::cout << "[RESULT] RX bytes=" << rxBytes
  //           << ", Avg throughput=" << throughputMbps << " Mbps" << std::endl;

  Simulator::Destroy();
  return 0;
}
