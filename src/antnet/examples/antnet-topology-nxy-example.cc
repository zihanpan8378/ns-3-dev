#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/mesh-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/node-list.h"
#include "ns3/timestamp-tag.h"
#include "ns3/propagation-module.h"

#include "ns3/ipv4-antnet-routing-helper.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AntNetTopologyNxyExample");

// ─────────────────────────────────────────────────────────────
// (NEW) STA2 ping-pong movement helpers
// ─────────────────────────────────────────────────────────────
static double
KmphToMps(double kmph)
{
  return kmph * 1000.0 / 3600.0;
}

static void
Sta2ToggleVelocityAndReschedule(Ptr<ConstantVelocityMobilityModel> mob,
                                double speedMps,
                                double oneWaySeconds,
                                double stopTimeSeconds)
{
  double now = Simulator::Now().GetSeconds();
  if (now + 1e-9 >= stopTimeSeconds)
  {
    return;
  }

  // Toggle x velocity between +speed and -speed
  Vector v = mob->GetVelocity();
  double vx = (v.x >= 0.0) ? -std::abs(speedMps) : std::abs(speedMps);
  mob->SetVelocity(Vector(vx, 0.0, 0.0));

  // Schedule next toggle if still within sim time
  if (now + oneWaySeconds < stopTimeSeconds - 1e-9)
  {
    Simulator::Schedule(Seconds(oneWaySeconds),
                        &Sta2ToggleVelocityAndReschedule,
                        mob, speedMps, oneWaySeconds, stopTimeSeconds);
  }
}

// ─────────────────────────────────────────────────────────────
// Per-second stats + JSON dump (double arrays)
// ─────────────────────────────────────────────────────────────
struct PerSecondStats
{
  uint32_t numSeconds{0};
  std::vector<double> sentCount;
  std::vector<double> recvCount;
  std::vector<double> sumDelaySec;

  void Init(double simTimeSec)
  {
    numSeconds = std::max<uint32_t>(1, static_cast<uint32_t>(std::ceil(simTimeSec)));
    sentCount.assign(numSeconds, 0.0);
    recvCount.assign(numSeconds, 0.0);
    sumDelaySec.assign(numSeconds, 0.0);
  }

  uint32_t NowBucket() const
  {
    double t = Simulator::Now().GetSeconds();
    if (t < 0) return 0;
    uint32_t sec = static_cast<uint32_t>(std::floor(t));
    if (sec >= numSeconds) sec = numSeconds - 1;
    return sec;
  }

  void OnSend()
  {
    sentCount[NowBucket()] += 1.0;
  }

  void OnRecv(double delaySeconds)
  {
    uint32_t b = NowBucket();
    recvCount[b] += 1.0;
    sumDelaySec[b] += delaySeconds;
  }

  std::vector<double> BuildLatencyAvg() const
  {
    std::vector<double> latency(numSeconds, 0.0);
    for (uint32_t i = 0; i < numSeconds; ++i)
    {
      if (recvCount[i] > 0.0)
      {
        latency[i] = sumDelaySec[i] / recvCount[i];
      }
    }
    return latency;
  }

  std::vector<double> BuildLossRate() const
  {
    std::vector<double> loss(numSeconds, 0.0);
    for (uint32_t i = 0; i < numSeconds; ++i)
    {
      if (sentCount[i] > 0.0)
      {
        double lost = sentCount[i] - recvCount[i];
        if (lost < 0.0) lost = 0.0;
        loss[i] = lost / sentCount[i];
      }
    }
    return loss;
  }

  static void WriteJsonDoubleArray(const std::string& path, const std::vector<double>& arr)
  {
    std::ofstream ofs(path, std::ios::out | std::ios::trunc);
    if (!ofs.is_open())
    {
      NS_LOG_UNCOND("[METRICS] Failed to open output file: " << path);
      return;
    }

    ofs << "[";
    ofs << std::setprecision(12);
    for (size_t i = 0; i < arr.size(); ++i)
    {
      if (i) ofs << ",";
      ofs << arr[i];
    }
    ofs << "]\n";
    ofs.close();
  }
};

