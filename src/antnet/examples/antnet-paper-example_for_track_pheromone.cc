#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include "ns3/ipv4-antnet-routing-helper.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

/*
                              10.1.1.1/24       10.1.1.2/24
                          [R0: 0.0.0.0] ----C1--- [R1: 0.0.0.1]
              10.1.3.1/24  /       \ 10.1.2.1/24         | 10.1.4.1/24
                          /         \                    |
                         /           \                   |
                       C3            C2                 C4
                       /               \                 |
                      /                 \                |
         10.1.3.2/24 /                   \ 10.1.2.2/24   | 10.1.4.2/24
              [R7: 0.0.0.7]        [R2: 0.0.0.2]  [R3: 0.0.0.3]
        10.1.9.2/24 |          10.1.5.1/24 \             | 10.1.6.1/24
                    |                       \            |
                    |                        \           |
                    |                         \          |
                    |                         C5        C6
                    |                           \        |
                    |                            \       |
                    |                             \      |
                   C9                              \     |
                    |                   10.1.5.2/24 \    | 10.1.6.2/24
                    |                             [R4: 0.0.0.4]
                    |                                    | 10.1.7.1/24
                    |                                    |
                    |                                   C7
                    |                                    |
        10.1.9.1/24 |                                    | 10.1.7.2/24
              [R6: 0.0.0.6] ----------C8--------- [R5: 0.0.0.5]
                      10.1.8.2/24           10.1.8.1/24

    C1: P2P channel, 10.1.1.0/24, R0 - R1
    C2: P2P channel, 10.1.2.0/24, R0 - R2
    C3: P2P channel, 10.1.3.0/24, R0 - R7
    C4: P2P channel, 10.1.4.0/24, R1 - R3
    C5: P2P channel, 10.1.5.0/24, R2 - R4
    C6: P2P channel, 10.1.6.0/24, R3 - R4
    C7: P2P channel, 10.1.7.0/24, R4 - R5
    C8: P2P channel, 10.1.8.0/24, R5 - R6
    C9: P2P channel, 10.1.9.0/24, R6 - R7
*/

NS_LOG_COMPONENT_DEFINE("PaperAntNetExample");

int
main(int argc, char* argv[])
{
    // Set random seed for reproducibility
    std::srand(538);

    // Enable logging
    LogComponentEnable("PaperAntNetExample", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4AntNetRoutingHelper", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4AntNetRouting", LOG_LEVEL_INFO);
    LogComponentEnable("AntHeader", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4AntNetLocalTrafficStatisticsEntry", LOG_LEVEL_INFO);
    
    // Set up some default values for the simulation.  Use the
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(512));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("448kb/s"));
    Config::SetDefault("ns3::Ipv4AntNetRouting::ForwardAntInterval", TimeValue(Seconds(5.0)));
    Config::SetDefault("ns3::Ipv4AntNetRouting::UseBeaconWindow", BooleanValue(false));
    Config::SetDefault("ns3::Ipv4AntNetRouting::UseFailureMessagePropagation", BooleanValue(false));

    CommandLine cmd(__FILE__);
    bool enableFlowMonitor = false;
    cmd.AddValue("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.Parse(argc, argv);

    // Create nodes
    NS_LOG_INFO("Create nodes.");
    NodeContainer c;
    c.Create(8);
    NodeContainer n0n1 = NodeContainer(c.Get(0), c.Get(1));
    NodeContainer n0n2 = NodeContainer(c.Get(0), c.Get(2));
    NodeContainer n0n7 = NodeContainer(c.Get(0), c.Get(7));
    NodeContainer n1n3 = NodeContainer(c.Get(1), c.Get(3));
    NodeContainer n2n4 = NodeContainer(c.Get(2), c.Get(4));
    NodeContainer n3n4 = NodeContainer(c.Get(3), c.Get(4));
    NodeContainer n4n5 = NodeContainer(c.Get(4), c.Get(5));
    NodeContainer n5n6 = NodeContainer(c.Get(5), c.Get(6));
    NodeContainer n6n7 = NodeContainer(c.Get(6), c.Get(7));

    InternetStackHelper internet;
    internet.SetRoutingHelper(Ipv4AntNetRoutingHelper());
    internet.Install(c);

    // Create channels
    NS_LOG_INFO("Create channels.");
    PointToPointHelper p2p;

    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    p2p.SetDeviceAttribute ("Mtu", UintegerValue (1500));
    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d0d7 = p2p.Install(n0n7);
    NetDeviceContainer d1d3 = p2p.Install(n1n3);
    NetDeviceContainer d2d4 = p2p.Install(n2n4);
    NetDeviceContainer d3d4 = p2p.Install(n3n4);
    NetDeviceContainer d4d5 = p2p.Install(n4n5);
    NetDeviceContainer d5d6 = p2p.Install(n5n6);
    NetDeviceContainer d6d7 = p2p.Install(n6n7);

    // Assign IP addresses
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i7 = ipv4.Assign(d0d7);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i3 = ipv4.Assign(d1d3);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i4 = ipv4.Assign(d2d4);

    ipv4.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i4 = ipv4.Assign(d3d4);

    ipv4.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer i4i5 = ipv4.Assign(d4d5);

    ipv4.SetBase("10.1.8.0", "255.255.255.0");
    Ipv4InterfaceContainer i5i6 = ipv4.Assign(d5d6);

    ipv4.SetBase("10.1.9.0", "255.255.255.0");
    Ipv4InterfaceContainer i6i7 = ipv4.Assign(d6d7);

    // Create routing tables
    Ipv4AntNetRoutingHelper::BuildAntNetTopology();
    Ipv4AntNetRoutingHelper::InitializeNodeRoutingTablesForSpecificSourceAndDestination(0,Ipv4Address("10.1.8.2"));

    // Print Initial Routing Tables
    NS_LOG_INFO("Initial Routing Tables:");
    Ipv4AntNetRoutingHelper::PrintRoutingTables();


    // for packet sending
    // uint16_t port = 9000;

    // InetSocketAddress dest = InetSocketAddress(i6i7.GetAddress(0), port);

    // OnOffHelper onoff("ns3::UdpSocketFactory", dest);  MPIA = 0.3ms
    // onoff.SetAttribute("DataRate", StringValue("2Mbps"));
    // onoff.SetAttribute("PacketSize", UintegerValue(512));

    // ApplicationContainer apps = onoff.Install(c.Get(0));
    // apps.Start(Seconds(30.0));
    // apps.Stop(Seconds(200.0));


    // Run Simulation
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print Final Routing Tables
    NS_LOG_INFO("Final Routing Tables:");
    Ipv4AntNetRoutingHelper::PrintRoutingTables();

}

