#include "ns3/antnet-routing-protocol.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/packet.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-raw-socket-factory.h"
#include "ns3/boolean.h"
#include "ns3/node.h"  // for GetId()
#include "ns3/node-list.h"
#include "ns3/net-device.h"
#include "ns3/socket.h"  // for SocketPriorityTag
#include <limits>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("AntNetRoutingProtocol");
NS_OBJECT_ENSURE_REGISTERED(AntNetRoutingProtocol);

TypeId AntNetRoutingProtocol::GetTypeId() {
  static TypeId tid = TypeId("ns3::AntNetRoutingProtocol")
    .SetParent<Ipv4RoutingProtocol>()
    .SetGroupName("Internet")
    .AddConstructor<AntNetRoutingProtocol>()
    .AddAttribute("AntPort", "UDP port for ant control packets",
                  UintegerValue(5001),
                  MakeUintegerAccessor(&AntNetRoutingProtocol::m_antPort),
                  MakeUintegerChecker<uint16_t>())
    .AddAttribute("HelloPort", "UDP port for neighbor hello",
                  UintegerValue(5002),
                  MakeUintegerAccessor(&AntNetRoutingProtocol::m_helloPort),
                  MakeUintegerChecker<uint16_t>())
    .AddAttribute("HelloPeriod", "Interval to send hello beacons",
                  TimeValue(Seconds(1.0)),
                  MakeTimeAccessor(&AntNetRoutingProtocol::m_helloPeriod),
                  MakeTimeChecker())
    .AddAttribute("NeighborTimeout", "Neighbor expiry interval",
                  TimeValue(Seconds(3.0)),
                  MakeTimeAccessor(&AntNetRoutingProtocol::m_neighborTimeout),
                  MakeTimeChecker())
    .AddAttribute("AntPeriod", "Interval to launch forward ants per known destination",
                  TimeValue(Seconds(3.0)),
                  MakeTimeAccessor(&AntNetRoutingProtocol::m_antPeriod),
                  MakeTimeChecker())
    .AddAttribute("BetaAnt", "Exponent for ant next-hop sampling",
                  DoubleValue(0.5),
                  MakeDoubleAccessor(&AntNetRoutingProtocol::m_betaAnt),
                  MakeDoubleChecker<double>())
    .AddAttribute("BetaData", "Exponent for data next-hop sampling",
                  DoubleValue(1.3),
                  MakeDoubleAccessor(&AntNetRoutingProtocol::m_betaData),
                  MakeDoubleChecker<double>())
    .AddAttribute("AlphaLearn", "Learning rate for reinforcement updates",
                  DoubleValue(0.4),
                  MakeDoubleAccessor(&AntNetRoutingProtocol::m_alphaLearn),
                  MakeDoubleChecker<double>())
    .AddAttribute("Eta", "EWMA step for mu/sigma2 stats",
                  DoubleValue(0.1),
                  MakeDoubleAccessor(&AntNetRoutingProtocol::m_eta),
                  MakeDoubleChecker<double>())
    .AddAttribute("Phi", "Power map for data route sampling",
                  DoubleValue(1.2),
                  MakeDoubleAccessor(&AntNetRoutingProtocol::m_phi),
                  MakeDoubleChecker<double>())
    .AddAttribute("DecayFactor", "Pheromone decay factor per period",
                  DoubleValue(0.95),
                  MakeDoubleAccessor(&AntNetRoutingProtocol::m_decayFactor),
                  MakeDoubleChecker<double>())
    .AddAttribute("DecayPeriod", "Interval to apply pheromone decay",
                  TimeValue(Seconds(5.0)),
                  MakeTimeAccessor(&AntNetRoutingProtocol::m_decayPeriod),
                  MakeTimeChecker());
  return tid;
}

AntNetRoutingProtocol::AntNetRoutingProtocol()
  : m_running(false),
    m_ipv4(nullptr),
    m_antSocket(nullptr),
    m_helloSocket(nullptr),
    m_antPort(5001),
    m_helloPort(5002),
    m_betaAnt(0.5),
    m_betaData(1.3),
    m_alphaLearn(0.4),
    m_eta(0.1),
    m_phi(1.2),
    m_decayFactor(0.9),
    m_helloPeriod(Seconds(1.0)),
    m_neighborTimeout(Seconds(3.0)),
    m_antPeriod(Seconds(5.0)),
    m_decayPeriod(Seconds(5.0)),
    m_antSeq(1)
{
  m_rng = CreateObject<UniformRandomVariable>();
}