// ─────────────────────────────────────────────────────────────
// UDP one-way timestamp client using TimestampTag
// ─────────────────────────────────────────────────────────────
class UdpTsClientApp : public Application
{
public:
  UdpTsClientApp() = default;

  void Setup(Ipv4Address dst, uint16_t port, Time interval, uint32_t packetSize, PerSecondStats* stats)
  {
    m_dst = dst;
    m_port = port;
    m_interval = interval;
    m_packetSize = packetSize;
    m_stats = stats;
  }

private:
  void StartApplication() override
  {
    m_running = true;

    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(InetSocketAddress(m_dst, m_port));

    SendOne();
  }

  void StopApplication() override
  {
    m_running = false;

    if (m_sendEvent.IsPending())
    {
      Simulator::Cancel(m_sendEvent);
    }

    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
  }

  void ScheduleNext()
  {
    if (!m_running) return;
    m_sendEvent = Simulator::Schedule(m_interval, &UdpTsClientApp::SendOne, this);
  }

  void SendOne()
  {
    if (!m_running) return;

    Ptr<Packet> p = Create<Packet>(m_packetSize);

    TimestampTag tag;
    tag.SetTimestamp(Simulator::Now());
    p->AddPacketTag(tag);

    m_socket->Send(p);
    if (m_stats)
    {
      m_stats->OnSend();
    }

    ScheduleNext();
  }

private:
  Ptr<Socket> m_socket;
  Ipv4Address m_dst{Ipv4Address("0.0.0.0")};
  uint16_t m_port{0};
  Time m_interval{MilliSeconds(200)};
  uint32_t m_packetSize{200};

  bool m_running{false};
  EventId m_sendEvent;
  PerSecondStats* m_stats{nullptr};
};

// ─────────────────────────────────────────────────────────────
// UDP timestamp server using TimestampTag
// ─────────────────────────────────────────────────────────────
class UdpTsServerApp : public Application
{
public:
  UdpTsServerApp() = default;

  void Setup(uint16_t port, PerSecondStats* stats)
  {
    m_port = port;
    m_stats = stats;
  }

private:
  void StartApplication() override
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpTsServerApp::HandleRead, this));
  }

  void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
      TimestampTag tag;
      if (packet->PeekPacketTag(tag))
      {
        Time sendTs = tag.GetTimestamp();
        Time delay = Simulator::Now() - sendTs;
        double delaySec = delay.GetSeconds();
        if (delaySec < 0) delaySec = 0.0;

        if (m_stats)
        {
          m_stats->OnRecv(delaySec);
        }
      }
      else
      {
        if (m_stats)
        {
          m_stats->OnRecv(0.0);
        }
      }
    }
  }

private:
  Ptr<Socket> m_socket;
  uint16_t m_port{0};
  PerSecondStats* m_stats{nullptr};
};

// ─────────────────────────────────────────────────────────────
// Soft failure: bring IPv4 interfaces down/up
// ─────────────────────────────────────────────────────────────
static void
SetNodeIpv4InterfacesUpDown(Ptr<Node> node, bool up, int32_t ifIndex)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
  if (!ipv4)
  {
    return;
  }

  auto SetOne = [&](uint32_t i) {
    if (up) ipv4->SetUp(i);
    else ipv4->SetDown(i);
  };

  if (ifIndex >= 0)
  {
    uint32_t ui = static_cast<uint32_t>(ifIndex);
    if (ui < ipv4->GetNInterfaces())
    {
      SetOne(ui);
    }
    return;
  }

  for (uint32_t i = 1; i < ipv4->GetNInterfaces(); ++i)
  {
    SetOne(i);
  }
}

static void
SoftBringNodeDown(Ptr<Node> node, int32_t ifIndex)
{
  NS_LOG_UNCOND("[SOFT-FAIL] NodeId=" << node->GetId()
                                     << " DOWN @ " << Simulator::Now().GetSeconds()
                                     << "s (ifIndex=" << ifIndex << ")");
  SetNodeIpv4InterfacesUpDown(node, false, ifIndex);
}

