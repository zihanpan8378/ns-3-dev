// src/antnet/examples/antnet-csma-grid.cc
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

NS_LOG_COMPONENT_DEFINE("AntNetCsmaGrid");


static void SetLinkDelay(const NetDeviceContainer& ndc, Time delay)
{
  // A 2-node CSMA link yields two CsmaNetDevices sharing the same channel.
  for (uint32_t i = 0; i < ndc.GetN(); ++i) {
    Ptr<CsmaNetDevice> dev = DynamicCast<CsmaNetDevice>(ndc.Get(i));
    if (!dev) continue;
    Ptr<CsmaChannel> ch = DynamicCast<CsmaChannel>(dev->GetChannel());
    if (ch) {
      ch->SetAttribute("Delay", TimeValue(delay));
      break; // both ends share the same channel; setting once is enough
    }
  }
}

int main (int argc, char *argv[])
{
  LogComponentEnable("AntNetRoutingProtocol", LOG_LEVEL_INFO);
  LogComponentEnable("PheromoneTable", LOG_LEVEL_INFO);

  LogComponentEnableAll(LOG_PREFIX_TIME);
  LogComponentEnableAll(LOG_PREFIX_NODE);
  LogComponentEnableAll(LOG_PREFIX_LEVEL);

  double simTime = 20.0;    // Simulation time
  bool   enablePcap = false;   // Switch to true if PCAP capture is desired

  CommandLine cmd;
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.AddValue("enablePcap", "Enable CSMA PCAP tracing", enablePcap);
  cmd.Parse(argc, argv);

  // Topology: 3x3 grid of routers with hosts at corners
  //
  //   H0[9] --(LAN_H0)-- R0[0] ==(L01)== R1[1] ==(L12)== R2[2]
  //                      ||               ||              ||
  //                     (L03)            (L14)           (L25)
  //                      ||               ||              ||
  //                     R3[3] ==(L34)== R4[4] ==(L45)== R5[5]
  //                      ||               ||              ||
  //                     (L36)            (L47)           (L58)
  //                      ||               ||              ||
  //                     R6[6] ==(L67)== R7[7] ==(L78)== R8[8] --(LAN_H8)-- H8[10]
  //
  // Grid layout:
  //   R0(0,0) -- R1(0,1) -- R2(0,2)
  //      |         |          |
  //   R3(1,0) -- R4(1,1) -- R5(1,2)
  //      |         |          |
  //   R6(2,0) -- R7(2,1) -- R8(2,2)

  // Create routers
  NodeContainer routers;
  routers.Create(9); // R0..R8
  
  // Create host nodes
  Ptr<Node> H0 = CreateObject<Node>();  // Host connected to R0
  Ptr<Node> H8 = CreateObject<Node>();  // Host connected to R8

  // Setup CSMA helper
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(5)));

  // Create all network device containers
  // Host links
  NetDeviceContainer lan_h0 = csma.Install(NodeContainer(H0, routers.Get(0))); // H0 <-> R0
  NetDeviceContainer lan_h8 = csma.Install(NodeContainer(routers.Get(8), H8)); // R8 <-> H8

  // Horizontal links (rows)
  NetDeviceContainer link_01 = csma.Install(NodeContainer(routers.Get(0), routers.Get(1))); // R0-R1
  SetLinkDelay(link_01, MilliSeconds(2));
  NetDeviceContainer link_12 = csma.Install(NodeContainer(routers.Get(1), routers.Get(2))); // R1-R2
  SetLinkDelay(link_12, MilliSeconds(3));
  NetDeviceContainer link_34 = csma.Install(NodeContainer(routers.Get(3), routers.Get(4))); // R3-R4
  SetLinkDelay(link_34, MilliSeconds(4));
  NetDeviceContainer link_45 = csma.Install(NodeContainer(routers.Get(4), routers.Get(5))); // R4-R5
  SetLinkDelay(link_45, MilliSeconds(5));
  NetDeviceContainer link_67 = csma.Install(NodeContainer(routers.Get(6), routers.Get(7))); // R6-R7
  SetLinkDelay(link_67, MilliSeconds(6));
  NetDeviceContainer link_78 = csma.Install(NodeContainer(routers.Get(7), routers.Get(8))); // R7-R8
  SetLinkDelay(link_78, MilliSeconds(7));


  // Vertical links (columns)
  NetDeviceContainer link_03 = csma.Install(NodeContainer(routers.Get(0), routers.Get(3))); // R0-R3
  SetLinkDelay(link_03, MilliSeconds(8));
  NetDeviceContainer link_36 = csma.Install(NodeContainer(routers.Get(3), routers.Get(6))); // R3-R6
  SetLinkDelay(link_36, MilliSeconds(9));
  NetDeviceContainer link_14 = csma.Install(NodeContainer(routers.Get(1), routers.Get(4))); // R1-R4
  SetLinkDelay(link_14, MilliSeconds(10));
  NetDeviceContainer link_47 = csma.Install(NodeContainer(routers.Get(4), routers.Get(7))); // R4-R7
  SetLinkDelay(link_47, MilliSeconds(11));
  NetDeviceContainer link_25 = csma.Install(NodeContainer(routers.Get(2), routers.Get(5))); // R2-R5
  SetLinkDelay(link_25, MilliSeconds(12));
  NetDeviceContainer link_58 = csma.Install(NodeContainer(routers.Get(5), routers.Get(8))); // R5-R8
  SetLinkDelay(link_58, MilliSeconds(13));

  // Install IPv4 stack and configure AntNet as the routing protocol
  InternetStackHelper stack;
  Ipv4ListRoutingHelper list;
  AntNetHelper antnet;
  list.Add(antnet, 10);  // Assign higher priority to AntNet
  stack.SetRoutingHelper(list);
  stack.Install(NodeContainer(routers, H0, H8));

  // Assign IP addresses to each subnet
  Ipv4AddressHelper addr;
  
  addr.SetBase("10.0.0.0", "255.255.255.0");   // H0-R0
  Ipv4InterfaceContainer if_h0 = addr.Assign(lan_h0);
  
  addr.SetBase("10.0.1.0", "255.255.255.0");   // R0-R1
  Ipv4InterfaceContainer if_01 = addr.Assign(link_01);
  
  addr.SetBase("10.0.2.0", "255.255.255.0");   // R1-R2
  Ipv4InterfaceContainer if_12 = addr.Assign(link_12);
  
  addr.SetBase("10.0.3.0", "255.255.255.0");   // R0-R3
  Ipv4InterfaceContainer if_03 = addr.Assign(link_03);
  
  addr.SetBase("10.0.4.0", "255.255.255.0");   // R3-R4
  Ipv4InterfaceContainer if_34 = addr.Assign(link_34);
  
  addr.SetBase("10.0.5.0", "255.255.255.0");   // R4-R5
  Ipv4InterfaceContainer if_45 = addr.Assign(link_45);
  
  addr.SetBase("10.0.6.0", "255.255.255.0");   // R3-R6
  Ipv4InterfaceContainer if_36 = addr.Assign(link_36);
  
  addr.SetBase("10.0.7.0", "255.255.255.0");   // R6-R7
  Ipv4InterfaceContainer if_67 = addr.Assign(link_67);
  
  addr.SetBase("10.0.8.0", "255.255.255.0");   // R7-R8
  Ipv4InterfaceContainer if_78 = addr.Assign(link_78);
  
  addr.SetBase("10.0.9.0", "255.255.255.0");   // R1-R4
  Ipv4InterfaceContainer if_14 = addr.Assign(link_14);
  
  addr.SetBase("10.0.10.0", "255.255.255.0");  // R4-R7
  Ipv4InterfaceContainer if_47 = addr.Assign(link_47);
  
  addr.SetBase("10.0.11.0", "255.255.255.0");  // R2-R5
  Ipv4InterfaceContainer if_25 = addr.Assign(link_25);
  
  addr.SetBase("10.0.12.0", "255.255.255.0");  // R5-R8
  Ipv4InterfaceContainer if_58 = addr.Assign(link_58);
  
  addr.SetBase("10.0.13.0", "255.255.255.0");  // R8-H8
  Ipv4InterfaceContainer if_h8 = addr.Assign(lan_h8);

  // Helper function to get AntNet routing protocol from a node
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

  // Get AntNet instances for all nodes
  Ptr<AntNetRoutingProtocol> antH0 = getAntnet(H0);
  std::vector<Ptr<AntNetRoutingProtocol>> antRouters;
  for (uint32_t i = 0; i < 9; i++) {
    antRouters.push_back(getAntnet(routers.Get(i)));
  }
  Ptr<AntNetRoutingProtocol> antH8 = getAntnet(H8);

  // Add static neighbors for all connections
  // H0 <-> R0
  antH0->AddStaticNeighbor(if_h0.GetAddress(1));  // H0 -> R0
  antRouters[0]->AddStaticNeighbor(if_h0.GetAddress(0));  // R0 -> H0
  
  // R0 neighbors (R1, R3)
  antRouters[0]->AddStaticNeighbor(if_01.GetAddress(1));  // R0 -> R1
  antRouters[0]->AddStaticNeighbor(if_03.GetAddress(1));  // R0 -> R3
  
  // R1 neighbors (R0, R2, R4)
  antRouters[1]->AddStaticNeighbor(if_01.GetAddress(0));  // R1 -> R0
  antRouters[1]->AddStaticNeighbor(if_12.GetAddress(1));  // R1 -> R2
  antRouters[1]->AddStaticNeighbor(if_14.GetAddress(1));  // R1 -> R4
  
  // R2 neighbors (R1, R5)
  antRouters[2]->AddStaticNeighbor(if_12.GetAddress(0));  // R2 -> R1
  antRouters[2]->AddStaticNeighbor(if_25.GetAddress(1));  // R2 -> R5
  
  // R3 neighbors (R0, R4, R6)
  antRouters[3]->AddStaticNeighbor(if_03.GetAddress(0));  // R3 -> R0
  antRouters[3]->AddStaticNeighbor(if_34.GetAddress(1));  // R3 -> R4
  antRouters[3]->AddStaticNeighbor(if_36.GetAddress(1));  // R3 -> R6
  
  // R4 neighbors (R1, R3, R5, R7) - center node with 4 neighbors
  antRouters[4]->AddStaticNeighbor(if_14.GetAddress(0));  // R4 -> R1
  antRouters[4]->AddStaticNeighbor(if_34.GetAddress(0));  // R4 -> R3
  antRouters[4]->AddStaticNeighbor(if_45.GetAddress(1));  // R4 -> R5
  antRouters[4]->AddStaticNeighbor(if_47.GetAddress(1));  // R4 -> R7
  
  // R5 neighbors (R2, R4, R8)
  antRouters[5]->AddStaticNeighbor(if_25.GetAddress(0));  // R5 -> R2
  antRouters[5]->AddStaticNeighbor(if_45.GetAddress(0));  // R5 -> R4
  antRouters[5]->AddStaticNeighbor(if_58.GetAddress(1));  // R5 -> R8
  
  // R6 neighbors (R3, R7)
  antRouters[6]->AddStaticNeighbor(if_36.GetAddress(0));  // R6 -> R3
  antRouters[6]->AddStaticNeighbor(if_67.GetAddress(1));  // R6 -> R7
  
  // R7 neighbors (R4, R6, R8)
  antRouters[7]->AddStaticNeighbor(if_47.GetAddress(0));  // R7 -> R4
  antRouters[7]->AddStaticNeighbor(if_67.GetAddress(0));  // R7 -> R6
  antRouters[7]->AddStaticNeighbor(if_78.GetAddress(1));  // R7 -> R8
  
  // R8 neighbors (R5, R7, H8)
  antRouters[8]->AddStaticNeighbor(if_58.GetAddress(0));  // R8 -> R5
  antRouters[8]->AddStaticNeighbor(if_78.GetAddress(0));  // R8 -> R7
  antRouters[8]->AddStaticNeighbor(if_h8.GetAddress(1));  // R8 -> H8
  
  // H8 <-> R8
  antH8->AddStaticNeighbor(if_h8.GetAddress(0));  // H8 -> R8

  // Print node ID mappings
  std::cout << "\n=== Node ID Mapping ===" << std::endl;
  std::cout << "H0 ID: " << H0->GetId() << " (" << if_h0.GetAddress(0) << ")" << std::endl;
  std::cout << "R0 ID: " << routers.Get(0)->GetId() << " (" << if_h0.GetAddress(1) << ")" << std::endl;
  std::cout << "R1 ID: " << routers.Get(1)->GetId() << " (" << if_01.GetAddress(1) << ")" << std::endl;
  std::cout << "R2 ID: " << routers.Get(2)->GetId() << " (" << if_12.GetAddress(1) << ")" << std::endl;
  std::cout << "R3 ID: " << routers.Get(3)->GetId() << " (" << if_03.GetAddress(1) << ")" << std::endl;
  std::cout << "R4 ID: " << routers.Get(4)->GetId() << " (" << if_34.GetAddress(1) << ")" << std::endl;
  std::cout << "R5 ID: " << routers.Get(5)->GetId() << " (" << if_45.GetAddress(1) << ")" << std::endl;
  std::cout << "R6 ID: " << routers.Get(6)->GetId() << " (" << if_36.GetAddress(1) << ")" << std::endl;
  std::cout << "R7 ID: " << routers.Get(7)->GetId() << " (" << if_67.GetAddress(1) << ")" << std::endl;
  std::cout << "R8 ID: " << routers.Get(8)->GetId() << " (" << if_h8.GetAddress(0) << ")" << std::endl;
  std::cout << "H8 ID: " << H8->GetId() << " (" << if_h8.GetAddress(1) << ")" << std::endl;
  std::cout << "========================\n" << std::endl;

  // Optional: Application traffic from H0 to H8
  // Uncomment to enable UDP traffic
  /*
  uint16_t port = 9000;
  ApplicationContainer sinkApp;
  {
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(if_h8.GetAddress(1), port)); // Address of H8
    sinkApp = sink.Install(H8);
    sinkApp.Start(Seconds(0.5));
  }
  {
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(if_h8.GetAddress(1), port));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer src = onoff.Install(H0);
    src.Start(Seconds(1.0));
    src.Stop(Seconds(simTime - 1));
  }
  */

  if (enablePcap) {
    csma.EnablePcapAll("antnet-csma-grid", true);
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  // Dump pheromone tables
  std::cout << "\n=== Pheromone Tables ===" << std::endl;
  std::cout << "\n--- H0 ---" << std::endl;
  antH0->DumpPheromoneTable();
  for (uint32_t i = 0; i < 9; i++) {
    std::cout << "\n--- R" << i << " ---" << std::endl;
    antRouters[i]->DumpPheromoneTable();
  }
  std::cout << "\n--- H8 ---" << std::endl;
  antH8->DumpPheromoneTable();

  // Optional: Print traffic statistics
  /*
  uint64_t rxBytes = DynamicCast<PacketSink>(sinkApp.Get(0))->GetTotalRx();
  double throughputMbps = (rxBytes * 8.0) / (simTime * 1e6);
  std::cout << "\n[RESULT] RX bytes=" << rxBytes
            << ", Avg throughput=" << throughputMbps << " Mbps" << std::endl;
  */

  Simulator::Destroy();
  return 0;
}
