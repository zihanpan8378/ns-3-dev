#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

#include "ns3/ipv4-antnet-routing-helper.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream> // (NEW)
#include <vector>  // (NEW)

using namespace ns3;

                                   LAN 10.8.0/24
                        =====================================
                        |                 |        |        |
                        |                 |        |        |     
                    [R0: Node0]          N4       N5       N6
            10.1.1.1/24 |                                   | 10.1.6.1/24
                        |                                   |
                        |                                   |
                       C1                                  C6
                        |                                   |
                        |                                   |
            10.1.1.2/24 |      10.1.5.1/24      10.1.5.2/24 | 10.1.6.2/24
                    [R2: Node2]----------C5----------[R9: Node9]
          10.1.2.2/24 /   \ 10.1.3.2/24               / 10.1.7.2/24
                     /     \                         /
                    /       \                       /
                   C2        C3                    /  
                  /           \                   C7      
                 /             \                 /      
    10.1.2.1/24 /               \ 10.1.3.1/24   /       
    [R1: Node1]------C4----------[R3: Node3]----/                 
        10.1.4.1/24      10.1.4.2/24  
                                   *
                                   *
                            N7 (Node7, Dual-AP) ******* Nx (Node7, Dual-AP)
                              *                                 *
                             ***                          ***
                           *                       *
                         *                   *
                        ***            ***
                        *         *
                    N8 (Node8 STA)


NS_LOG_COMPONENT_DEFINE("SimpleAntNetWifiExample");

// ============================================================
// Soft failure: bring node interfaces DOWN (no crash)
// ============================================================
[[maybe_unused]] static void
SoftBringNodeDown(Ptr<Node> node)
{
    NS_LOG_UNCOND("[SOFT-FAIL] Node " << node->GetId()
                                      << " DOWN @ " << Simulator::Now().GetSeconds() << "s");

    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    if (!ipv4)
    {
        NS_LOG_UNCOND("Node " << node->GetId() << " has no Ipv4!");
        return;
    }

    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i)
    {
        if (i == 0) // loopback
            continue;
        ipv4->SetDown(i);
    }

    for (uint32_t i = 0; i < node->GetNApplications(); ++i)
    {
        Ptr<Application> app = node->GetApplication(i);
        if (app)
        {
            app->SetStopTime(Simulator::Now());
        }
    }
}

// (NEW) helper: build "10.5.X.0" string
static std::string
MakeSubnet(uint32_t thirdOctet)
{
    std::ostringstream oss;
    oss << "10.5." << thirdOctet << ".0";
    return oss.str();
}

