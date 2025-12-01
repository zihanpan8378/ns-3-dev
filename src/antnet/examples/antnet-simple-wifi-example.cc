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

                                LAN 10.4.0/24
                        =====================================
                        |                 |        |        |
                        |                 |        |        |     
                    [R0: 0.0.0.0]         N4       N5       N6
                        | 10.1.1.1/24
                        |
                        |
                        C1
                        |
                        |
                        | 10.1.1.2/24
                    [R2: 0.0.0.2]
         10.1.2.2/24  /   \  10.1.3.2/24
                     /     \
                    /       \       
                   C2        C3
                  /           \
                 /             \
    10.1.2.1/24 /               \ 10.1.3.1/24
    [R1: 0.0.0.1]             [R3: 0.0.0.3]          N7       N8       N9
                                *****************************************
                                AP
                                            Wifi 10.5.0/24

    C1:   P2P channel, 10.1.1.0/24
    C2:   P2P channel, 10.1.2.0/24
    C3:   P2P channel, 10.1.3.0/24
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
    LogComponentEnable("AntHeader", LOG_LEVEL_INFO);
    
    // Set up some default values for the simulation.  Use the
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(210));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("448kb/s"));
    Config::SetDefault("ns3::Ipv4AntNetRouting::ForwardAntInterval", TimeValue(Seconds(10)));

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
    NodeContainer lanNodes = NodeContainer(c.Get(0), c.Get(4), c.Get(5), c.Get(6));
    NodeContainer wifiApNode = NodeContainer(c.Get(3));
    NodeContainer wifiStaNodes = NodeContainer(c.Get(7), c.Get(8), c.Get(9));

    InternetStackHelper internet;
    internet.SetRoutingHelper(Ipv4AntNetRoutingHelper());
    internet.Install(c);
    internet.Install(lanNodes);
    internet.Install(wifiStaNodes);

    // Create channels
    NS_LOG_INFO("Create channels.");
    PointToPointHelper p2p;

    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p.SetDeviceAttribute ("Mtu", UintegerValue (1500));
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    p2p.SetDeviceAttribute("DataRate", StringValue("1500kbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer d3d2 = p2p.Install(n3n2);

    // Create LAN
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
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
    apDevices = wifi.Install(phy, mac, wifiApNode);

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
    Ipv4InterfaceContainer lanInterfaces = ipv4.Assign(lanDevices);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiApInterfaces = ipv4.Assign(apDevices);
    Ipv4InterfaceContainer wifiStaInterfaces = ipv4.Assign(staDevices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(5.0),
                                  "DeltaY",
                                  DoubleValue(10.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds",
                              RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(wifiStaNodes);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    // Create routing tables
    Ipv4AntNetRoutingHelper::BuildAntNetTopology();
    Ipv4AntNetRoutingHelper::InitializeNodeRoutingTables();

    // Print Initial Routing Tables
    NS_LOG_INFO("Initial Routing Tables:");
    Ipv4AntNetRoutingHelper::PrintRoutingTables();

    // Run Simulation
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(15));
    Simulator::Run();

    // Print Final Routing Tables
    NS_LOG_INFO("Final Routing Tables:");
    Ipv4AntNetRoutingHelper::PrintRoutingTables();
}