AntNetRoutingProtocol::~AntNetRoutingProtocol() {
  Stop();
}

void AntNetRoutingProtocol::SetIpv4(Ptr<Ipv4> ipv4) {
  NS_LOG_FUNCTION(this << ipv4);
  if (ipv4 == nullptr) {
    Stop();
    m_ipv4 = nullptr;
    return;
  }
  m_ipv4 = ipv4;
  Simulator::ScheduleNow(&AntNetRoutingProtocol::Start, this);
}

void AntNetRoutingProtocol::Start() {
  if (m_running || m_ipv4 == nullptr) return;
  m_running = true;
  CreateSockets();

  DiscoverAllNodes();

  m_antEvent = Simulator::Schedule(m_antPeriod, &AntNetRoutingProtocol::ScheduleAnt, this);
  m_decayEvent = Simulator::Schedule(m_decayPeriod, &AntNetRoutingProtocol::ApplyPheromoneDecay, this);
}

void AntNetRoutingProtocol::Stop() {
  if (!m_running) return;
  m_running = false;
  if (m_antSocket) { m_antSocket->Close(); m_antSocket = nullptr; }
  if (m_helloSocket) { m_helloSocket->Close(); m_helloSocket = nullptr; }
  if (m_helloEvent.IsPending()) m_helloEvent.Cancel();
  if (m_antEvent.IsPending()) m_antEvent.Cancel();
  if (m_decayEvent.IsPending()) m_decayEvent.Cancel();
}

