#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/antnet-helper.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AntNetExample");

int main(int argc, char *argv[]) {
  LogComponentEnable("AntNetRoutingProtocol", LOG_LEVEL_INFO);
  LogComponentEnable("PheromoneTable", LOG_LEVEL_DEBUG);

  LogComponentEnableAll(LOG_PREFIX_TIME);
  LogComponentEnableAll(LOG_PREFIX_NODE);
  LogComponentEnableAll(LOG_PREFIX_LEVEL);

  uint32_t nNodes = 6;
  double simTime = 120.0;
  double distance = 150.0;

  CommandLine cmd;
  cmd.AddValue("nNodes", "Number of adhoc nodes", nNodes);
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(nNodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());
  phy.Set("TxPowerStart", DoubleValue(3.0));
  phy.Set("TxPowerEnd", DoubleValue(3.0));

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devs = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(distance),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  Ipv4ListRoutingHelper list;
  AntNetHelper antnet;
  list.Add(antnet, 10);
  stack.SetRoutingHelper(list);
  stack.Install(nodes);

  Ipv4AddressHelper ip;
  ip.SetBase("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer ifs = ip.Assign(devs);

  uint16_t port = 9000;
  for (uint32_t i=0; i<nNodes; ++i) {
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(ifs.GetAddress(i), port));
    ApplicationContainer s = sink.Install(nodes.Get(i));
    s.Start(Seconds(0.5));
  }

  Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
  for (uint32_t k=0; k<4; ++k) {
    uint32_t src = rng->GetInteger(0, nNodes-1);
    uint32_t dst = rng->GetInteger(0, nNodes-1);
    if (src == dst) dst = (dst+1) % nNodes;
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(ifs.GetAddress(dst), port));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(200));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer a = onoff.Install(nodes.Get(src));
    a.Start(Seconds(1.0 + 0.5*k));
    a.Stop(Seconds(simTime - 1));
  }

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}
