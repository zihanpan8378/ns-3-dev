#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"          // BridgeHelper
#include "ns3/mesh-module.h"            // MeshHelper (802.11s)
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"

#include "ns3/ipv4-antnet-routing-helper.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AntNetTopologyNxyExample");

// ─────────────────────────────────────────────────────────────
// Helper: install ESS (multiple APs share same SSID/channel) + one STA
// Returns AP devices (one per AP node) + STA device
// ─────────────────────────────────────────────────────────────
struct EssWifiDevices
{
  std::vector<NetDeviceContainer> apDevs; // each size=1
  NetDeviceContainer staDev;             // size=1
};

static EssWifiDevices
InstallEssMultiApOneSta(const std::vector<Ptr<Node>>& apNodes,
                        Ptr<Node> staNode,
                        const std::string& ssidStr,
                        double txPowerDbm,
                        uint32_t channelNumber,
                        WifiHelper& wifi)
{
  // One shared channel for ESS
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  Ptr<YansWifiChannel> ch = channel.Create();

  YansWifiPhyHelper phy;
  phy.SetChannel(ch);
  phy.Set("TxPowerStart", DoubleValue(txPowerDbm));
  phy.Set("TxPowerEnd", DoubleValue(txPowerDbm));
  phy.Set("ChannelNumber", UintegerValue(channelNumber));

  WifiMacHelper mac;
  Ssid ssid = Ssid(ssidStr);

  EssWifiDevices out;
  out.apDevs.resize(apNodes.size());

  for (size_t i = 0; i < apNodes.size(); ++i)
  {
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    out.apDevs[i] = wifi.Install(phy, mac, apNodes[i]);
  }

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(true));
  out.staDev = wifi.Install(phy, mac, staNode);

  return out;
}

static std::string
MakeSubnet(uint32_t a, uint32_t b, uint32_t c)
{
  std::ostringstream oss;
  oss << a << "." << b << "." << c << ".0";
  return oss.str();
}

