/**
 * SimpleNet Experiment from AntNet Paper (Di Caro & Dorigo, 1998)
 *
 * Network Topology (Figure 5):
 *   8 nodes, 9 bidirectional links
 *   (μ, σ, N) = (1.9, 0.7, 8)
 *
 * Physical Parameters:
 *   - Link bandwidth: 10 Mbit/s (all links)
 *   - Propagation delay: 1 msec (all links)
 *   - Node buffer: 1 Gbit
 *
 * Traffic Pattern:
 *   - Type: F-CBR (Fixed Constant Bit Rate)
 *   - Source: Node 1
 *   - Destination: Node 6
 *   - Load: Higher than single link capacity (forces multipath)
 *   - Packet size: 4096 bits = 512 bytes
 *
 * Simulation Settings:
 *   - Total time: 1500 seconds (500s warmup + 1000s measurement)
 *   - Warmup period: 500 seconds (Ant-only, no traffic)
 *   - Measurement period: 1000 seconds (traffic starts after warmup)
 *   - Trials: 10 (results averaged)
 *
 * Purpose:
 *   Study how different algorithms distribute load across multiple available paths
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/antnet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/antnet-routing-protocol.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/log.h"
#include "ns3/ptr.h"
#include "ns3/nstime.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AntNetSimpleNet");

// Global variables for statistics collection
uint64_t g_totalBytesReceived = 0;
uint64_t g_totalPacketsSent = 0;
uint64_t g_totalPacketsReceived = 0;
std::vector<double> g_packetDelays;
Time g_warmupEnd;

// Global variables for pheromone logging
std::ofstream g_pheromoneLogFile;
NodeContainer g_allNodes;

/**
 * Helper function to get AntNetRoutingProtocol from a node
 */
Ptr<AntNetRoutingProtocol> GetAntNet(Ptr<Node> node)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    if (!ipv4) return nullptr;

    Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol();
    Ptr<AntNetRoutingProtocol> ant = DynamicCast<AntNetRoutingProtocol>(proto);
    if (ant) return ant;

    // Try Ipv4ListRouting
    Ptr<Ipv4ListRouting> list = DynamicCast<Ipv4ListRouting>(proto);
    if (list) {
        for (uint32_t i = 0; i < list->GetNRoutingProtocols(); ++i) {
            int16_t priority;
            Ptr<Ipv4RoutingProtocol> rp = list->GetRoutingProtocol(i, priority);
            ant = DynamicCast<AntNetRoutingProtocol>(rp);
            if (ant) return ant;
        }
    }
    return nullptr;
}

/**
 * Periodically log pheromone tables of all nodes
 */
void LogPheromoneTable()
{
    double currentTime = Simulator::Now().GetSeconds();

    g_pheromoneLogFile << "\n========================================\n";
    g_pheromoneLogFile << "Time: " << std::fixed << std::setprecision(3) << currentTime << " seconds\n";
    g_pheromoneLogFile << "========================================\n";

    for (uint32_t i = 0; i < g_allNodes.GetN(); ++i)
    {
        Ptr<Node> node = g_allNodes.Get(i);
        Ptr<AntNetRoutingProtocol> antnet = GetAntNet(node);

        if (antnet)
        {
            g_pheromoneLogFile << "\n--- Node " << node->GetId() << " ---\n";

            // Use the new DumpPheromoneTableToStream method
            antnet->DumpPheromoneTableToStream(g_pheromoneLogFile);
            g_pheromoneLogFile << "\n";
        }
    }

    g_pheromoneLogFile << std::endl;
    g_pheromoneLogFile.flush();
}

/**
 * Packet receive callback for statistics
 */
void ReceivePacket(Ptr<const Packet> packet, const Address& address)
{
    if (Simulator::Now() >= g_warmupEnd)
    {
        g_totalBytesReceived += packet->GetSize();
        g_totalPacketsReceived++;
    }
}

/**
 * Track packet transmission
 */
void TransmitPacket(Ptr<const Packet> packet)
{
    if (Simulator::Now() >= g_warmupEnd)
    {
        g_totalPacketsSent++;
    }
}

/**
 * Run a single trial of the SimpleNet experiment
 */
