#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

#include "ns3/ipv4-antnet-routing-helper.h"

#include <iomanip>
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleAntNetWifiExample");

int
main(int argc, char* argv[])
{
    // Seed
    std::srand(538);

    // Logging
    LogComponentEnable("SimpleAntNetWifiExample", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4AntNetRoutingHelper", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4AntNetRouting", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4AntNetRoutingTableEntry", LOG_LEVEL_INFO);
    LogComponentEnable("AntHeader", LOG_LEVEL_INFO);

    // Defaults
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(210));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("448kb/s"));
    Config::SetDefault("ns3::Ipv4AntNetRouting::ForwardAntInterval", TimeValue(Seconds(10)));
    Config::SetDefault("ns3::Ipv4AntNetRouting::UseBeaconWindow", BooleanValue(true));
    Config::SetDefault("ns3::Ipv4AntNetRouting::UseFailureMessagePropagation", BooleanValue(false));

    bool enableFlowMonitor = true;
    double simTime = 50.0;

    // Traffic knobs
    std::string appDataRate = "448kb/s";
    uint32_t pktSize = 210;
    double appStart = 2.0;
    double appStop = 45.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.AddValue("SimTime", "Simulation stop time (seconds)", simTime);
    cmd.AddValue("AppDataRate", "OnOff app data rate", appDataRate);
    cmd.AddValue("PacketSize", "OnOff app packet size", pktSize);
    cmd.AddValue("AppStart", "App start time (seconds)", appStart);
    cmd.AddValue("AppStop", "App stop time (seconds)", appStop);
    cmd.Parse(argc, argv);

    // Create nodes
    NS_LOG_INFO("Create nodes.");
    NodeContainer c;
    c.Create(10);

    NodeContainer n0n2 = NodeContainer(c.Get(0), c.Get(2));
    NodeContainer n1n2 = NodeContainer(c.Get(1), c.Get(2));
    NodeContainer n3n2 = NodeContainer(c.Get(3), c.Get(2));
    NodeContainer n1n3 = NodeContainer(c.Get(1), c.Get(3));
    NodeContainer n2n9 = NodeContainer(c.Get(2), c.Get(9));
    NodeContainer n6n9 = NodeContainer(c.Get(6), c.Get(9));
    NodeContainer n3n9 = NodeContainer(c.Get(3), c.Get(9));

    NodeContainer lanNodes = NodeContainer(c.Get(0), c.Get(4), c.Get(5), c.Get(6));
    NodeContainer wifiApNodes = NodeContainer(c.Get(3));
    NodeContainer wifiStaNodes = NodeContainer(c.Get(7), c.Get(8));

    // Install internet + AntNet routing
    InternetStackHelper internet;
    internet.SetRoutingHelper(Ipv4AntNetRoutingHelper());
    internet.Install(c);

    // P2P channels
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

    // LAN (CSMA)
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("0.1ms"));
    NetDeviceContainer lanDevices = csma.Install(lanNodes);

    // WiFi
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    WifiHelper wifi;

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNodes);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Assign IP
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i2 = ipv4.Assign(d3d2);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i3 = ipv4.Assign(d1d3);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i9 = ipv4.Assign(d2d9);

    ipv4.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer i6i9 = ipv4.Assign(d6d9);

    ipv4.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer i8i9 = ipv4.Assign(d3d9);

    ipv4.SetBase("10.1.8.0", "255.255.255.0");
    Ipv4InterfaceContainer lanIfs = ipv4.Assign(lanDevices);

    ipv4.SetBase("10.1.9.0", "255.255.255.0");
    Ipv4InterfaceContainer apIfs = ipv4.Assign(apDevices);
    Ipv4InterfaceContainer staIfs = ipv4.Assign(staDevices);

    // Mobility
    MobilityHelper mobility;

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // AP
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);

    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(wifiStaNodes);

    // AntNet tables
    Ipv4AntNetRoutingHelper::BuildAntNetTopology();
    Ipv4AntNetRoutingHelper::InitializeNodeRoutingTables();

    NS_LOG_INFO("Initial Routing Tables:");
    Ipv4AntNetRoutingHelper::PrintRoutingTables();

    // ----------------------------
    // (NEW) Applications: create NORMAL traffic flows
    // ----------------------------
    NS_LOG_INFO("Install Applications (normal UDP traffic).");

    uint16_t port1 = 5000;
    uint16_t port2 = 5001;

    // Pick some receivers on LAN: node4 and node5 are in lanNodes
    // lanIfs order corresponds to lanNodes: (node0, node4, node5, node6)
    Ipv4Address node4LanIp = lanIfs.GetAddress(1);
    Ipv4Address node5LanIp = lanIfs.GetAddress(2);

    // Senders: wifi STAs (node7, node8)
    // staIfs order corresponds to wifiStaNodes: (node7, node8)
    Ipv4Address node7WifiIp = staIfs.GetAddress(0);
    Ipv4Address node8WifiIp = staIfs.GetAddress(1);
    (void)node7WifiIp;
    (void)node8WifiIp;

    // Sink on node4 (UDP)
    PacketSinkHelper sink1("ns3::UdpSocketFactory",
                           InetSocketAddress(Ipv4Address::GetAny(), port1));
    ApplicationContainer sinkApp1 = sink1.Install(c.Get(4));
    sinkApp1.Start(Seconds(0.5));
    sinkApp1.Stop(Seconds(simTime - 0.1));

    // Sink on node5 (UDP)
    PacketSinkHelper sink2("ns3::UdpSocketFactory",
                           InetSocketAddress(Ipv4Address::GetAny(), port2));
    ApplicationContainer sinkApp2 = sink2.Install(c.Get(5));
    sinkApp2.Start(Seconds(0.5));
    sinkApp2.Stop(Seconds(simTime - 0.1));

    // OnOff from node7 -> node4
    OnOffHelper onoff1("ns3::UdpSocketFactory",
                       InetSocketAddress(node4LanIp, port1));
    onoff1.SetAttribute("DataRate", StringValue(appDataRate));
    onoff1.SetAttribute("PacketSize", UintegerValue(pktSize));
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer app1 = onoff1.Install(c.Get(7));
    app1.Start(Seconds(appStart));
    app1.Stop(Seconds(appStop));

    // OnOff from node8 -> node5
    OnOffHelper onoff2("ns3::UdpSocketFactory",
                       InetSocketAddress(node5LanIp, port2));
    onoff2.SetAttribute("DataRate", StringValue(appDataRate));
    onoff2.SetAttribute("PacketSize", UintegerValue(pktSize));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer app2 = onoff2.Install(c.Get(8));
    app2.Start(Seconds(appStart));
    app2.Stop(Seconds(appStop));
    
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    Ptr<Ipv4FlowClassifier> classifier;

    if (enableFlowMonitor)
    {
        monitor = flowmon.InstallAll();
        classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    }

    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    NS_LOG_INFO("Final Routing Tables:");
    Ipv4AntNetRoutingHelper::PrintRoutingTables();

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

            // per-flow average delay (only for received packets)
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