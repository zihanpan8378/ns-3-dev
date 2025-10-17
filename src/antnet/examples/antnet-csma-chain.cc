// src/antnet/examples/antnet-csma-chain.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/antnet-helper.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AntNetCsmaChain");

int main (int argc, char *argv[])
{
  LogComponentEnable("AntNetRoutingProtocol", LOG_LEVEL_DEBUG);
  LogComponentEnable("PheromoneTable", LOG_LEVEL_DEBUG);

  LogComponentEnableAll(LOG_PREFIX_TIME);
  LogComponentEnableAll(LOG_PREFIX_NODE);
  LogComponentEnableAll(LOG_PREFIX_LEVEL);

  double simTime = 30.0;     // Simulation time
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

  // Application: H0 sends UDP traffic to H4; H4 runs a UDP sink
  uint16_t port = 9000;
  ApplicationContainer sinkApp;
  {
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(if_lan4.GetAddress(1), port)); // Address of H4
    sinkApp = sink.Install(H4);
    sinkApp.Start(Seconds(0.5));
  }
  {
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(if_lan4.GetAddress(1), port));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer src = onoff.Install(H0);
    src.Start(Seconds(1.0));
    src.Stop(Seconds(simTime - 1));
  }

  if (enablePcap) {
    csma.EnablePcapAll("antnet-csma", true);
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  // Print simple stats: total received bytes and average throughput
  uint64_t rxBytes = DynamicCast<PacketSink>(sinkApp.Get(0))->GetTotalRx();
  double throughputMbps = (rxBytes * 8.0) / (simTime * 1e6);
  std::cout << "[RESULT] RX bytes=" << rxBytes
            << ", Avg throughput=" << throughputMbps << " Mbps" << std::endl;

  Simulator::Destroy();
  return 0;
}