void RunTrial(uint32_t trialNumber, double dataRate, std::ofstream& resultsFile,
              double logInterval, bool enablePheromoneLog)
{
    NS_LOG_INFO("=== Running Trial " << trialNumber << " with DataRate=" << dataRate << " Mbps ===");

    // Reset statistics
    g_totalBytesReceived = 0;
    g_totalPacketsSent = 0;
    g_totalPacketsReceived = 0;
    g_packetDelays.clear();

    // Create 8 nodes
    NodeContainer nodes;
    nodes.Create(8);
    g_allNodes = nodes;  // Store for pheromone logging

    // Create CSMA channels for the links
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("1ms"));

    std::vector<NetDeviceContainer> links;

    // Define link pairs
    std::vector<std::pair<uint32_t, uint32_t>> linkPairs = {
        {0, 1},  
        {0, 2},
        {0, 7},  
        {1, 3},
        {2, 4},  
        {3, 4},  
        {4, 5},  
        {5, 6}, 
        {6, 7},
    };

    // Create all links
    for (const auto& pair : linkPairs)
    {
        NodeContainer linkNodes;
        linkNodes.Add(nodes.Get(pair.first));
        linkNodes.Add(nodes.Get(pair.second));
        links.push_back(csma.Install(linkNodes));
    }

    // Install AntNet routing
    AntNetHelper antnet;
    antnet.Set("BetaAnt", DoubleValue(1.5));        // Exploration temperature
    antnet.Set("BetaData", DoubleValue(0.7));       // Exploitation temperature
    antnet.Set("AlphaLearn", DoubleValue(0.2));     // Learning rate
    antnet.Set("Eta", DoubleValue(0.005));          // RTT mean tracking (from paper)
    antnet.Set("Phi", DoubleValue(0.05));           // RTT variance tracking
    antnet.Set("AntPeriod", TimeValue(Seconds(0.3)));  // Δg from paper (correct attribute name)

    InternetStackHelper internet;
    internet.SetRoutingHelper(antnet);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> interfaces;

    for (size_t i = 0; i < links.size(); ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces.push_back(ipv4.Assign(links[i]));
    }

    // Configure static neighbors for each node based on topology
    // Link structure (from linkPairs):
    //   {0,1}, {0,2}, {0,7}, {1,3}, {2,4}, {3,4}, {4,5}, {5,6}, {6,7}
    //   Interface indices:
    //     0: {0,1} -> nodes.Get(0) and nodes.Get(1)
    //     1: {0,2} -> nodes.Get(0) and nodes.Get(2)
    //     2: {0,7} -> nodes.Get(0) and nodes.Get(7)
    //     3: {1,3} -> nodes.Get(1) and nodes.Get(3)
    //     4: {2,4} -> nodes.Get(2) and nodes.Get(4)
    //     5: {3,4} -> nodes.Get(3) and nodes.Get(4)
    //     6: {4,5} -> nodes.Get(4) and nodes.Get(5)
    //     7: {5,6} -> nodes.Get(5) and nodes.Get(6)
    //     8: {6,7} -> nodes.Get(6) and nodes.Get(7)

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<AntNetRoutingProtocol> antnet = GetAntNet(nodes.Get(i));
        if (antnet)
        {
            // Add neighbors based on link topology
            if (i == 0) {
                antnet->AddStaticNeighbor(interfaces[0].GetAddress(1)); // to node 1
                antnet->AddStaticNeighbor(interfaces[1].GetAddress(1)); // to node 2
                antnet->AddStaticNeighbor(interfaces[2].GetAddress(1)); // to node 7
            }
            else if (i == 1) {
                antnet->AddStaticNeighbor(interfaces[0].GetAddress(0)); // to node 0
                antnet->AddStaticNeighbor(interfaces[3].GetAddress(1)); // to node 3
            }
            else if (i == 2) {
                antnet->AddStaticNeighbor(interfaces[1].GetAddress(0)); // to node 0
                antnet->AddStaticNeighbor(interfaces[4].GetAddress(1)); // to node 4
            }
            else if (i == 3) {
                antnet->AddStaticNeighbor(interfaces[3].GetAddress(0)); // to node 1
                antnet->AddStaticNeighbor(interfaces[5].GetAddress(1)); // to node 4
            }
            else if (i == 4) {
                antnet->AddStaticNeighbor(interfaces[4].GetAddress(0)); // to node 2
                antnet->AddStaticNeighbor(interfaces[5].GetAddress(0)); // to node 3
                antnet->AddStaticNeighbor(interfaces[6].GetAddress(1)); // to node 5
            }
            else if (i == 5) {
                antnet->AddStaticNeighbor(interfaces[6].GetAddress(0)); // to node 4
                antnet->AddStaticNeighbor(interfaces[7].GetAddress(1)); // to node 6
            }
            else if (i == 6) {
                antnet->AddStaticNeighbor(interfaces[7].GetAddress(0)); // to node 5
                antnet->AddStaticNeighbor(interfaces[8].GetAddress(1)); // to node 7
            }
            else if (i == 7) {
                antnet->AddStaticNeighbor(interfaces[2].GetAddress(0)); // to node 0
                antnet->AddStaticNeighbor(interfaces[8].GetAddress(0)); // to node 6
            }

            // Discover all nodes for node ID mapping
            antnet->DiscoverAllNodesPublic();
        }
    }

    // Set up traffic: F-CBR from Node 1 (index 0) to Node 6 (index 5)
    // Node numbering in paper: 1-indexed, in code: 0-indexed
    // Source: Node 1 (paper) = Node 0 (code)
    // Destination: Node 6 (paper) = Node 5 (code)

    uint16_t port = 9;

    // Install packet sink on destination (Node 6 = index 5)
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(5));

    // PAPER-COMPLIANT: Start traffic AFTER warmup period (500s)
    // 0-500s: Ant-only period (routing table initialization)
    // 500-1500s: Traffic + Ant period (1000s measurement)
    sinkApp.Start(Seconds(500.0));  // Changed from 0.0
    sinkApp.Stop(Seconds(1500.0));  // Changed from 1000.0

    // Connect receive callback
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    sink->TraceConnectWithoutContext("Rx", MakeCallback(&ReceivePacket));

    // Install OnOff application on source (Node 1 = index 0)
    // Get destination address
    Ipv4Address destAddr = nodes.Get(5)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(destAddr, port));

    // F-CBR configuration: constant bit rate, no off time
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(std::to_string(dataRate) + "Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));  // 4096 bits = 512 bytes
    onoff.SetConstantRate(DataRate(std::to_string(dataRate) + "Mbps"), 512);

    ApplicationContainer sourceApp = onoff.Install(nodes.Get(0));

    // PAPER-COMPLIANT: Start traffic AFTER warmup period
    sourceApp.Start(Seconds(500.0));  // Changed from 0.0
    sourceApp.Stop(Seconds(1500.0));  // Changed from 1000.0

    // Connect transmit callback to track sent packets
    Ptr<OnOffApplication> onoffApp = DynamicCast<OnOffApplication>(sourceApp.Get(0));
    onoffApp->TraceConnectWithoutContext("Tx", MakeCallback(&TransmitPacket));

    // Set warmup end time (500 seconds as per paper)
    // This marks the end of the initialization period
    g_warmupEnd = Seconds(500.0);

    // Enable flow monitor for detailed statistics
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    // Schedule periodic pheromone table logging if enabled
    if (enablePheromoneLog)
    {
        // Open pheromone log file for this trial
        std::ostringstream pheromoneLogName;
        pheromoneLogName << "pheromone-trial-" << trialNumber << ".log";
        g_pheromoneLogFile.open(pheromoneLogName.str());

        g_pheromoneLogFile << "========================================\n";
        g_pheromoneLogFile << "Pheromone Table Log - Trial " << trialNumber << "\n";
        g_pheromoneLogFile << "Data Rate: " << dataRate << " Mbps\n";
        g_pheromoneLogFile << "Log Interval: " << logInterval << " seconds\n";
        g_pheromoneLogFile << "Note: Traffic starts at t=500s (after warmup)\n";
        g_pheromoneLogFile << "========================================\n\n";

        // Schedule periodic logging throughout entire simulation
        // Log during warmup period to see routing table initialization
        for (double t = logInterval; t < 1500.0; t += logInterval)
        {
            Simulator::Schedule(Seconds(t), &LogPheromoneTable);
        }

        // Log at the end as well
        Simulator::Schedule(Seconds(1499.9), &LogPheromoneTable);
    }

    // Run simulation: 500s warmup + 1000s measurement = 1500s total
    Simulator::Stop(Seconds(1500.0));
    Simulator::Run();

    // Collect flow monitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    // Calculate metrics (measurement period: 500-1500s = 1000s)
    double measurementTime = 1500.0 - 500.0;  // 1000s measurement period (paper-compliant)
    double throughputMbps = (g_totalBytesReceived * 8.0) / (measurementTime * 1e6);

    // Calculate delay statistics from flow monitor
    std::vector<double> delays;
    for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
        if (iter->second.rxPackets > 0)
        {
            double avgDelay = iter->second.delaySum.GetSeconds() / iter->second.rxPackets;
            delays.push_back(avgDelay);
        }
    }

    // Calculate 90th percentile delay
    double delay90th = 0.0;
    if (!delays.empty())
    {
        std::sort(delays.begin(), delays.end());
        size_t idx = static_cast<size_t>(delays.size() * 0.9);
        if (idx < delays.size())
        {
            delay90th = delays[idx];
        }
    }

    // Packet delivery ratio
    double deliveryRatio = g_totalPacketsSent > 0
        ? (double)g_totalPacketsReceived / g_totalPacketsSent
        : 0.0;

    // Write results to file
    resultsFile << trialNumber << ","
                << dataRate << ","
                << throughputMbps << ","
                << delay90th * 1000.0 << ","  // Convert to ms
                << deliveryRatio << std::endl;

    NS_LOG_INFO("Trial " << trialNumber << " completed:");
    NS_LOG_INFO("  Throughput: " << throughputMbps << " Mbps");
    NS_LOG_INFO("  90th percentile delay: " << delay90th * 1000.0 << " ms");
    NS_LOG_INFO("  Delivery ratio: " << deliveryRatio);

    // Close pheromone log file if it was opened
    if (g_pheromoneLogFile.is_open())
    {
        g_pheromoneLogFile.close();
    }

    Simulator::Destroy();
}