void AntNetRoutingProtocol::CreateSockets() {
  if (!m_antSocket) {
    m_antSocket = Socket::CreateSocket(GetObject<Node>(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_antPort);
    m_antSocket->Bind(local);
    m_antSocket->SetRecvCallback(MakeCallback(&AntNetRoutingProtocol::RecvAnt, this));
  }
  if (!m_helloSocket) {
    m_helloSocket = Socket::CreateSocket(GetObject<Node>(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_helloPort);
    m_helloSocket->Bind(local);
    m_helloSocket->SetRecvCallback(MakeCallback(&AntNetRoutingProtocol::RecvHello, this));
    m_helloSocket->SetAllowBroadcast(true);
  }
}

void AntNetRoutingProtocol::SendHello() {
  // Not used when static neighbors are configured in the example, kept for compatibility.
  NS_LOG_INFO("SendHello: nIf=" << m_ipv4->GetNInterfaces());
  Ptr<Packet> p = Create<Packet>(1);
  for (uint32_t i=0; i<m_ipv4->GetNInterfaces(); ++i) {
    for (uint32_t j=0; j<m_ipv4->GetNAddresses(i); ++j) {
      Ipv4InterfaceAddress ifaddr = m_ipv4->GetAddress(i, j);
      if (ifaddr.GetMask() == Ipv4Mask::GetZero()) continue;
      Ipv4Address bcast = ifaddr.GetBroadcast();
      m_helloSocket->BindToNetDevice(m_ipv4->GetNetDevice(i));
      m_helloSocket->SendTo(p->Copy(), 0, InetSocketAddress(bcast, m_helloPort));
      m_helloSocket->BindToNetDevice(Ptr<NetDevice>());
    }
  }
  Time now = Simulator::Now();
  std::vector<Ipv4Address> toErase;
  for (auto const& kv : m_neighbors) {
    if (!kv.second.isStatic && now - kv.second.lastSeen > m_neighborTimeout) {
      toErase.push_back(kv.first);
    }
  }
  for (auto const& a : toErase) m_neighbors.erase(a);
  m_helloEvent = Simulator::Schedule(m_helloPeriod, &AntNetRoutingProtocol::SendHello, this);
}

void AntNetRoutingProtocol::RecvHello(Ptr<Socket> socket) {
  // Not used when static neighbors are configured in the example, kept for compatibility.
  Address from;
  Ptr<Packet> p = socket->RecvFrom(from);
  InetSocketAddress isa = InetSocketAddress::ConvertFrom(from);
  Ipv4Address peer = isa.GetIpv4();
  if (IsMyAddress(peer)) return;
  auto it = m_neighbors.find(peer);
  if (it == m_neighbors.end()) {
    m_neighbors.emplace(peer, NeighborInfo{Simulator::Now(), false});
  } else {
    it->second.lastSeen = Simulator::Now();
  }
  NS_LOG_INFO("RecvHello from=" << peer);
}

void AntNetRoutingProtocol::ScheduleAnt() {
  NS_LOG_INFO("ScheduleAnt node=" << GetObject<Node>()->GetId()
               << " knownDestinationNodes=" << m_knownDestinationNodes.size()
               << " neighbors=" << m_neighbors.size());
  LaunchAntsForKnownDestinations();
  m_antEvent = Simulator::Schedule(m_antPeriod, &AntNetRoutingProtocol::ScheduleAnt, this);
}

void AntNetRoutingProtocol::LaunchAntsForKnownDestinations() {
  // Select destination using node ID
  std::vector<uint32_t> candidates;
  std::vector<double> weights;
  double totalWeight = 0.0;
  
  for (auto const& nodeId : m_knownDestinationNodes) {
    double w = m_ph.GetFlowWeightForNode(nodeId);
    if (w <= 0.0) {
      w = 1e-6; // fallback weight so destinations without history still participate
    }
    candidates.push_back(nodeId);
    weights.push_back(w);
    totalWeight += w;
  }
  
  if (candidates.empty()) {
    NS_LOG_WARN("LaunchAntsForKnownDestinations: no candidates!");
    return;
  }
  
  if (totalWeight <= 0.0) {
    totalWeight = static_cast<double>(candidates.size());
    for (auto &w : weights) {
      w = 1.0;
    }
  }
  
  // Use epsilon-greedy strategy: 20% exploration, 80% exploitation
  uint32_t selectedNodeId;
  double explorationProb = 0.2;
  
  if (m_rng->GetValue(0.0, 1.0) < explorationProb) {
    // Exploration: uniform random selection
    int idx = static_cast<int>(m_rng->GetValue(0.0, static_cast<double>(candidates.size())));
    if (idx >= static_cast<int>(candidates.size())) {
      idx = static_cast<int>(candidates.size()) - 1;
    }
    selectedNodeId = candidates[idx];
    NS_LOG_INFO("LaunchAntsForKnownDestinations: EXPLORATION - selected destNode=" << selectedNodeId 
                << " (uniform random from " << candidates.size() << " candidates)");
  } else {
    // Exploitation: weighted selection based on flow
    double r = m_rng->GetValue(0.0, totalWeight);
    double acc = 0.0;
    size_t selectedIdx = candidates.size() - 1;
    for (size_t i = 0; i < candidates.size(); ++i) {
      acc += weights[i];
      if (r <= acc) {
        selectedIdx = i;
        break;
      }
    }
    selectedNodeId = candidates[selectedIdx];
    NS_LOG_INFO("LaunchAntsForKnownDestinations: EXPLOITATION - selected destNode=" << selectedNodeId 
                << " weight=" << weights[selectedIdx] 
                << " (total=" << candidates.size() << " candidates)");
  }
  
  // Get the primary address of the destination node
  Ipv4Address dstAddr = m_nodeIdToPrimaryAddress[selectedNodeId];
  SendForwardAnt(dstAddr);
}

void AntNetRoutingProtocol::SendForwardAnt(Ipv4Address dst) {
  auto neighbors = GetNeighborAddresses();
  if (neighbors.empty()) return;
  
  // Convert destination IP to node ID
  uint32_t dstNodeId;
  auto it = m_addressToNodeId.find(dst);
  if (it != m_addressToNodeId.end()) {
    dstNodeId = it->second;
  } else {
    // If mapping not found, try real-time resolution
    dstNodeId = ResolveNodeId(dst);
    if (dstNodeId == std::numeric_limits<uint32_t>::max()) {
      NS_LOG_WARN("Cannot resolve node ID for address " << dst);
      return;
    }
  }
  
  AntHeader h;
  h.SetType(ANT_FORWARD);
  h.SetSrc(GetPrimaryAddress());
  h.SetDst(dst);
  uint32_t srcNodeId = GetObject<Node>()->GetId();
  h.SetId(srcNodeId, dstNodeId, m_antSeq++);
  h.SetLaunchTime(Simulator::Now().GetSeconds());
  h.PushHop(GetPrimaryAddress());

  // Query pheromone table using node ID
  m_ph.EnsureDestNode(dstNodeId, neighbors);
  Ipv4Address nh = m_ph.SampleNextHopForNode(dstNodeId, m_betaAnt, m_rng->GetInteger(1, 0x7fffffff));
  if (nh == Ipv4Address()) return;

  Ptr<Packet> p = Create<Packet>();
  p->AddHeader(h);
  
  // Set priority for Forward Ant: same as data packets (priority 0 - Best Effort)
  // According to AntNet paper, Forward Ants share the same queue as data packets
  SocketPriorityTag priorityTag;
  priorityTag.SetPriority(0);  // Best Effort priority
  p->AddPacketTag(priorityTag);
  
  m_antSocket->SendTo(p, 0, InetSocketAddress(nh, m_antPort));
  NS_LOG_INFO("SendForwardAnt id=" << h.GetId() << " srcNode=" << srcNodeId 
              << " destNode=" << dstNodeId << " dst=" << dst << " nh=" << nh
              << " priority=0 (Best Effort, same as data packets)");
}

void AntNetRoutingProtocol::RecvAnt(Ptr<Socket> socket) {
  Address from;
  Ptr<Packet> p = socket->RecvFrom(from);
  InetSocketAddress isa = InetSocketAddress::ConvertFrom(from);
  Ipv4Address prev = isa.GetIpv4();

  AntHeader h;
  p->PeekHeader(h);
  NS_LOG_DEBUG("RecvAnt node=" << GetObject<Node>()->GetId() 
               << " from=" << prev << " id=" << h.GetId());
  
  if (p->RemoveHeader(h), true) {
    // Parse destination node ID
    uint32_t dstNodeId;
    auto it = m_addressToNodeId.find(h.GetDst());
    if (it != m_addressToNodeId.end()) {
      dstNodeId = it->second;
    } else {
      dstNodeId = ResolveNodeId(h.GetDst());
    }
    
    if (h.GetType() == ANT_FORWARD) { // ANT_FORWARD
      if (IsMyAddress(h.GetDst())) { // arrived at destination
        NS_LOG_INFO("FWD arrives at destNode=" << dstNodeId 
                    << " (node " << GetObject<Node>()->GetId() << ")"
                    << " dst=" << h.GetDst() << " -> turn BACKWARD id=" << h.GetId() 
                    << " pathLen=" << h.GetPath().size());
        h.SetType(ANT_BACKWARD);
        // The path contains all intermediate nodes but not the destination
        // The last element in path is the previous hop (sender of this packet)
        // We need to send back to that address
        Ipv4Address back;
        if (h.PopHop(back)) {
          NS_LOG_INFO("BWD start from dest, sending back to " << back 
                      << " pathLen=" << h.GetPath().size());
          Ptr<Packet> qq = Create<Packet>();
          qq->AddHeader(h);
          
          // Set high priority for Backward Ant (priority 7 - highest)
          // According to AntNet paper, Backward Ants have their own high-priority queue
          SocketPriorityTag priorityTag;
          priorityTag.SetPriority(7);  // Highest priority
          qq->AddPacketTag(priorityTag);
          
          m_antSocket->SendTo(qq, 0, InetSocketAddress(back, m_antPort));
          NS_LOG_INFO("BWD packet sent with priority=7 (high priority queue)");
        } else {
          NS_LOG_WARN("BWD failed at dest id=" << h.GetId() << " - no hop in path");
        }
        return;
      } else { // relay
        auto path = h.GetPath();
        
        // Check path length limit - more lenient for chain topologies
        if (!path.empty() && path.size() > 20) {
          NS_LOG_WARN("FWD dropped id=" << h.GetId() << " - path too long (" << path.size() << ")");
          return;
        }
        
        // AntNet paper's cycle detection and handling
        Ipv4Address myAddr = GetPrimaryAddress();
        int cycleStartIndex = -1;
        
        // Find current node in path to determine cycle start
        for (size_t i = 0; i < path.size(); ++i) {
          if (path[i] == myAddr) {
            cycleStartIndex = static_cast<int>(i);
            break;
          }
        }
        
        if (cycleStartIndex >= 0) {
          // Cycle detected
          size_t cycleLength = path.size() - cycleStartIndex;
          double antAge = Simulator::Now().GetSeconds() - h.GetLaunchTime();
          
          NS_LOG_INFO("FWD cycle detected id=" << h.GetId() 
                      << " at=" << myAddr 
                      << " cycleLen=" << cycleLength 
                      << " antAge=" << antAge
                      << " pathLen=" << path.size());
          
          // More lenient cycle handling: only destroy if path gets too long
          // or ant has been alive too long (indicating it's truly stuck)
          if (path.size() >= 12 || antAge > 1.0) {
            NS_LOG_WARN("FWD destroyed id=" << h.GetId() 
                        << " - path too long (" << path.size() 
                        << ") or ant too old (" << antAge << "s)");
            return;
          }
          
          // Otherwise, pop cycle nodes from stack and continue
          std::vector<Ipv4Address> newPath;
          for (int i = 0; i < cycleStartIndex; ++i) {
            newPath.push_back(path[i]);
          }
          h.SetPath(newPath);
          
          NS_LOG_INFO("FWD cycle removed id=" << h.GetId() 
                      << " - popped " << cycleLength << " nodes"
                      << " newPathLen=" << newPath.size());
        }
        
        h.PushHop(myAddr);
        auto nbs = GetNeighborAddresses();
        
        // Query pheromone table using node ID
        m_ph.EnsureDestNode(dstNodeId, nbs);
        Ipv4Address nh = m_ph.SampleNextHopForNode(dstNodeId, m_betaAnt, m_rng->GetInteger(1, 0x7fffffff));
        NS_LOG_INFO("FWD relay id=" << h.GetId() << " destNode=" << dstNodeId 
                    << " (node " << GetObject<Node>()->GetId() << ")"
                    << " dst=" << h.GetDst() << " this=" << myAddr << " next=" << nh
                    << " pathLen=" << h.GetPath().size() << " prev=" << prev);
        if (nh == Ipv4Address()) {
          NS_LOG_WARN("FWD dropped id=" << h.GetId() << " destNode=" << dstNodeId 
                      << " - no next hop available");
          return;
        }
        Ptr<Packet> q = Create<Packet>();
        q->AddHeader(h);
        
        // Set priority for Forward Ant: same as data packets (priority 0 - Best Effort)
        SocketPriorityTag priorityTag;
        priorityTag.SetPriority(0);  // Best Effort priority
        q->AddPacketTag(priorityTag);
        
        int32_t sent = m_antSocket->SendTo(q, 0, InetSocketAddress(nh, m_antPort));
        if (sent < 0) {
          NS_LOG_WARN("FWD send failed id=" << h.GetId() << " to=" << nh 
                      << " error=" << m_antSocket->GetErrno());
        }
      }
    } else { // ANT_BACKWARD
      double T = Simulator::Now().GetSeconds() - h.GetLaunchTime();
      const auto& path = h.GetPath();
      Ipv4Address myAddr = GetPrimaryAddress();
      NS_LOG_INFO("BWD id=" << h.GetId() << " destNode=" << dstNodeId 
                  << " (node " << GetObject<Node>()->GetId() << ")"
                  << " dst=" << h.GetDst() << " T=" << T 
                  << " pathLen=" << path.size() << " at=" << myAddr 
                  << " prev=" << prev);
      
      // Update statistics using node ID
      m_ph.ObserveRttForNode(dstNodeId, T, m_eta);
      double r = m_ph.GetReinforcementForNode(dstNodeId, T);
      auto nbs = GetNeighborAddresses();
      
      // Convert prev (possibly a primary address) to link-local neighbor address
      Ipv4Address actualNeighbor = prev;
      // Check if prev is a primary address; if so, find the corresponding link-local address
      auto it_nodeId = m_addressToNodeId.find(prev);
      if (it_nodeId != m_addressToNodeId.end()) {
        uint32_t senderNodeId = it_nodeId->second;
        // Find address in neighbor list that belongs to the same node
        for (const auto& nb : nbs) {
          auto it_nb = m_addressToNodeId.find(nb);
          if (it_nb != m_addressToNodeId.end() && it_nb->second == senderNodeId) {
            actualNeighbor = nb;
            NS_LOG_DEBUG("Converted prev=" << prev << " (node " << senderNodeId 
                        << ") to neighbor=" << actualNeighbor);
            break;
          }
        }
      }
      
      // Reinforce using the actual neighbor address
      m_ph.ReinforceNode(dstNodeId, actualNeighbor, r, m_alphaLearn, nbs);
      
      // Check if we've reached the source node (ant originator)
      uint32_t myNodeId = GetObject<Node>()->GetId();
      uint32_t srcNodeId = h.GetId().srcNodeId;
      
      if (myNodeId == srcNodeId) {
        // We are the source node - ant has completed its round trip
        NS_LOG_INFO("BWD reached source id=" << h.GetId() << " at node " << myNodeId
                    << " - ant completed successfully");
      } else {
        // Not the source - continue forwarding backward
        Ipv4Address back;
        if (h.PopHop(back)) {
          NS_LOG_INFO("BWD id=" << h.GetId() << " destNode=" << dstNodeId 
                      << " (node " << myNodeId << ")"
                      << " dst=" << h.GetDst() << " T=" << T 
                      << " pathLen=" << h.GetPath().size()
                      << " at=" << GetPrimaryAddress() << " prev=" << prev
                      << " sendingTo=" << back);
          Ptr<Packet> q = Create<Packet>();
          q->AddHeader(h);
          
          // Set high priority for Backward Ant (priority 7 - highest)
          // According to AntNet paper, Backward Ants have their own high-priority queue
          SocketPriorityTag priorityTag;
          priorityTag.SetPriority(7);  // Highest priority
          q->AddPacketTag(priorityTag);
          
          int32_t sent = m_antSocket->SendTo(q, 0, InetSocketAddress(back, m_antPort));
          if (sent < 0) {
            NS_LOG_WARN("BWD send failed id=" << h.GetId() << " to=" << back);
          }
        } else {
          NS_LOG_WARN("BWD PopHop failed id=" << h.GetId() << " at node " << myNodeId
                      << " - empty path but not source node (srcNode=" << srcNodeId << ")");
        }
      }
    }
  }
}

Ptr<Ipv4Route> AntNetRoutingProtocol::BuildRoute(Ipv4Address dest, Ipv4Address nextHop) const {
  int32_t ifIndex = FindInterfaceForNextHop(nextHop);
  if (ifIndex < 0) return nullptr;
  Ipv4InterfaceAddress ifaddr = m_ipv4->GetAddress(ifIndex, 0);
  Ptr<Ipv4Route> rt = Create<Ipv4Route>();
  rt->SetDestination(dest);
  rt->SetGateway(nextHop);
  rt->SetSource(ifaddr.GetLocal());
  rt->SetOutputDevice(m_ipv4->GetNetDevice(ifIndex));
  return rt;
}

Ptr<Ipv4Route> AntNetRoutingProtocol::RouteOutput(Ptr<Packet> p, const Ipv4Header& header,
                             Ptr<NetDevice> oif, Socket::SocketErrno& sockerr) {
  Ipv4Address dst = header.GetDestination();
  if (IsMyAddress(dst)) { sockerr = Socket::ERROR_NOROUTETOHOST; return nullptr; }
  
  // Convert destination IP to node ID
  uint32_t dstNodeId;
  auto it = m_addressToNodeId.find(dst);
  if (it != m_addressToNodeId.end()) {
    dstNodeId = it->second;
  } else {
    // Unknown destination, try real-time resolution
    dstNodeId = ResolveNodeId(dst);
    if (dstNodeId == std::numeric_limits<uint32_t>::max()) {
      // Cannot resolve, fall back to old method
      m_knownDestinations.insert(dst);
      auto nbs = GetNeighborAddresses();
      m_ph.EnsureDest(dst, nbs);
      Ipv4Address nh = m_ph.SampleNextHop(dst, m_betaData, m_rng->GetInteger(1, 0x7fffffff));
      Ptr<Ipv4Route> rt = BuildRoute(dst, nh);
      if (rt) {
        if (p) m_ph.AccumulateFlow(dst, static_cast<double>(p->GetSize()));
        sockerr = Socket::ERROR_NOTERROR;
        return rt;
      }
      sockerr = Socket::ERROR_NOROUTETOHOST;
      return nullptr;
    }
    m_addressToNodeId[dst] = dstNodeId;
    m_knownDestinationNodes.insert(dstNodeId);
  }
  
  // Route using node ID
  auto nbs = GetNeighborAddresses();
  m_ph.EnsureDestNode(dstNodeId, nbs);
  Ipv4Address nh = m_ph.SampleNextHopForNode(dstNodeId, m_betaData, m_rng->GetInteger(1, 0x7fffffff));
  // NS_LOG_INFO("RouteOutput destNode=" << dstNodeId << " dst=" << dst << " nh=" << nh);
  Ptr<Ipv4Route> rt = BuildRoute(dst, nh);
  if (rt) {
    if (p) {
      m_ph.AccumulateFlowForNode(dstNodeId, static_cast<double>(p->GetSize()));
    }
    sockerr = Socket::ERROR_NOTERROR;
    return rt;
  }
  sockerr = Socket::ERROR_NOROUTETOHOST;
  return nullptr;
}

bool AntNetRoutingProtocol::RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                  const UnicastForwardCallback &ucb, const MulticastForwardCallback &mcb,
                  const LocalDeliverCallback &lcb, const ErrorCallback &ecb) {
  Ipv4Address dst = header.GetDestination();
  if (IsMyAddress(dst)) {
    NS_LOG_INFO("LocalDeliver dst=" << dst);
    if (!lcb.IsNull()) {
      int32_t iif = m_ipv4->GetInterfaceForDevice(idev);
      uint32_t iface = (iif >= 0) ? static_cast<uint32_t>(iif) : 0u;
      lcb(p, header, iface);
      return true;
    }
    return false;
  }
  
  // Convert destination IP to node ID
  uint32_t dstNodeId;
  auto it = m_addressToNodeId.find(dst);
  if (it != m_addressToNodeId.end()) {
    dstNodeId = it->second;
  } else {
    // Unknown destination, try real-time resolution
    dstNodeId = ResolveNodeId(dst);
    if (dstNodeId == std::numeric_limits<uint32_t>::max()) {
      // Cannot resolve, fall back to old method
      m_knownDestinations.insert(dst);
      auto nbs = GetNeighborAddresses();
      m_ph.EnsureDest(dst, nbs);
      Ipv4Address nh = m_ph.SampleNextHop(dst, m_betaData, m_rng->GetInteger(1, 0x7fffffff));
      Ptr<Ipv4Route> rt = BuildRoute(dst, nh);
      if (rt) {
        m_ph.AccumulateFlow(dst, static_cast<double>(p->GetSize()));
        if (!ucb.IsNull()) { ucb(rt, p, header); return true; }
      }
      if (!ecb.IsNull()) ecb(p, header, Socket::ERROR_NOROUTETOHOST);
      return false;
    }
    m_addressToNodeId[dst] = dstNodeId;
    m_knownDestinationNodes.insert(dstNodeId);
  }
  
  // Route using node ID
  auto nbs = GetNeighborAddresses();
  m_ph.EnsureDestNode(dstNodeId, nbs);
  Ipv4Address nh = m_ph.SampleNextHopForNode(dstNodeId, m_betaData, m_rng->GetInteger(1, 0x7fffffff));
  Ptr<Ipv4Route> rt = BuildRoute(dst, nh);
  if (rt) {
    m_ph.AccumulateFlowForNode(dstNodeId, static_cast<double>(p->GetSize()));
    if (!ucb.IsNull()) { ucb(rt, p, header); return true; }
  }
  if (!ecb.IsNull()) ecb(p, header, Socket::ERROR_NOROUTETOHOST);
  return false;
}

void AntNetRoutingProtocol::NotifyInterfaceUp(uint32_t interface) {}
void AntNetRoutingProtocol::NotifyInterfaceDown(uint32_t interface) {}
void AntNetRoutingProtocol::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) {}
void AntNetRoutingProtocol::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) {}