int
main(int argc, char* argv[])
{
  // ── Parameters
  uint32_t n = 4; // Upper AP count (STA2 chooses)
  uint32_t x = 3; // Lower group A count: N12~Nx (mesh only)
  uint32_t y = 3; // Lower group B count: N13~Ny (mesh + AP for STA1)

  double hopDistance = 30.0;
  double sta1Distance = 60.0;
  double sta2Distance = 60.0;

  std::string p2pRate = "50Mbps";
  std::string lanRate = "100Mbps";

  double simTimeSec = 30.0;

  // Channels (separate to reduce interference)
  uint32_t upperAccessChannel = 6;
  uint32_t upperMeshChannel   = 1;
  uint32_t lowerAccessChannel = 11;
  uint32_t lowerMeshChannel   = 3;

  CommandLine cmd(__FILE__);
  cmd.AddValue("n", "Upper equivalent AP count for STA2 to choose", n);
  cmd.AddValue("x", "Lower group A count (N12~Nx), mesh-only", x);
  cmd.AddValue("y", "Lower group B count (N13~Ny), mesh + AP for STA1", y);
  cmd.AddValue("hopDistance", "Distance between adjacent equivalent nodes (meters)", hopDistance);
  cmd.AddValue("sta1Distance", "Offset distance for STA1 placement (meters)", sta1Distance);
  cmd.AddValue("sta2Distance", "Offset distance for STA2 placement (meters)", sta2Distance);
  cmd.AddValue("simTimeSec", "Simulation time in seconds", simTimeSec);

  cmd.AddValue("upperAccessChannel", "Upper ESS channel (STA2 <-> upper APs)", upperAccessChannel);
  cmd.AddValue("upperMeshChannel", "Upper mesh channel (N10 <-> upper APs)", upperMeshChannel);
  cmd.AddValue("lowerAccessChannel", "Lower ESS channel (STA1 <-> groupB APs)", lowerAccessChannel);
  cmd.AddValue("lowerMeshChannel", "Lower mesh channel (N3 + groupA + groupB)", lowerMeshChannel);

  cmd.Parse(argc, argv);

  // AntNet defaults
  Config::SetDefault("ns3::Ipv4AntNetRouting::ForwardAntInterval", TimeValue(Seconds(5)));
  Config::SetDefault("ns3::Ipv4AntNetRouting::UseBeaconWindow", BooleanValue(true));
  Config::SetDefault("ns3::Ipv4AntNetRouting::UseFailureMessagePropagation", BooleanValue(false));

  // IMPORTANT: N10 / N3 need to forward between wired and mesh domains
  Config::SetDefault("ns3::Ipv4L3Protocol::IpForward", BooleanValue(true));

  // ─────────────────────────────────────────────────────────────
  // 1) Create nodes
  // ─────────────────────────────────────────────────────────────
  Ptr<Node> N0 = CreateObject<Node>();
  Ptr<Node> N1 = CreateObject<Node>();
  Ptr<Node> N2 = CreateObject<Node>();
  Ptr<Node> N3 = CreateObject<Node>();
  Ptr<Node> N4 = CreateObject<Node>();
  Ptr<Node> N5 = CreateObject<Node>();
  Ptr<Node> N6 = CreateObject<Node>();
  Ptr<Node> N9 = CreateObject<Node>();
  Ptr<Node> N10 = CreateObject<Node>();

  Ptr<Node> H0 = CreateObject<Node>();
  Ptr<Node> H1 = CreateObject<Node>();
  Ptr<Node> H2 = CreateObject<Node>();

  Ptr<Node> STA1 = CreateObject<Node>();
  Ptr<Node> STA2 = CreateObject<Node>();

  // Upper APs (for STA2)
  std::vector<Ptr<Node>> upperAps;
  upperAps.reserve(n);
  for (uint32_t i = 0; i < n; ++i)
  {
    upperAps.push_back(CreateObject<Node>());
  }

  // Lower group A: N12~Nx (mesh-only)
  std::vector<Ptr<Node>> lowerGroupA;
  lowerGroupA.reserve(x);
  for (uint32_t i = 0; i < x; ++i)
  {
    lowerGroupA.push_back(CreateObject<Node>());
  }

  // Lower group B: N13~Ny (mesh + AP for STA1)
  std::vector<Ptr<Node>> lowerGroupB;
  lowerGroupB.reserve(y);
  for (uint32_t i = 0; i < y; ++i)
  {
    lowerGroupB.push_back(CreateObject<Node>());
  }

  NodeContainer all;
  all.Add(N0);
  all.Add(N1);
  all.Add(N2);
  all.Add(N3);
  all.Add(N4);
  all.Add(N5);
  all.Add(N6);
  all.Add(N9);
  all.Add(N10);

  all.Add(H0);
  all.Add(H1);
  all.Add(H2);

  for (auto& ap : upperAps) all.Add(ap);
  for (auto& a : lowerGroupA) all.Add(a);
  for (auto& b : lowerGroupB) all.Add(b);

  all.Add(STA1);
  all.Add(STA2);

  // ─────────────────────────────────────────────────────────────
  // 2) Internet + AntNet routing
  // ─────────────────────────────────────────────────────────────
  InternetStackHelper internet;
  internet.SetRoutingHelper(Ipv4AntNetRoutingHelper());
  internet.Install(all);

  // ─────────────────────────────────────────────────────────────
  // 3) Wired links (P2P + LAN CSMA)
  // ─────────────────────────────────────────────────────────────
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(p2pRate));
  p2p.SetDeviceAttribute("Mtu", UintegerValue(1500));

  auto InstallP2p = [&](Ptr<Node> a, Ptr<Node> b, const std::string& delay) -> NetDeviceContainer {
    p2p.SetChannelAttribute("Delay", StringValue(delay));
    return p2p.Install(a, b);
  };

  NetDeviceContainer d0_2  = InstallP2p(N0, N2, "15ms");
  NetDeviceContainer d2_1  = InstallP2p(N2, N1, "20ms");
  NetDeviceContainer d2_9  = InstallP2p(N2, N9, "15ms");
  NetDeviceContainer d6_9  = InstallP2p(N6, N9, "10ms");
  NetDeviceContainer d2_3  = InstallP2p(N2, N3, "13ms");
  NetDeviceContainer d1_3  = InstallP2p(N1, N3, "3ms");
  NetDeviceContainer d9_3  = InstallP2p(N9, N3, "16ms");
  NetDeviceContainer d6_10 = InstallP2p(N6, N10, "5ms");

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue(lanRate));
  csma.SetChannelAttribute("Delay", StringValue("0.1ms"));

  NodeContainer lanNodes;
  lanNodes.Add(N0);
  lanNodes.Add(N4);
  lanNodes.Add(N5);
  lanNodes.Add(N6);
  lanNodes.Add(H0);
  lanNodes.Add(H1);
  lanNodes.Add(H2);
  NetDeviceContainer dLan = csma.Install(lanNodes);

  // ─────────────────────────────────────────────────────────────
  // 4) Addressing for wired
  // ─────────────────────────────────────────────────────────────
  Ipv4AddressHelper ipv4;
  const std::string mask = "255.255.255.0";
  uint32_t p2pNet = 1;

  auto AssignP2p = [&](NetDeviceContainer d) {
    std::string base = MakeSubnet(10, 1, p2pNet++);
    ipv4.SetBase(Ipv4Address(base.c_str()), Ipv4Mask(mask.c_str()));
    ipv4.Assign(d);
  };

  AssignP2p(d0_2);
  AssignP2p(d2_1);
  AssignP2p(d2_9);
  AssignP2p(d6_9);
  AssignP2p(d2_3);
  AssignP2p(d1_3);
  AssignP2p(d9_3);
  AssignP2p(d6_10);

  ipv4.SetBase("10.8.0.0", mask.c_str());
  ipv4.Assign(dLan);

  // ─────────────────────────────────────────────────────────────
  // 5) WiFi config (for ESS AP/STA)
  // ─────────────────────────────────────────────────────────────
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode",
                               StringValue("ErpOfdmRate24Mbps"),
                               "ControlMode",
                               StringValue("ErpOfdmRate6Mbps"));

  double txPowerDbm = 16.0;

  // ─────────────────────────────────────────────────────────────
  // 6) Upper: ESS (upperAps as APs) + Mesh backhaul (N10 + upperAps)
  //    Each upper AP bridges (Access AP dev) <-> (Mesh dev)
  //    IPs: N10 mesh + STA2 + each AP bridge dev all in same subnet 10.20.0.0/24
  // ─────────────────────────────────────────────────────────────
  EssWifiDevices upperEss = InstallEssMultiApOneSta(upperAps, STA2, "STA2-ESS", txPowerDbm, upperAccessChannel, wifi);

  // Mesh nodes: N10 + upperAps
  NodeContainer upperMeshNodes;
  upperMeshNodes.Add(N10);
  for (auto& ap : upperAps) upperMeshNodes.Add(ap);

  // Mesh PHY
  YansWifiChannelHelper upperMeshCh = YansWifiChannelHelper::Default();
  YansWifiPhyHelper upperMeshPhy;
  upperMeshPhy.SetChannel(upperMeshCh.Create());
  upperMeshPhy.Set("TxPowerStart", DoubleValue(txPowerDbm));
  upperMeshPhy.Set("TxPowerEnd", DoubleValue(txPowerDbm));
  upperMeshPhy.Set("ChannelNumber", UintegerValue(upperMeshChannel));

  MeshHelper upperMesh;
  upperMesh.SetStackInstaller("ns3::Dot11sStack");
  upperMesh.SetSpreadInterfaceChannels(MeshHelper::SINGLE_CHANNEL);
  upperMesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));

  NetDeviceContainer upperMeshDevs = upperMesh.Install(upperMeshPhy, upperMeshNodes);

  // Bridge on each AP: bridge its mesh dev with its access AP dev
  BridgeHelper bridge;
  std::vector<Ptr<NetDevice>> upperBridgeDevs;
  upperBridgeDevs.reserve(n);

  for (uint32_t i = 0; i < n; ++i)
  {
    Ptr<NetDevice> apMeshDev = upperMeshDevs.Get(i + 1);          // [0]=N10, [i+1]=APi
    Ptr<NetDevice> apAccessDev = upperEss.apDevs[i].Get(0);       // AP dev

    NetDeviceContainer ports;
    ports.Add(apMeshDev);
    ports.Add(apAccessDev);

    NetDeviceContainer bd = bridge.Install(upperAps[i], ports);   // BridgeNetDevice
    upperBridgeDevs.push_back(bd.Get(0));
  }

  // Assign IPs in 10.20.0.0/24 to: N10 mesh, STA2 sta, and AP bridge devs
  {
    ipv4.SetBase("10.20.0.0", mask.c_str());
    NetDeviceContainer ipDevs;

    ipDevs.Add(upperMeshDevs.Get(0)); // N10 mesh
    ipDevs.Add(upperEss.staDev);      // STA2

    for (auto& bd : upperBridgeDevs) ipDevs.Add(bd); // each AP bridge dev has IP (so it can run AntNet too)

    ipv4.Assign(ipDevs);
  }

  // ─────────────────────────────────────────────────────────────
  // 7) Lower RIGHT: Two groups + mesh interconnect + STA1 can choose any AP in groupB
  //
  // Requirement:
  // - groupA (N12~Nx) <-> groupB (N13~Ny) interconnect (mesh)
  // - groupA nodes can connect with N3 (mesh)
  // - STA1 can connect any node in groupB (ESS access)
  //
  // Implementation:
  // - Mesh nodes: N3 + groupA + groupB on lowerMeshChannel
  // - Access ESS: groupB as APs + STA1 on lowerAccessChannel
  // - Bridge each groupB node: (mesh dev) <-> (access AP dev)
  // - IPs on same subnet 10.30.0.0/24 for: N3 mesh, groupA mesh devs, groupB bridge devs, STA1 sta
  // ─────────────────────────────────────────────────────────────

  // 7.1 Access ESS for STA1: APs are groupB
  EssWifiDevices lowerEss = InstallEssMultiApOneSta(lowerGroupB, STA1, "STA1-ESS", txPowerDbm, lowerAccessChannel, wifi);

  // 7.2 Mesh nodes: N3 + groupA + groupB
  NodeContainer lowerMeshNodes;
  lowerMeshNodes.Add(N3);
  for (auto& a : lowerGroupA) lowerMeshNodes.Add(a);
  for (auto& b : lowerGroupB) lowerMeshNodes.Add(b);

  YansWifiChannelHelper lowerMeshCh = YansWifiChannelHelper::Default();
  YansWifiPhyHelper lowerMeshPhy;
  lowerMeshPhy.SetChannel(lowerMeshCh.Create());
  lowerMeshPhy.Set("TxPowerStart", DoubleValue(txPowerDbm));
  lowerMeshPhy.Set("TxPowerEnd", DoubleValue(txPowerDbm));
  lowerMeshPhy.Set("ChannelNumber", UintegerValue(lowerMeshChannel));

  MeshHelper lowerMesh;
  lowerMesh.SetStackInstaller("ns3::Dot11sStack");
  lowerMesh.SetSpreadInterfaceChannels(MeshHelper::SINGLE_CHANNEL);
  lowerMesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));

  NetDeviceContainer lowerMeshDevs = lowerMesh.Install(lowerMeshPhy, lowerMeshNodes);
  // Index mapping:
  // lowerMeshDevs[0] = N3 mesh
  // lowerMeshDevs[1..x] = groupA mesh devs
  // lowerMeshDevs[x+1 .. x+y] = groupB mesh devs

  // 7.3 Bridge on each groupB node: mesh dev <-> access AP dev
  std::vector<Ptr<NetDevice>> lowerBridgeDevs;
  lowerBridgeDevs.reserve(y);

  for (uint32_t i = 0; i < y; ++i)
  {
    Ptr<NetDevice> bMeshDev = lowerMeshDevs.Get(1 + x + i);   // groupB start at 1+x
    Ptr<NetDevice> bAccessApDev = lowerEss.apDevs[i].Get(0);  // its AP device in ESS

    NetDeviceContainer ports;
    ports.Add(bMeshDev);
    ports.Add(bAccessApDev);

    NetDeviceContainer bd = bridge.Install(lowerGroupB[i], ports);
    lowerBridgeDevs.push_back(bd.Get(0));
  }

  // 7.4 Assign IPs in 10.30.0.0/24:
  // - N3 mesh dev
  // - groupA mesh devs
  // - groupB bridge devs
  // - STA1 STA dev
  {
    ipv4.SetBase("10.30.0.0", mask.c_str());
    NetDeviceContainer ipDevs;

    ipDevs.Add(lowerMeshDevs.Get(0)); // N3 mesh

    for (uint32_t i = 0; i < x; ++i)
    {
      ipDevs.Add(lowerMeshDevs.Get(1 + i)); // groupA mesh devs
    }

    for (auto& bd : lowerBridgeDevs)
    {
      ipDevs.Add(bd); // groupB bridge devs
    }

    ipDevs.Add(lowerEss.staDev); // STA1

    ipv4.Assign(ipDevs);
  }

  // ─────────────────────────────────────────────────────────────
  // 8) Mobility (rough placement)
  // ─────────────────────────────────────────────────────────────
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();

  auto AddPos = [&](Ptr<Node> node, double x, double y, double z = 0.0) {
    pos->Add(Vector(x, y, z));
  };

  // Wired rough cluster
  AddPos(N0, 0, 200);
  AddPos(N4, 50, 220);
  AddPos(N5, 80, 220);
  AddPos(N6, 120, 220);

  AddPos(H0, 30, 250);
  AddPos(H1, 60, 250);
  AddPos(H2, 90, 250);

  AddPos(N2, 0, 120);
  AddPos(N1, 0, 60);
  AddPos(N9, 120, 120);
  AddPos(N3, 180, 60);
  AddPos(N10, 180, 200);

  // Upper APs line near N10
  double ux0 = 240.0;
  double uy0 = 200.0;
  for (uint32_t i = 0; i < n; ++i)
  {
    AddPos(upperAps[i], ux0 + i * hopDistance, uy0);
  }
  // STA2 above upper AP line
  double sta2x = (n > 0) ? (ux0 + (n - 1) * hopDistance) : ux0;
  AddPos(STA2, sta2x, uy0 + sta2Distance);

  // Lower groupA (N12~Nx) line near N3 (same y as N3)
  double ax0 = 240.0;
  double ay0 = 60.0;
  for (uint32_t i = 0; i < x; ++i)
  {
    AddPos(lowerGroupA[i], ax0 + i * hopDistance, ay0);
  }

  // Lower groupB (N13~Ny) line below groupA
  double bx0 = 240.0;
  double by0 = 30.0;
  for (uint32_t i = 0; i < y; ++i)
  {
    AddPos(lowerGroupB[i], bx0 + i * hopDistance, by0);
  }

  // STA1 near the groupB line (to the right)
  double sta1x = (y > 0) ? (bx0 + (y - 1) * hopDistance) : bx0;
  AddPos(STA1, sta1x + sta1Distance, by0);

  mobility.SetPositionAllocator(pos);
  mobility.Install(all);

  // ─────────────────────────────────────────────────────────────
  // 9) AntNet build + init
  // ─────────────────────────────────────────────────────────────
  Ipv4AntNetRoutingHelper::BuildAntNetTopology();
  Ipv4AntNetRoutingHelper::InitializeNodeRoutingTables();

  // ─────────────────────────────────────────────────────────────
  // 10) Simple traffic: STA1 -> STA2 (UDP Echo)
  // ─────────────────────────────────────────────────────────────
  auto GetFirstNonLoopback = [](Ptr<Node> node) -> Ipv4Address {
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i)
    {
      for (uint32_t j = 0; j < ipv4->GetNAddresses(i); ++j)
      {
        Ipv4InterfaceAddress ifAddr = ipv4->GetAddress(i, j);
        if (ifAddr.GetLocal() != Ipv4Address("127.0.0.1"))
        {
          return ifAddr.GetLocal();
        }
      }
    }
    return Ipv4Address("0.0.0.0");
  };

  Ipv4Address sta2Addr = GetFirstNonLoopback(STA2);

  uint16_t port = 9;
  UdpEchoServerHelper server(port);
  ApplicationContainer apps = server.Install(STA2);
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(simTimeSec - 1));

  UdpEchoClientHelper client(sta2Addr, port);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
  client.SetAttribute("PacketSize", UintegerValue(200));

  apps = client.Install(STA1);
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(simTimeSec - 1));

  Simulator::Stop(Seconds(simTimeSec));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}