int
main(int argc, char* argv[])
{
    std::srand(538);

    LogComponentEnable("SimpleAntNetWifiExample", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4AntNetRoutingHelper", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4AntNetRouting", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4AntNetRoutingTableEntry", LOG_LEVEL_INFO);
    LogComponentEnable("AntHeader", LOG_LEVEL_INFO);

    // ============================================================
    // Command line knobs
    // ============================================================
    bool enableFlowMonitor = false;
    bool enableNodeFail = false;
    double failTimeSec = 25.0;
    uint32_t failNodeId = 7;
    double simTimeSec = 50.0;
    double intervalSec = 10.0;

    double appStartSec = 2.0;
    double appStopSec = 45.0;

    // (NEW) dual-AP count including original Node7
    uint32_t numDualAp = 1;

    // (NEW) traffic knobs for UDP echo
    uint32_t maxPackets = 30000;
    double udpIntervalSec = 0.003;
    uint32_t packetSize = 210;

    CommandLine cmd(__FILE__);
    cmd.AddValue("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.AddValue("EnableNodeFail", "Enable node soft failure event", enableNodeFail);
    cmd.AddValue("FailTime", "Failure time (seconds)", failTimeSec);
    cmd.AddValue("FailNodeId", "Node id to fail (default 7)", failNodeId);
    cmd.AddValue("SimTime", "Simulation stop time (seconds)", simTimeSec);
    cmd.AddValue("IntervalSec", "AntNet ForwardAntInterval (seconds)", intervalSec);
    cmd.AddValue("AppStart", "UDP traffic start time (seconds)", appStartSec);
    cmd.AddValue("AppStop", "UDP traffic stop time (seconds)", appStopSec);

    // (NEW)
    cmd.AddValue("NumDualAp", "Number of dual-AP nodes like Node7 (including Node7).", numDualAp);

    // (NEW) expose UDP knobs (你之前 help 里就有 maxPackets/interval/port 的影子，这里补齐)
    cmd.AddValue("maxPackets", "UDP client max packets", maxPackets);
    cmd.AddValue("intervalSec", "UDP client interval seconds", udpIntervalSec);
    cmd.AddValue("packetSize", "UDP packet size", packetSize);

    cmd.Parse(argc, argv);

    // Defaults (apply parsed IntervalSec)
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(packetSize));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("448kb/s"));
    Config::SetDefault("ns3::Ipv4AntNetRouting::ForwardAntInterval", TimeValue(Seconds(intervalSec)));
    Config::SetDefault("ns3::Ipv4AntNetRouting::UseBeaconWindow", BooleanValue(true));
    Config::SetDefault("ns3::Ipv4AntNetRouting::UseFailureMessagePropagation", BooleanValue(false));

    // ============================================================
    // Create nodes
    // ============================================================
    NS_LOG_INFO("Create nodes.");

    NodeContainer c;

    // (CHANGED) 原来固定 10；现在为了新增 dual-AP + STA 客户端扩展
    // base: 10 nodes (0..9)
    // 每新增 1 个 dual-AP（除了 Node7 本身）需要额外 2 个节点：AP + STA(client)
    uint32_t extraApCount = (numDualAp > 1) ? (numDualAp - 1) : 0;
    uint32_t totalNodes = 10 + extraApCount * 2;

    c.Create(totalNodes);

    // 保持原来的容器/连线（p2p/lan 不动）
    NodeContainer n0n2 = NodeContainer(c.Get(0), c.Get(2));
    NodeContainer n1n2 = NodeContainer(c.Get(1), c.Get(2));
    NodeContainer n3n2 = NodeContainer(c.Get(3), c.Get(2));
    NodeContainer n1n3 = NodeContainer(c.Get(1), c.Get(3));
    NodeContainer n2n9 = NodeContainer(c.Get(2), c.Get(9));
    NodeContainer n6n9 = NodeContainer(c.Get(6), c.Get(9));
    NodeContainer n3n9 = NodeContainer(c.Get(3), c.Get(9));
    NodeContainer lanNodes = NodeContainer(c.Get(0), c.Get(4), c.Get(5), c.Get(6));

    Ptr<Node> node8 = c.Get(8); // (NEW) Node8 作为所有 WiFi#2 的“多网卡 STA”目的地

    // ============================================================
    // (CHANGED) dual-AP 列表 + WiFi#1 客户端 STA 列表
    //   i=0: AP_0 = Node7, STA_0 = Node3 (保持你原来的)
    //   i>=1: AP_i = 新增节点, STA_i = 新增节点
    // ============================================================
    std::vector<Ptr<Node>> dualApNodes;
    std::vector<Ptr<Node>> wifi1StaClients; // all clients, each should reach Node8

    // i=0
    dualApNodes.push_back(c.Get(7));
    wifi1StaClients.push_back(c.Get(3));

    // i>=1 extras: [AP_i, STA_i]
    for (uint32_t i = 1; i < numDualAp; ++i)
    {
        uint32_t base = 10 + (i - 1) * 2;
        Ptr<Node> ap = c.Get(base);
        Ptr<Node> sta = c.Get(base + 1);

        dualApNodes.push_back(ap);
        wifi1StaClients.push_back(sta);
    }

    // ============================================================
    // Internet + AntNet routing
    // ============================================================
    InternetStackHelper internet;
    internet.SetRoutingHelper(Ipv4AntNetRoutingHelper());
    internet.Install(c);

    // ============================================================
    // P2P channels (原样保持)
    // ============================================================
    NS_LOG_INFO("Create channels.");
    PointToPointHelper p2p;

    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer d6d9 = p2p.Install(n6n9);
    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    p2p.SetDeviceAttribute("DataRate", StringValue("1500kbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer d3d2 = p2p.Install(n3n2);
    NetDeviceContainer d3d9 = p2p.Install(n3n9);

    p2p.SetDeviceAttribute("DataRate", StringValue("3Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));
    NetDeviceContainer d2d9 = p2p.Install(n2n9);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d1d3 = p2p.Install(n1n3);

    // ============================================================
    // LAN (CSMA) 原样保持
    // ============================================================
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("0.1ms"));
    NetDeviceContainer lanDevices = csma.Install(lanNodes);

    // ============================================================
    // Assign IPs for original non-wifi networks (原样保持)
    // ============================================================
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(d0d2);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ipv4.Assign(d3d2);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    ipv4.Assign(d1d3);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    ipv4.Assign(d2d9);

    ipv4.SetBase("10.1.6.0", "255.255.255.0");
    ipv4.Assign(d6d9);

    ipv4.SetBase("10.1.7.0", "255.255.255.0");
    ipv4.Assign(d3d9);

    ipv4.SetBase("10.1.8.0", "255.255.255.0");
    ipv4.Assign(lanDevices);

    // ============================================================
    // (CHANGED) WiFi: 每个 dual-AP i 建两条 WiFi:
    //   WiFi#1_i: AP_i <-> STA_i (client)
    //   WiFi#2_i: AP_i <-> Node8 (destination, multi-STA NIC)
    // ============================================================
    WifiHelper wifi;

    // (NEW) store Node8 per-wifi2 interface, used as server address
    std::vector<Ipv4Address> node8Wifi2Ips;

    for (uint32_t i = 0; i < numDualAp; ++i)
    {
        Ptr<Node> apNode = dualApNodes[i];
        Ptr<Node> clientSta = wifi1StaClients[i];

        // -------------------------
        // WiFi#1_i : AP_i <-> clientSta
        // -------------------------
        {
            YansWifiChannelHelper ch = YansWifiChannelHelper::Default();
            YansWifiPhyHelper phy;
            phy.SetChannel(ch.Create());

            WifiMacHelper mac;
            std::ostringstream ssidoss;
            ssidoss << "ssid-wifi1-" << i;
            Ssid ssid = Ssid(ssidoss.str());

            NodeContainer apC(apNode);
            NodeContainer staC(clientSta);

            mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
            NetDeviceContainer apDev = wifi.Install(phy, mac, apC);

            mac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "ActiveProbing", BooleanValue(false));
            NetDeviceContainer staDev = wifi.Install(phy, mac, staC);

            // subnet: 10.5.(1+2*i).0/24
            uint32_t oct = 1 + 2 * i;
            ipv4.SetBase(MakeSubnet(oct).c_str(), "255.255.255.0");
            ipv4.Assign(apDev);
            ipv4.Assign(staDev);
        }

        // -------------------------
        // WiFi#2_i : AP_i <-> Node8 (multi-STA on node8)
        // -------------------------
        {
            YansWifiChannelHelper ch = YansWifiChannelHelper::Default();
            YansWifiPhyHelper phy;
            phy.SetChannel(ch.Create());

            WifiMacHelper mac;
            std::ostringstream ssidoss;
            ssidoss << "ssid-wifi2-" << i;
            Ssid ssid = Ssid(ssidoss.str());

            NodeContainer apC(apNode);
            NodeContainer staC(node8);

            mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
            NetDeviceContainer apDev = wifi.Install(phy, mac, apC);

            mac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "ActiveProbing", BooleanValue(false));
            NetDeviceContainer staDev = wifi.Install(phy, mac, staC);

            // subnet: 10.5.(2+2*i).0/24
            uint32_t oct = 2 + 2 * i;
            ipv4.SetBase(MakeSubnet(oct).c_str(), "255.255.255.0");
            ipv4.Assign(apDev);
            Ipv4InterfaceContainer staIf = ipv4.Assign(staDev);

            // Node8 在这条 WiFi#2_i 上的 IP
            node8Wifi2Ips.push_back(staIf.GetAddress(0));
        }
    }

    // ============================================================
    // Mobility (CHANGED): 固定所有 WiFi 相关节点，避免走丢
    // ============================================================
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
    mobility.SetPositionAllocator(pos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    NodeContainer fixed;
    fixed.Add(node8);

    // 给每个 AP_i + client_i 固定位置（Node8 放中间）
    pos->Add(Vector(0.0, 0.0, 0.0)); // Node8

    for (uint32_t i = 0; i < numDualAp; ++i)
    {
        double baseX = 30.0 + 30.0 * i;
        // AP_i
        pos->Add(Vector(baseX, 0.0, 0.0));
        fixed.Add(dualApNodes[i]);

        // client STA_i
        pos->Add(Vector(baseX, -5.0, 0.0));
        fixed.Add(wifi1StaClients[i]);
    }

    mobility.Install(fixed);

    // ============================================================
    // (CHANGED) UDP traffic: 每个 client STA_i -> Node8
    //   多条 flow，FlowMonitor 一定会有 OVERALL
    // ============================================================
    NS_LOG_INFO("Install UDP traffic: ALL clients -> Node8 (multi paths).");

    uint16_t basePort = 5000;

    for (uint32_t i = 0; i < numDualAp; ++i)
    {
        Ptr<Node> clientNode = wifi1StaClients[i];
        uint16_t port = basePort + i;

        // server: Node8 (同一个 Node8 上开多个端口)
        UdpEchoServerHelper server(port);
        ApplicationContainer sApps = server.Install(node8);
        sApps.Start(Seconds(1.0));
        sApps.Stop(Seconds(simTimeSec - 0.1));

        // client: dest = Node8 在 WiFi#2_i 的 IP（每条路径不同）
        Ipv4Address dst = node8Wifi2Ips[i];

        UdpEchoClientHelper client(dst, port);
        client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        client.SetAttribute("Interval", TimeValue(Seconds(udpIntervalSec)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer cApps = client.Install(clientNode);
        cApps.Start(Seconds(appStartSec));
        cApps.Stop(Seconds(appStopSec));
    }

    // ============================================================
    // AntNet tables
    // ============================================================
    Ipv4AntNetRoutingHelper::BuildAntNetTopology();
    Ipv4AntNetRoutingHelper::InitializeNodeRoutingTables();

    NS_LOG_INFO("Initial Routing Tables:");
    Ipv4AntNetRoutingHelper::PrintRoutingTables();

    // Optional soft failure
    if (enableNodeFail)
    {
        NS_LOG_UNCOND("[CONFIG] EnableNodeFail=1, FailNodeId=" << failNodeId
                                                              << ", FailTime=" << failTimeSec << "s");
        Simulator::Schedule(Seconds(failTimeSec), &SoftBringNodeDown, c.Get(failNodeId));
    }
    else
    {
        NS_LOG_UNCOND("[CONFIG] EnableNodeFail=0");
    }

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    Ptr<Ipv4FlowClassifier> classifier;

    if (enableFlowMonitor)
    {
        monitor = flowmon.InstallAll();
        classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
        NS_LOG_UNCOND("[CONFIG] EnableMonitor=1");
    }
    else
    {
        NS_LOG_UNCOND("[CONFIG] EnableMonitor=0");
    }

    // Run
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(simTimeSec));
    Simulator::Run();

    NS_LOG_INFO("Final Routing Tables:");
    Ipv4AntNetRoutingHelper::PrintRoutingTables();

    // Print FlowMonitor results
    if (enableFlowMonitor && monitor && classifier)
    {
        monitor->CheckForLostPackets();
        auto stats = monitor->GetFlowStats();

        uint64_t totalRxPkts = 0;
        Time totalDelaySum = Seconds(0);

        std::cout << "\n========== FlowMonitor Results ==========\n";
        for (const auto& kv : stats)
        {
            FlowId id = kv.first;
            const FlowMonitor::FlowStats& st = kv.second;

            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(id);

            double avgDelayMs = (st.rxPackets > 0)
                                    ? (st.delaySum.GetSeconds() * 1000.0 / st.rxPackets)
                                    : 0.0;

            std::cout << "Flow " << id << "  "
                      << t.sourceAddress << ":" << t.sourcePort
                      << " -> " << t.destinationAddress << ":" << t.destinationPort
                      << "  proto=" << (uint32_t)t.protocol << "\n";
            std::cout << "  txPackets=" << st.txPackets
                      << "  rxPackets=" << st.rxPackets
                      << "  lostPackets=" << st.lostPackets
                      << "  avgDelay(ms)=" << std::fixed << std::setprecision(3) << avgDelayMs
                      << "\n";

            totalRxPkts += st.rxPackets;
            totalDelaySum += st.delaySum;
        }

        double overallAvgDelayMs = (totalRxPkts > 0)
                                       ? (totalDelaySum.GetSeconds() * 1000.0 / totalRxPkts)
                                       : 0.0;

        std::cout << "----------------------------------------\n";
        std::cout << "OVERALL (all flows combined): "
                  << "rxPackets=" << totalRxPkts
                  << "  avgDelay(ms)=" << std::fixed << std::setprecision(3) << overallAvgDelayMs
                  << "\n";
        std::cout << "========================================\n\n";
    }

    Simulator::Destroy();
    return 0;
}