void AntNetRoutingProtocol::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const {
  *stream->GetStream() << "Node " << GetObject<Node>()->GetId() << " AntNet P-table\n";
}

bool AntNetRoutingProtocol::IsMyAddress(Ipv4Address a) const {
  for (uint32_t i=0; i<m_ipv4->GetNInterfaces(); ++i) {
    for (uint32_t j=0; j<m_ipv4->GetNAddresses(i); ++j) {
      if (m_ipv4->GetAddress(i,j).GetLocal() == a) return true;
    }
  }
  return false;
}

Ipv4Address AntNetRoutingProtocol::GetPrimaryAddress() const {
  for (uint32_t i=0; i<m_ipv4->GetNInterfaces(); ++i) {
    for (uint32_t j=0; j<m_ipv4->GetNAddresses(i); ++j) {
      Ipv4InterfaceAddress ifaddr = m_ipv4->GetAddress(i,j);
      if (ifaddr.GetMask() != Ipv4Mask::GetZero() && ifaddr.GetLocal() != Ipv4Address::GetLoopback()) {
        return ifaddr.GetLocal();
      }
    }
  }
  return Ipv4Address("0.0.0.0");
}

int32_t AntNetRoutingProtocol::FindInterfaceForAddress(Ipv4Address a) const {
  for (uint32_t i=0; i<m_ipv4->GetNInterfaces(); ++i) {
    for (uint32_t j=0; j<m_ipv4->GetNAddresses(i); ++j) {
      if (m_ipv4->GetAddress(i,j).GetLocal() == a) return static_cast<int32_t>(i);
    }
  }
  return -1;
}