int main(int argc, char* argv[])
{
    // Default parameters
    uint32_t numTrials = 1;
    double baseDataRate = 15.0;  // Mbps, higher than single link (10 Mbps)
    std::string outputFile = "simplenet-results.csv";
    bool verbose = false;
    bool enablePheromoneLog = false;
    double logInterval = 50.0;  // seconds

    // Parse command line
    CommandLine cmd;
    cmd.AddValue("trials", "Number of trials to run", numTrials);
    cmd.AddValue("dataRate", "Data rate in Mbps (should be > 10 to force multipath)", baseDataRate);
    cmd.AddValue("output", "Output CSV file name", outputFile);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.AddValue("pheromoneLog", "Enable pheromone table logging", enablePheromoneLog);
    cmd.AddValue("logInterval", "Pheromone log interval in seconds", logInterval);
    cmd.Parse(argc, argv);

    // Configure logging
    if (verbose)
    {
        LogComponentEnable("AntNetSimpleNet", LOG_LEVEL_INFO);
        LogComponentEnable("AntNetRoutingProtocol", LOG_LEVEL_INFO);
    }

    // Open results file
    std::ofstream resultsFile(outputFile);
    resultsFile << "Trial,DataRate_Mbps,Throughput_Mbps,Delay90th_ms,DeliveryRatio" << std::endl;

    std::cout << "========================================" << std::endl;
    std::cout << "SimpleNet Experiment (AntNet Paper)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Network: 8 nodes, 9 bidirectional links" << std::endl;
    std::cout << "Link bandwidth: 10 Mbit/s" << std::endl;
    std::cout << "Link delay: 1 msec" << std::endl;
    std::cout << "Traffic: Node 1 -> Node 6 (F-CBR)" << std::endl;
    std::cout << "Data rate: " << baseDataRate << " Mbps" << std::endl;
    std::cout << "Number of trials: " << numTrials << std::endl;
    std::cout << "Total simulation time: 1500 seconds" << std::endl;
    std::cout << "Warmup period: 500 seconds (Ant-only)" << std::endl;
    std::cout << "Measurement period: 1000 seconds (500-1500s)" << std::endl;
    std::cout << "Pheromone logging: " << (enablePheromoneLog ? "enabled" : "disabled") << std::endl;
    if (enablePheromoneLog)
    {
        std::cout << "Log interval: " << logInterval << " seconds" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    // Run multiple trials and average results
    std::vector<double> throughputs, delays, deliveryRatios;

    for (uint32_t trial = 1; trial <= numTrials; ++trial)
    {
        RunTrial(trial, baseDataRate, resultsFile, logInterval, enablePheromoneLog);
    }

    resultsFile.close();

    std::cout << "\n========================================" << std::endl;
    std::cout << "Experiment completed!" << std::endl;
    std::cout << "Results written to: " << outputFile << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
