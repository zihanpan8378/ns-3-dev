#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
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

using namespace ns3;

/*

                                   LAN 10.8.0/24
                        =====================================
                        |                 |        |        |
                        |                 |        |        |     
                    [R0: 0.0.0.0]         N4       N5       N6
            10.1.1.1/24 |                                   | 10.1.6.1/24
                        |                                   |
                        |                                   |
                        C1                                  C6
                        |                                   |
                        |                                   |
            10.1.1.2/24 |      10.1.5.1/24      10.1.5.2/24 | 10.1.6.2/24
                    [R2: 0.0.0.2]----------C5----------[R9: 0.0.0.2]
          10.1.2.2/24 /   \ 10.1.3.2/24               / 10.1.7.2/24
                     /     \                         /
                    /       \                       /
                   C2        C3                    /  
                  /           \                   C7      
                 /             \                 /      
    10.1.2.1/24 /               \ 10.1.3.1/24   /       
    [R1: 0.0.0.1]------C4-----[R3: 0.0.0.3]----/             N7                 N8
        10.1.4.1/24      10.1.4.2/24  *    10.1.7.1/24        *                  *
                              ****************************************************
                                AP        Wifi 10.9.0/24

    C1:   P2P channel, 10.1.1.0/24
    C2:   P2P channel, 10.1.2.0/24
    C3:   P2P channel, 10.1.3.0/24
    C4:   P2P channel, 10.1.4.0/24
    C5:   P2P channel, 10.1.5.0/24
    C6:   P2P channel, 10.1.6.0/24
    C7:   P2P channel, 10.1.7.0/24
    LAN:  CSMA channel, 10.4.0/24
    Wifi: Wifi channel, 10.5.0/24 
*/

NS_LOG_COMPONENT_DEFINE("SimpleAntNetWifiExample");

int
main(int argc, char* argv[])
{
    // Set random seed for reproducibility
    std::srand(538);

    // Enable logging
    LogComponentEnable("SimpleAntNetWifiExample", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4AntNetRoutingHelper", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4AntNetRouting", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4AntNetRoutingTableEntry", LOG_LEVEL_INFO);
    LogComponentEnable("AntHeader", LOG_LEVEL_INFO);
    
    // Set up some default values for the simulation.  Use the
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(210));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("448kb/s"));
    Config::SetDefault("ns3::Ipv4AntNetRouting::ForwardAntInterval", TimeValue(Seconds(10)));
    Config::SetDefault("ns3::Ipv4AntNetRouting::UseBeaconWindow", BooleanValue(true));
    Config::SetDefault("ns3::Ipv4AntNetRouting::UseFailureMessagePropagation", BooleanValue(false));

    CommandLine cmd(__FILE__);
    bool enableFlowMonitor = false;
    cmd.AddValue("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
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

    InternetStackHelper internet;
    internet.SetRoutingHelper(Ipv4AntNetRoutingHelper());
    internet.Install(c);

    // Create channels
    NS_LOG_INFO("Create channels.");
    PointToPointHelper p2p;

    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p.SetDeviceAttribute ("Mtu", UintegerValue (1500));
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

    // Create LAN
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("0.1ms"));
    NetDeviceContainer lanDevices = csma.Install(lanNodes);

    // Create Wifi
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    WifiHelper wifi;

    NetDeviceContainer apDevices;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevices = wifi.Install(phy, mac, wifiApNodes);

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Assign IP Addresses
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
    Ipv4InterfaceContainer lanInterfaces = ipv4.Assign(lanDevices);

    ipv4.SetBase("10.1.9.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiApInterfaces = ipv4.Assign(apDevices);
    Ipv4InterfaceContainer wifiStaInterfaces = ipv4.Assign(staDevices);

    // Set up mobility
    MobilityHelper mobility;
    
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));    // AP1
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));   // AP2

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNodes);
    
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(wifiStaNodes);

    // Create routing tables
    Ipv4AntNetRoutingHelper::BuildAntNetTopology();
    Ipv4AntNetRoutingHelper::InitializeNodeRoutingTables();

    // Print Initial Routing Tables
    NS_LOG_INFO("Initial Routing Tables:");
    Ipv4AntNetRoutingHelper::PrintRoutingTables();

    // Run Simulation
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(50));
    Simulator::Run();

    // Print Final Routing Tables
    NS_LOG_INFO("Final Routing Tables:");
    Ipv4AntNetRoutingHelper::PrintRoutingTables();
}