static void
SoftBringNodeUp(Ptr<Node> node, int32_t ifIndex)
{
  NS_LOG_UNCOND("[SOFT-FAIL] NodeId=" << node->GetId()
                                     << " UP @ " << Simulator::Now().GetSeconds()
                                     << "s (ifIndex=" << ifIndex << ")");
  SetNodeIpv4InterfacesUpDown(node, true, ifIndex);
}

static void
EnableNodeForwarding(Ptr<Node> node)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
  if (!ipv4)
  {
    return;
  }
  for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i)
  {
    ipv4->SetForwarding(i, true);
  }
}

static void
EnableForwardingOnNodes(const std::vector<Ptr<Node>>& nodes)
{
  for (auto& n : nodes)
  {
    EnableNodeForwarding(n);
  }
}

// ─────────────────────────────────────────────────────────────
// ESS helper (maxRangeMeters to keep km reachable)
// ─────────────────────────────────────────────────────────────
struct EssWifiDevices
{
  std::vector<NetDeviceContainer> apDevs;
  NetDeviceContainer staDev;
};

static EssWifiDevices
InstallEssMultiApOneSta(const std::vector<Ptr<Node>>& apNodes,
                        Ptr<Node> staNode,
                        const std::string& ssidStr,
                        double txPowerDbm,
                        uint32_t /*channelNumber*/,
                        WifiHelper& wifi,
                        double maxRangeMeters)
{
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

  channel.AddPropagationLoss("ns3::RangePropagationLossModel",
                             "MaxRange", DoubleValue(maxRangeMeters));

  Ptr<YansWifiChannel> ch = channel.Create();

  YansWifiPhyHelper phy;
  phy.SetChannel(ch);
  phy.Set("TxPowerStart", DoubleValue(txPowerDbm));
  phy.Set("TxPowerEnd", DoubleValue(txPowerDbm));

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

static Ptr<Node>
ResolveTargetNode(const std::string& failTarget,
                  Ptr<Node> N0, Ptr<Node> N1, Ptr<Node> N2, Ptr<Node> N3,
                  Ptr<Node> N4, Ptr<Node> N5, Ptr<Node> N6, Ptr<Node> N9, Ptr<Node> N10,
                  Ptr<Node> STA1, Ptr<Node> STA2,
                  const std::vector<Ptr<Node>>& upperAps,
                  const std::vector<Ptr<Node>>& lowerGroupA,
                  const std::vector<Ptr<Node>>& lowerGroupB)
{
  if (failTarget == "N0") return N0;
  if (failTarget == "N1") return N1;
  if (failTarget == "N2") return N2;
  if (failTarget == "N3") return N3;
  if (failTarget == "N4") return N4;
  if (failTarget == "N5") return N5;
  if (failTarget == "N6") return N6;
  if (failTarget == "N9") return N9;
  if (failTarget == "N10") return N10;
  if (failTarget == "STA1") return STA1;
  if (failTarget == "STA2") return STA2;

  auto ParseIndex = [&](const std::string& s, size_t pos) -> int {
    if (pos >= s.size()) return -1;
    for (size_t i = pos; i < s.size(); ++i)
    {
      if (!std::isdigit(static_cast<unsigned char>(s[i])))
      {
        return -1;
      }
    }
    return std::stoi(s.substr(pos));
  };

  if (failTarget.size() >= 2 && failTarget[0] == 'U')
  {
    int idx = ParseIndex(failTarget, 1);
    if (idx >= 0 && static_cast<size_t>(idx) < upperAps.size()) return upperAps[idx];
  }
  if (failTarget.size() >= 2 && failTarget[0] == 'A')
  {
    int idx = ParseIndex(failTarget, 1);
    if (idx >= 0 && static_cast<size_t>(idx) < lowerGroupA.size()) return lowerGroupA[idx];
  }
  if (failTarget.size() >= 2 && failTarget[0] == 'B')
  {
    int idx = ParseIndex(failTarget, 1);
    if (idx >= 0 && static_cast<size_t>(idx) < lowerGroupB.size()) return lowerGroupB[idx];
  }

  bool allDigits = !failTarget.empty();
  for (char ch : failTarget)
  {
    if (!std::isdigit(static_cast<unsigned char>(ch)))
    {
      allDigits = false;
      break;
    }
  }
  if (allDigits)
  {
    uint32_t nodeId = static_cast<uint32_t>(std::stoul(failTarget));
    if (nodeId < NodeList::GetNNodes())
    {
      return NodeList::GetNode(nodeId);
    }
  }

  return nullptr;
}

int
main(int argc, char* argv[])
{
  uint32_t n = 4;
  uint32_t x = 3;
  uint32_t y = 3;

  double hopDistance = 30.0;
  double sta1Distance = 60.0;
  double sta2Distance = 60.0;

  std::string p2pRate = "50Mbps";
  std::string lanRate = "100Mbps";

  double simTimeSec = 30.0;

  uint32_t upperAccessChannel = 6;
  uint32_t upperMeshChannel   = 1;
  uint32_t lowerAccessChannel = 11;
  uint32_t lowerMeshChannel   = 3;

  // failure knobs
  bool enableFail = false;
  std::string failTarget = "N10";
  double failTimeSec = 20.0;
  double recoverTimeSec = 40.0;
  int32_t failIfIndex = -1;

  // metrics knobs
  std::string latencyJsonPath = "sta1_sta2_latency.json";
  std::string lossJsonPath = "sta1_sta2_loss.json";
  double trafficIntervalMs = 200.0;
  uint32_t trafficPacketSize = 200;

  // upper-line geometry + range knobs
  double upperHopMeters = 5000.0;        // 5km spacing
  double upperMaxRangeMeters = 6000.0;   // > upperHopMeters

  // (NEW) STA2 mobility knobs
  bool enableSta2PingPong = true;
  double sta2SpeedKmph = 80.0;           // 80 km/h
  double sta2Z = 0.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("n", "Upper equivalent AP count for STA2 to choose", n);
  cmd.AddValue("x", "Lower group A count (mesh-only)", x);
  cmd.AddValue("y", "Lower group B count (mesh + AP for STA1)", y);
  cmd.AddValue("hopDistance", "Distance between adjacent equivalent nodes (meters) (lower groups)", hopDistance);
  cmd.AddValue("sta1Distance", "Offset distance for STA1 placement (meters)", sta1Distance);
  cmd.AddValue("sta2Distance", "Offset distance for STA2 placement (meters)", sta2Distance);
  cmd.AddValue("simTimeSec", "Simulation time in seconds", simTimeSec);

  cmd.AddValue("upperAccessChannel", "Upper ESS channel (compat only)", upperAccessChannel);
  cmd.AddValue("upperMeshChannel", "Upper mesh channel (compat only)", upperMeshChannel);
  cmd.AddValue("lowerAccessChannel", "Lower ESS channel (compat only)", lowerAccessChannel);
  cmd.AddValue("lowerMeshChannel", "Lower mesh channel (compat only)", lowerMeshChannel);

  cmd.AddValue("enableFail", "Enable scheduled soft failure (true/false)", enableFail);
  cmd.AddValue("failTarget", "Failure target: N10/N3/STA1/STA2/U0/A0/B0 or NodeId digits", failTarget);
  cmd.AddValue("failTimeSec", "Time to bring target DOWN (sec)", failTimeSec);
  cmd.AddValue("recoverTimeSec", "Time to bring target UP (sec)", recoverTimeSec);
  cmd.AddValue("failIfIndex", "Interface index to toggle; -1 means all (except loopback)", failIfIndex);

  cmd.AddValue("latencyJson", "Output JSON file for per-second STA1->STA2 avg one-way latency (seconds)", latencyJsonPath);
  cmd.AddValue("lossJson", "Output JSON file for per-second STA1->STA2 loss rate [0,1]", lossJsonPath);
  cmd.AddValue("trafficIntervalMs", "STA1->STA2 send interval in ms", trafficIntervalMs);
  cmd.AddValue("trafficPacketSize", "STA1->STA2 packet size in bytes", trafficPacketSize);

  cmd.AddValue("upperHopMeters", "Upper line spacing between N10->U0 and Ui->U(i+1) (meters)", upperHopMeters);
  cmd.AddValue("upperMaxRangeMeters", "Upper wifi RangePropagationLossModel MaxRange (meters)", upperMaxRangeMeters);

  // (NEW)
  cmd.AddValue("enableSta2PingPong", "Enable STA2 ping-pong movement (true/false)", enableSta2PingPong);
  cmd.AddValue("sta2SpeedKmph", "STA2 speed in km/h (ping-pong)", sta2SpeedKmph);

  cmd.Parse(argc, argv);

  // AntNet defaults
  Config::SetDefault("ns3::Ipv4AntNetRouting::ForwardAntInterval", TimeValue(Seconds(5)));
  Config::SetDefault("ns3::Ipv4AntNetRouting::UseBeaconWindow", BooleanValue(true));
  Config::SetDefault("ns3::Ipv4AntNetRouting::UseFailureMessagePropagation", BooleanValue(false));

  // Create nodes
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

  std::vector<Ptr<Node>> upperAps;
  upperAps.reserve(n);
  for (uint32_t i = 0; i < n; ++i)
  {
    upperAps.push_back(CreateObject<Node>());
  }

  std::vector<Ptr<Node>> lowerGroupA;
  lowerGroupA.reserve(x);
  for (uint32_t i = 0; i < x; ++i)
  {
    lowerGroupA.push_back(CreateObject<Node>());
  }

  std::vector<Ptr<Node>> lowerGroupB;
  lowerGroupB.reserve(y);
  for (uint32_t i = 0; i < y; ++i)
  {
    lowerGroupB.push_back(CreateObject<Node>());
  }

  NodeContainer all;
  all.Add(N0); all.Add(N1); all.Add(N2); all.Add(N3); all.Add(N4);
  all.Add(N5); all.Add(N6); all.Add(N9); all.Add(N10);
  all.Add(H0); all.Add(H1); all.Add(H2);
  for (auto& ap : upperAps) all.Add(ap);
  for (auto& a : lowerGroupA) all.Add(a);
  for (auto& b : lowerGroupB) all.Add(b);
  all.Add(STA1); all.Add(STA2);

  // Internet + AntNet routing
  InternetStackHelper internet;
  internet.SetRoutingHelper(Ipv4AntNetRoutingHelper());
  internet.Install(all);

  // Wired links
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
  lanNodes.Add(N0); lanNodes.Add(N4); lanNodes.Add(N5); lanNodes.Add(N6);
  lanNodes.Add(H0); lanNodes.Add(H1); lanNodes.Add(H2);
  NetDeviceContainer dLan = csma.Install(lanNodes);

  // Addressing
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

  // WiFi config
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("ErpOfdmRate24Mbps"),
                               "ControlMode", StringValue("ErpOfdmRate6Mbps"));
  double txPowerDbm = 16.0;

  // Upper: ESS + Mesh with max range for 5km
  EssWifiDevices upperEss =
    InstallEssMultiApOneSta(upperAps, STA2, "STA2-ESS", txPowerDbm,
                            upperAccessChannel, wifi,
                            upperMaxRangeMeters);

  NodeContainer upperMeshNodes;
  upperMeshNodes.Add(N10);
  for (auto& ap : upperAps) upperMeshNodes.Add(ap);

  YansWifiChannelHelper upperMeshCh = YansWifiChannelHelper::Default();
  upperMeshCh.AddPropagationLoss("ns3::RangePropagationLossModel",
                                 "MaxRange", DoubleValue(upperMaxRangeMeters));

  YansWifiPhyHelper upperMeshPhy;
  upperMeshPhy.SetChannel(upperMeshCh.Create());
  upperMeshPhy.Set("TxPowerStart", DoubleValue(txPowerDbm));
  upperMeshPhy.Set("TxPowerEnd", DoubleValue(txPowerDbm));

  MeshHelper upperMesh;
  upperMesh.SetStackInstaller("ns3::Dot11sStack");
  upperMesh.SetSpreadInterfaceChannels(MeshHelper::ZERO_CHANNEL);
  upperMesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
  upperMesh.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue("OfdmRate24Mbps"),
                                    "ControlMode", StringValue("OfdmRate6Mbps"));

  NetDeviceContainer upperMeshDevs = upperMesh.Install(upperMeshPhy, upperMeshNodes);

  {
    ipv4.SetBase("10.20.0.0", mask.c_str());
    NetDeviceContainer accessDevs;
    accessDevs.Add(upperEss.staDev);
    for (uint32_t i = 0; i < n; ++i) accessDevs.Add(upperEss.apDevs[i].Get(0));
    ipv4.Assign(accessDevs);

    ipv4.SetBase("10.21.0.0", mask.c_str());
    ipv4.Assign(upperMeshDevs);
  }

  // Lower: Mesh + ESS (short range; keep simple)
  {
    double lowerMaxRangeMeters = 1000.0;

    EssWifiDevices lowerEss =
      InstallEssMultiApOneSta(lowerGroupB, STA1, "STA1-ESS", txPowerDbm,
                              lowerAccessChannel, wifi,
                              lowerMaxRangeMeters);

    NodeContainer lowerMeshNodes;
    lowerMeshNodes.Add(N3);
    for (auto& a : lowerGroupA) lowerMeshNodes.Add(a);
    for (auto& b : lowerGroupB) lowerMeshNodes.Add(b);

    YansWifiChannelHelper lowerMeshCh = YansWifiChannelHelper::Default();
    lowerMeshCh.AddPropagationLoss("ns3::RangePropagationLossModel",
                                   "MaxRange", DoubleValue(lowerMaxRangeMeters));

    YansWifiPhyHelper lowerMeshPhy;
    lowerMeshPhy.SetChannel(lowerMeshCh.Create());
    lowerMeshPhy.Set("TxPowerStart", DoubleValue(txPowerDbm));
    lowerMeshPhy.Set("TxPowerEnd", DoubleValue(txPowerDbm));

    MeshHelper lowerMesh;
    lowerMesh.SetStackInstaller("ns3::Dot11sStack");
    lowerMesh.SetSpreadInterfaceChannels(MeshHelper::ZERO_CHANNEL);
    lowerMesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    lowerMesh.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue("OfdmRate24Mbps"),
                                      "ControlMode", StringValue("OfdmRate6Mbps"));

    NetDeviceContainer lowerMeshDevs = lowerMesh.Install(lowerMeshPhy, lowerMeshNodes);

    {
      ipv4.SetBase("10.30.0.0", mask.c_str());
      NetDeviceContainer accessDevs;
      accessDevs.Add(lowerEss.staDev);
      for (uint32_t i = 0; i < y; ++i) accessDevs.Add(lowerEss.apDevs[i].Get(0));
      ipv4.Assign(accessDevs);

      ipv4.SetBase("10.31.0.0", mask.c_str());
      ipv4.Assign(lowerMeshDevs);
    }
  }

  // ─────────────────────────────────────────────────────────────
  // Mobility / placement
  // ─────────────────────────────────────────────────────────────
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
  auto AddPos = [&](Ptr<Node> node, double x_, double y_, double z = 0.0) {
    pos->Add(Vector(x_, y_, z));
  };

  // Base wired placement
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

  // Upper right line: N10 + U0..U(n-1) spacing = upperHopMeters
  double n10x = 180.0;
  double n10y = 200.0;
  AddPos(N10, n10x, n10y);

  for (uint32_t i = 0; i < n; ++i)
  {
    AddPos(upperAps[i], n10x + (i + 1) * upperHopMeters, n10y);
  }

  // (CHANGED) STA2 initial position will be set by ConstantVelocityMobilityModel below,
  // so we DO NOT AddPos(STA2, ...) here.

  // Lower groups keep dense layout
  double ax0 = 240.0, ay0 = 60.0;
  for (uint32_t i = 0; i < x; ++i) AddPos(lowerGroupA[i], ax0 + i * hopDistance, ay0);

  double bx0 = 240.0, by0 = 30.0;
  for (uint32_t i = 0; i < y; ++i) AddPos(lowerGroupB[i], bx0 + i * hopDistance, by0);

  double sta1x = (y > 0) ? (bx0 + (y - 1) * hopDistance) : bx0;
  AddPos(STA1, sta1x + sta1Distance, by0);

  // (NEW) Install mobility:
  // - All nodes except STA2: ConstantPosition
  // - STA2: ConstantVelocity (ping-pong)
  MobilityHelper mobilityStatic;
  mobilityStatic.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityStatic.SetPositionAllocator(pos);

  NodeContainer staticNodes;
  staticNodes.Add(N0); staticNodes.Add(N1); staticNodes.Add(N2); staticNodes.Add(N3);
  staticNodes.Add(N4); staticNodes.Add(N5); staticNodes.Add(N6); staticNodes.Add(N9);
  staticNodes.Add(N10);
  staticNodes.Add(H0); staticNodes.Add(H1); staticNodes.Add(H2);
  for (auto& ap : upperAps) staticNodes.Add(ap);
  for (auto& a : lowerGroupA) staticNodes.Add(a);
  for (auto& b : lowerGroupB) staticNodes.Add(b);
  staticNodes.Add(STA1);

  mobilityStatic.Install(staticNodes);

  MobilityHelper mobilitySta2;
  mobilitySta2.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobilitySta2.Install(STA2);

  // Set STA2 initial pos and velocity for ping-pong between U0 and U(n-1)
  Ptr<ConstantVelocityMobilityModel> sta2Mob = STA2->GetObject<ConstantVelocityMobilityModel>();
  if (!sta2Mob)
  {
    NS_LOG_UNCOND("[STA2-MOB] Failed to get ConstantVelocityMobilityModel on STA2.");
  }
  else
  {
    // Define the travel corridor: from leftmost AP (U0) to rightmost AP (U(n-1)) on x-axis,
    // keep STA2 at y = n10y + sta2Distance (above the AP line).
    double xLeft = (n > 0) ? (n10x + 1 * upperHopMeters) : (n10x + 1 * upperHopMeters);
    double xRight = (n >= 2) ? (n10x + n * upperHopMeters) : xLeft; // if n=1 -> no movement

    double ySta2 = n10y + sta2Distance;

    // Start above the LEFTMOST AP
    sta2Mob->SetPosition(Vector(xLeft, ySta2, sta2Z));

    double speedMps = KmphToMps(sta2SpeedKmph);

    if (!enableSta2PingPong || n < 2 || speedMps <= 1e-9 || xRight <= xLeft + 1e-9)
    {
      sta2Mob->SetVelocity(Vector(0.0, 0.0, 0.0));
      NS_LOG_UNCOND("[STA2-MOB] Ping-pong disabled or not enough upper APs; STA2 stays still.");
    }
    else
    {
      sta2Mob->SetVelocity(Vector(std::abs(speedMps), 0.0, 0.0));

      double distance = xRight - xLeft;
      double oneWay = distance / std::abs(speedMps);

      NS_LOG_UNCOND("[STA2-MOB] STA2 ping-pong enabled: "
                    << "xLeft=" << xLeft << ", xRight=" << xRight
                    << ", distance=" << distance << "m"
                    << ", speed=" << speedMps << "m/s"
                    << ", oneWay=" << oneWay << "s");

      // First toggle at one-way time, then repeat
      Simulator::Schedule(Seconds(oneWay),
                          &Sta2ToggleVelocityAndReschedule,
                          sta2Mob, std::abs(speedMps), oneWay, simTimeSec);
    }
  }

  // Enable forwarding on router-ish nodes
  {
    std::vector<Ptr<Node>> routers;
    routers.push_back(N10);
    routers.push_back(N3);
    routers.push_back(N2);
    routers.push_back(N9);
    routers.push_back(N6);
    for (auto& ap : upperAps) routers.push_back(ap);
    for (auto& b : lowerGroupB) routers.push_back(b);
    EnableForwardingOnNodes(routers);
  }

  // AntNet init
  Ipv4AntNetRoutingHelper::BuildAntNetTopology();
  Ipv4AntNetRoutingHelper::InitializeNodeRoutingTables();

  // failure schedule
  if (enableFail)
  {
    Ptr<Node> target = ResolveTargetNode(failTarget,
                                         N0, N1, N2, N3, N4, N5, N6, N9, N10,
                                         STA1, STA2,
                                         upperAps, lowerGroupA, lowerGroupB);

    if (!target)
    {
      NS_LOG_UNCOND("[SOFT-FAIL] failTarget=\"" << failTarget << "\" not found. Skip failure schedule.");
    }
    else
    {
      Simulator::Schedule(Seconds(failTimeSec), &SoftBringNodeDown, target, failIfIndex);
      Simulator::Schedule(Seconds(recoverTimeSec), &SoftBringNodeUp, target, failIfIndex);
    }
  }

  // Metrics + traffic (STA1 -> STA2)
  PerSecondStats stats;
  stats.Init(simTimeSec);

  auto GetFirstNonLoopback = [](Ptr<Node> node) -> Ipv4Address {
    Ptr<Ipv4> ipv4_ = node->GetObject<Ipv4>();
    for (uint32_t i = 0; i < ipv4_->GetNInterfaces(); ++i)
    {
      for (uint32_t j = 0; j < ipv4_->GetNAddresses(i); ++j)
      {
        Ipv4InterfaceAddress ifAddr = ipv4_->GetAddress(i, j);
        if (ifAddr.GetLocal() != Ipv4Address("127.0.0.1"))
        {
          return ifAddr.GetLocal();
        }
      }
    }
    return Ipv4Address("0.0.0.0");
  };

  Ipv4Address sta2Addr = GetFirstNonLoopback(STA2);
  if (sta2Addr == Ipv4Address("0.0.0.0"))
  {
    NS_LOG_UNCOND("[METRICS] STA2 has no non-loopback IP, cannot send traffic.");
  }

  uint16_t port = 9000;
  Time interval = MilliSeconds(trafficIntervalMs);

  Ptr<UdpTsServerApp> serverApp = CreateObject<UdpTsServerApp>();
  serverApp->Setup(port, &stats);
  STA2->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(1.0));
  serverApp->SetStopTime(Seconds(simTimeSec - 1));

  Ptr<UdpTsClientApp> clientApp = CreateObject<UdpTsClientApp>();
  clientApp->Setup(sta2Addr, port, interval, trafficPacketSize, &stats);
  STA1->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(2.0));
  clientApp->SetStopTime(Seconds(simTimeSec - 1));

  Simulator::Stop(Seconds(simTimeSec));
  Simulator::Run();
  Simulator::Destroy();

  // Dump JSON arrays
  {
    std::vector<double> latency = stats.BuildLatencyAvg();
    std::vector<double> loss = stats.BuildLossRate();

    PerSecondStats::WriteJsonDoubleArray(latencyJsonPath, latency);
    PerSecondStats::WriteJsonDoubleArray(lossJsonPath, loss);

    NS_LOG_UNCOND("[METRICS] Wrote latency JSON: " << latencyJsonPath);
    NS_LOG_UNCOND("[METRICS] Wrote loss JSON:    " << lossJsonPath);
  }

  return 0;
}
