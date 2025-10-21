// src/antnet/examples/antnet-csma-mesh.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/antnet-helper.h"
#include "ns3/antnet-routing-protocol.h"   // <= needed for AddStaticNeighbor

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AntNetCsmaMesh");

// Helper to index routers on an RxC grid
static inline uint32_t Idx(uint32_t r, uint32_t c, uint32_t cols) { return r * cols + c; }

// Helper to set next subnet base: 10.0.N.0/24
static void SetNextSubnet(Ipv4AddressHelper &addr, uint32_t &n) {
  std::ostringstream base;
  base << "10.0." << n++ << ".0";
  addr.SetBase(Ipv4Address(base.str().c_str()), "255.255.255.0");
}

int main(int argc, char *argv[])
{
  // Logging (useful if anything goes wrong)
  LogComponentEnable("AntNetRoutingProtocol", LOG_LEVEL_INFO);
  LogComponentEnable("PheromoneTable", LOG_LEVEL_DEBUG);
  LogComponentEnableAll(LOG_PREFIX_TIME);
  LogComponentEnableAll(LOG_PREFIX_NODE);
  LogComponentEnableAll(LOG_PREFIX_LEVEL);

  double simTime = 90.0;
  bool enablePcap = false;
  uint32_t rows = 3, cols = 3;
  double fastDelayUs = 2000;  // 2 ms
  double slowDelayUs = 8000;  // 8 ms
  std::string dataRate = "100Mbps";

  CommandLine cmd;
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.AddValue("enablePcap", "Enable CSMA PCAP tracing", enablePcap);
  cmd.AddValue("rows", "Grid rows", rows);
  cmd.AddValue("cols", "Grid cols", cols);
  cmd.AddValue("fastDelayUs", "Fast link delay (us)", fastDelayUs);
  cmd.AddValue("slowDelayUs", "Slow link delay (us)", slowDelayUs);
  cmd.Parse(argc, argv);

  // --- 1) Create nodes: RxC routers + two end hosts Hs/Hd
  NodeContainer routers; routers.Create(rows * cols);
  Ptr<Node> Hs = CreateObject<Node>();
  Ptr<Node> Hd = CreateObject<Node>();

  // --- 2) Install Internet + AntNet early (before address assignment)
  InternetStackHelper stack;
  Ipv4ListRoutingHelper list;
  AntNetHelper antnet;
  // Optional AntNet tuning:
  // antnet.Set("AntPeriod", TimeValue(Seconds(0.5)));
  // antnet.Set("BetaData", DoubleValue(1.6));
  list.Add(antnet, 10);
  stack.SetRoutingHelper(list);
  stack.Install(NodeContainer(routers, Hs, Hd));

  // --- 3) Prepare CSMA helpers
  CsmaHelper csmaFast, csmaSlow;
  csmaFast.SetChannelAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  csmaFast.SetChannelAttribute("Delay",   TimeValue(MicroSeconds(fastDelayUs)));
  csmaSlow.SetChannelAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  csmaSlow.SetChannelAttribute("Delay",   TimeValue(MicroSeconds(slowDelayUs)));

  auto isSlowEdge = [rows, cols](uint32_t r, uint32_t c, bool horizontal) {
    if (rows >= 3 && cols >= 3) {
      if (horizontal) {                // (r,c) -- (r,c+1)
        return (r == 1 && c == 1);     // central horizontal link is slow
      } else {                         // (r,c) -- (r+1,c)
        return false;
      }
    }
    return false;
  };

  // --- 4) Wire up the grid with per-edge subnets and assign IPs
  Ipv4AddressHelper addr;
  uint32_t subnetId = 0;

  // (We’ll need these to fetch host-side IPs and to identify host LAN channels)
  Ipv4InterfaceContainer ifsHs, ifsHd;
  NetDeviceContainer hsLanDevs, hdLanDevs;

  // Horizontal edges (router <-> router)
  for (uint32_t r = 0; r < rows; ++r) {
    for (uint32_t c = 0; c + 1 < cols; ++c) {
      NodeContainer pair(routers.Get(Idx(r, c, cols)), routers.Get(Idx(r, c + 1, cols)));
      NetDeviceContainer devs = (isSlowEdge(r, c, true) ? csmaSlow : csmaFast).Install(pair);
      SetNextSubnet(addr, subnetId);
      addr.Assign(devs);
    }
  }

  // Vertical edges (router <-> router)
  for (uint32_t r = 0; r + 1 < rows; ++r) {
    for (uint32_t c = 0; c < cols; ++c) {
      NodeContainer pair(routers.Get(Idx(r, c, cols)), routers.Get(Idx(r + 1, c, cols)));
      NetDeviceContainer devs = (isSlowEdge(r, c, false) ? csmaSlow : csmaFast).Install(pair);
      SetNextSubnet(addr, subnetId);
      addr.Assign(devs);
    }
  }

  // --- 5) Attach hosts to corner routers (each on its own /24)
  // Hs <-> R(0,0)
  {
    NodeContainer lan(Hs, routers.Get(Idx(0, 0, cols)));
    hsLanDevs = csmaFast.Install(lan);           // save to identify host LAN channel
    SetNextSubnet(addr, subnetId);
    ifsHs = addr.Assign(hsLanDevs);              // index 0 = Hs, index 1 = router
  }
  // Hd <-> R(rows-1, cols-1)
  {
    NodeContainer lan(Hd, routers.Get(Idx(rows - 1, cols - 1, cols)));
    hdLanDevs = csmaFast.Install(lan);           // save to identify host LAN channel
    SetNextSubnet(addr, subnetId);
    ifsHd = addr.Assign(hdLanDevs);              // index 0 = Hd, index 1 = router
  }

  // --- 6) Register static neighbors (inline, routers only; skip host LANs)
  {
    // Identify the two host LAN channels so we can skip them
    Ptr<Channel> hsLanCh = hsLanDevs.Get(0)->GetChannel();
    Ptr<Channel> hdLanCh = hdLanDevs.Get(0)->GetChannel();

    for (auto it = routers.Begin(); it != routers.End(); ++it) {  // routers only
      Ptr<Node> node = *it;
      Ptr<AntNetRoutingProtocol> antr = node->GetObject<AntNetRoutingProtocol>();
      if (!antr) continue;

      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
      if (!ipv4) continue;

      const uint32_t nIf = ipv4->GetNInterfaces();
      for (uint32_t i = 0; i < nIf; ++i) {
        Ptr<NetDevice> dev = ipv4->GetNetDevice(i);
        if (!dev) continue;

        Ptr<CsmaChannel> ch = DynamicCast<CsmaChannel>(dev->GetChannel());
        if (!ch) continue;

        // Skip Hs<->router and Hd<->router LANs
        if (ch == hsLanCh || ch == hdLanCh) continue;

        // For each other device on this inter-router CSMA segment…
        for (uint32_t d = 0; d < ch->GetNDevices(); ++d) {
          Ptr<NetDevice> otherDev = ch->GetDevice(d);
          if (otherDev == dev) continue;

          Ptr<Node> otherNode = otherDev->GetNode();
          // Only form static-neighbor edges to other routers
          if (!routers.Contains(otherNode->GetId())) continue;

          Ptr<Ipv4> oipv4 = otherNode->GetObject<Ipv4>();
          if (!oipv4) continue;

          int32_t otherIf = oipv4->GetInterfaceForDevice(otherDev);
          if (otherIf < 0) continue;

          Ipv4InterfaceAddress oifa = oipv4->GetAddress(static_cast<uint32_t>(otherIf), 0);
          Ipv4Address neighIp = oifa.GetLocal();
          if (neighIp == Ipv4Address::GetAny() || neighIp == Ipv4Address("0.0.0.0"))
            continue;

          antr->AddStaticNeighbor(neighIp);  // router ↔ router only
        }
      }
    }
  }
  // --- end static neighbor registration ---

  // --- 7) Application: UDP flow from Hs to Hd
  const Ipv4Address hdIp = ifsHd.GetAddress(0); // sink must bind to Hd’s IP
  const uint16_t port = 9000;

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(hdIp, port));
  ApplicationContainer sinkApp = sink.Install(Hd);
  sinkApp.Start(Seconds(0.4));
  sinkApp.Stop(Seconds(simTime));

  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(hdIp, port));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("12Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(400));
  onoff.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer src = onoff.Install(Hs);
  src.Start(Seconds(1.0));
  src.Stop(Seconds(simTime - 1));

  // Optional PCAP
  if (enablePcap) {
    csmaFast.EnablePcapAll("antnet-mesh", true);
    csmaSlow.EnablePcapAll("antnet-mesh-slow", true);
  }

  // --- 8) FlowMonitor for quick stats
  FlowMonitorHelper fmh;
  Ptr<FlowMonitor> fm = fmh.InstallAll();

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  fm->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> cl = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
  auto stats = fm->GetFlowStats();

  double aggThr = 0.0;
  for (const auto &kv : stats) {
    uint32_t flowId = kv.first;
    const auto &st = kv.second;
    Ipv4FlowClassifier::FiveTuple t = cl->FindFlow(flowId);
    if (t.destinationPort == port) {
      double duration = (st.timeLastRxPacket - st.timeFirstTxPacket).GetSeconds();
      double thrMbps = duration > 0 ? (st.rxBytes * 8.0 / duration / 1e6) : 0.0;
      aggThr += thrMbps;
      std::cout << "[FLOW] " << t.sourceAddress << " -> " << t.destinationAddress
                << " rx=" << st.rxBytes
                << " thr=" << thrMbps << " Mbps"
                << " delayAvg=" << (st.rxPackets ? st.delaySum.GetSeconds() / st.rxPackets : 0.0) << " s"
                << " loss=" << (st.txPackets - st.rxPackets) << "\n";
    }
  }
  std::cout << "[RESULT] Aggregate throughput ~ " << aggThr << " Mbps\n";

  Simulator::Destroy();
  return 0;
}
