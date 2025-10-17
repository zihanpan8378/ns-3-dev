// src/antnet/examples/antnet-csma-mesh.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/antnet-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AntNetCsmaMesh");

// Generate consecutive subnets: 10.1.N.0/24
static void SetNextSubnet(Ipv4AddressHelper &addr, uint32_t n)
{
  std::ostringstream base;
  base << "10.1." << n << ".0";
  addr.SetBase(base.str().c_str(), "255.255.255.0");
}

int main(int argc, char *argv[])
{
  LogComponentEnable("AntNetRoutingProtocol", LOG_LEVEL_INFO);
  LogComponentEnable("PheromoneTable", LOG_LEVEL_DEBUG);

  LogComponentEnableAll(LOG_PREFIX_TIME);
  LogComponentEnableAll(LOG_PREFIX_NODE);
  LogComponentEnableAll(LOG_PREFIX_LEVEL);

  double simTime = 90.0;      // Simulation duration
  bool enablePcap = false;    // Enable packet capture
  uint32_t rows = 3, cols = 3;
  double fastDelayUs = 2000;  // 2 ms for most links
  double slowDelayUs = 8000;  // 8 ms for the central "slow link" (for comparison)
  std::string dataRate = "100Mbps";

  CommandLine cmd;
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.AddValue("enablePcap", "Enable CSMA PCAP tracing", enablePcap);
  cmd.AddValue("rows", "Grid rows", rows);
  cmd.AddValue("cols", "Grid cols", cols);
  cmd.Parse(argc, argv);

  // --- 1) Create nodes: R = rows*cols routers + two end hosts Hs/Hd
  NodeContainer routers; routers.Create(rows * cols);
  Ptr<Node> Hs = CreateObject<Node>(); // Source host (attached to R(0,0))
  Ptr<Node> Hd = CreateObject<Node>(); // Destination host (attached to R(rows-1, cols-1))

  auto idx = [cols](uint32_t r, uint32_t c){ return r * cols + c; };

  // --- 2) Two CSMA types: fast & slow (slow only for one specific link)
  CsmaHelper csmaFast, csmaSlow;
  csmaFast.SetChannelAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  csmaFast.SetChannelAttribute("Delay",   TimeValue(MicroSeconds(fastDelayUs)));
  csmaSlow.SetChannelAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  csmaSlow.SetChannelAttribute("Delay",   TimeValue(MicroSeconds(slowDelayUs)));

  // Helper to assign IP addresses consistently
  Ipv4AddressHelper addr;
  uint32_t netId = 0;
  std::vector<Ipv4InterfaceContainer> allIfs;

  // --- 3) Wire up adjacent routers in the grid (one subnet per edge)
  // Slow down the "central horizontal edge"; e.g., rows=3, cols=3 uses slow link for (1,1)<->(1,2)
  auto isSlowEdge = [rows,cols](uint32_t r, uint32_t c, bool horizontal) {
    if (rows >= 3 && cols >= 3) {
      if (horizontal) { // (r,c) -- (r,c+1)
        return (r == 1 && c == 1); // Central horizontal link
      } else { // (r,c) -- (r+1,c)
        return false;
      }
    }
    return false;
  };

  // Horizontal edges
  for (uint32_t r=0; r<rows; ++r) {
    for (uint32_t c=0; c+1<cols; ++c) {
      NodeContainer pair(routers.Get(idx(r,c)), routers.Get(idx(r,c+1)));
      NetDeviceContainer devs = (isSlowEdge(r,c,true) ? csmaSlow : csmaFast).Install(pair);
      SetNextSubnet(addr, netId++);
      allIfs.push_back(addr.Assign(devs));
    }
  }
  // Vertical edges
  for (uint32_t r=0; r+1<rows; ++r) {
    for (uint32_t c=0; c<cols; ++c) {
      NodeContainer pair(routers.Get(idx(r,c)), routers.Get(idx(r+1,c)));
      NetDeviceContainer devs = (isSlowEdge(r,c,false) ? csmaSlow : csmaFast).Install(pair);
      SetNextSubnet(addr, netId++);
      allIfs.push_back(addr.Assign(devs));
    }
  }

  // --- 4) Attach each host to a router
  // Hs <-> R(0,0)
  {
    NodeContainer lan(Hs, routers.Get(idx(0,0)));
    NetDeviceContainer devs = csmaFast.Install(lan);
    SetNextSubnet(addr, netId++);
    allIfs.push_back(addr.Assign(devs));
  }
  // Hd <-> R(rows-1, cols-1)
  {
    NodeContainer lan(Hd, routers.Get(idx(rows-1, cols-1)));
    NetDeviceContainer devs = csmaFast.Install(lan);
    SetNextSubnet(addr, netId++);
    allIfs.push_back(addr.Assign(devs));
  }

  // --- 5) Network stack + AntNet routing
  InternetStackHelper stack;
  Ipv4ListRoutingHelper list;
  AntNetHelper antnet;
  // You may fine-tune parameters here, e.g.:
  // antnet.Set("AntPeriod", TimeValue(Seconds(0.5)));
  // antnet.Set("BetaData", DoubleValue(1.6));
  list.Add(antnet, 10);
  stack.SetRoutingHelper(list);
  stack.Install(NodeContainer(routers, Hs, Hd));

  // --- 6) Application: UDP flow from Hs to Hd
  // Retrieve Hd's host-side IP (the last interface container, index 1 for Hd)
  Ipv4Address hdIp = allIfs.back().GetAddress(1);
  uint16_t port = 9000;

  // Sink on Hd
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(hdIp, port));
  ApplicationContainer sinkApp = sink.Install(Hd);
  sinkApp.Start(Seconds(0.4));

  // OnOff on Hs
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(hdIp, port));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("12Mbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(400));
  onoff.SetAttribute("OnTime",  StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer src = onoff.Install(Hs);
  src.Start(Seconds(1.0));
  src.Stop(Seconds(simTime - 1));

  // Optional packet capture
  if (enablePcap) {
    csmaFast.EnablePcapAll("antnet-mesh", true);
    csmaSlow.EnablePcapAll("antnet-mesh-slow", true);
  }

  // --- 7) FlowMonitor: inspect throughput/latency roughly
  Ptr<FlowMonitor> fm;
  FlowMonitorHelper fmh;
  fm = fmh.InstallAll();

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  fm->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> cl = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = fm->GetFlowStats();
  double aggThr = 0.0;
  for (const auto &kv : stats) {
    auto flowId = kv.first;
    const auto &st = kv.second;
    Ipv4FlowClassifier::FiveTuple t = cl->FindFlow(flowId);
    if (t.destinationPort == port) {
      double duration = (st.timeLastRxPacket - st.timeFirstTxPacket).GetSeconds();
      double thrMbps = duration > 0 ? (st.rxBytes * 8.0 / duration / 1e6) : 0.0;
      aggThr += thrMbps;
      std::cout << "[FLOW] " << t.sourceAddress << " -> " << t.destinationAddress
                << " rx=" << st.rxBytes
                << " thr=" << thrMbps << " Mbps"
                << " delayAvg=" << (st.delaySum.GetSeconds()/std::max<uint64_t>(1, st.rxPackets)) << " s"
                << " loss=" << (st.txPackets - st.rxPackets) << "\n";
    }
  }
  std::cout << "[RESULT] Aggregate throughput ~ " << aggThr << " Mbps\n";

  Simulator::Destroy();
  return 0;
}