int32_t AntNetRoutingProtocol::FindInterfaceForNextHop(Ipv4Address nh) const {
  for (uint32_t i=0; i<m_ipv4->GetNInterfaces(); ++i) {
    for (uint32_t j=0; j<m_ipv4->GetNAddresses(i); ++j) {
      Ipv4InterfaceAddress ifaddr = m_ipv4->GetAddress(i,j);
      Ipv4Mask m = ifaddr.GetMask();
      if (m == Ipv4Mask::GetZero()) continue;
      if ( (ifaddr.GetLocal().CombineMask(m)).Get() == (nh.CombineMask(m)).Get() ) {
        return static_cast<int32_t>(i);
      }
    }
  }
  return -1;
}

uint32_t AntNetRoutingProtocol::ResolveNodeId(Ipv4Address addr) const {
  for (NodeList::Iterator it = NodeList::Begin(); it != NodeList::End(); ++it) {
    Ptr<Node> node = *it;
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    if (ipv4 == nullptr) {
      continue;
    }
    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i) {
      for (uint32_t j = 0; j < ipv4->GetNAddresses(i); ++j) {
        if (ipv4->GetAddress(i, j).GetLocal() == addr) {
          return node->GetId();
        }
      }
    }
  }
  return std::numeric_limits<uint32_t>::max();
}

void AntNetRoutingProtocol::AddStaticNeighbor(Ipv4Address neighbor) {
  NeighborInfo &info = m_neighbors[neighbor];
  info.lastSeen = Simulator::Now();
  info.isStatic = true;
  NS_LOG_INFO("AddStaticNeighbor neighbor=" << neighbor);
}

std::vector<Ipv4Address> AntNetRoutingProtocol::GetNeighborAddresses() const {
  // NS_LOG_INFO("GetNeighborAddresses: nNeighbors=" << m_neighbors.size());
  std::vector<Ipv4Address> nbs;
  nbs.reserve(m_neighbors.size());
  for (auto const& kv : m_neighbors) {
    nbs.push_back(kv.first);
  }
  return nbs;
}

void AntNetRoutingProtocol::DumpPheromoneTable() const {
  NS_LOG_INFO("Node " << GetObject<Node>()->GetId()
                       << " PheromoneTable snapshot\n" << m_ph.DebugString(&m_addressToNodeId));
}

void AntNetRoutingProtocol::DiscoverAllNodes() {
  NS_LOG_INFO("DiscoverAllNodes on node=" << GetObject<Node>()->GetId());
  
  uint32_t myNodeId = GetObject<Node>()->GetId();
  
  for (NodeList::Iterator it = NodeList::Begin(); it != NodeList::End(); ++it) {
    Ptr<Node> node = *it;
    uint32_t nodeId = node->GetId();
    
    // Skip self
    if (nodeId == myNodeId) continue;
    
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    if (ipv4 == nullptr) continue;
    
    Ipv4Address primaryAddr;
    
    // Collect all addresses of the node and select primary address
    for (uint32_t i = 1; i < ipv4->GetNInterfaces(); ++i) {  // Skip loopback interface
      if (ipv4->GetNAddresses(i) > 0) {
        Ipv4Address addr = ipv4->GetAddress(i, 0).GetLocal();
        
        // Use first non-loopback address as primary address
        if (primaryAddr == Ipv4Address()) {
          primaryAddr = addr;
          m_nodeIdToPrimaryAddress[nodeId] = addr;
          m_knownDestinationNodes.insert(nodeId);
          NS_LOG_INFO("  Node " << nodeId << " primary address: " << addr);
        }
        
        // Establish reverse mapping: all addresses map to node ID
        m_addressToNodeId[addr] = nodeId;
        NS_LOG_INFO("    Node " << nodeId << " address: " << addr);
      }
    }
  }
  
  NS_LOG_INFO("Total known destination nodes: " << m_knownDestinationNodes.size());
}

void AntNetRoutingProtocol::ApplyPheromoneDecay() {
  if (!m_running) return;
  
  m_ph.DecayPheromones(m_decayFactor);
  NS_LOG_INFO("Applied pheromone decay with factor " << m_decayFactor);
  
  m_decayEvent = Simulator::Schedule(m_decayPeriod, &AntNetRoutingProtocol::ApplyPheromoneDecay, this);
}

} // namespace ns3
