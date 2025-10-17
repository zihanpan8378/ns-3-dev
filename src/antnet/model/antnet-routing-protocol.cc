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
                  TimeValue(Seconds(1.0)),
                  MakeTimeAccessor(&AntNetRoutingProtocol::m_antPeriod),
                  MakeTimeChecker())
    .AddAttribute("BetaAnt", "Exponent for ant next-hop sampling",
                  DoubleValue(1.0),
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
                  MakeDoubleChecker<double>());
  return tid;
}

AntNetRoutingProtocol::AntNetRoutingProtocol()
  : m_running(false),
    m_ipv4(nullptr),
    m_antSocket(nullptr),
    m_helloSocket(nullptr),
    m_antPort(5001),
    m_helloPort(5002),
    m_betaAnt(1.0),
    m_betaData(1.3),
    m_alphaLearn(0.4),
    m_eta(0.1),
    m_phi(1.2),
    m_helloPeriod(Seconds(1.0)),
    m_neighborTimeout(Seconds(3.0)),
    m_antPeriod(Seconds(1.0)),
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
  m_helloEvent = Simulator::Schedule(Seconds(1), &AntNetRoutingProtocol::SendHello, this);
  m_antEvent = Simulator::Schedule(Seconds(5), &AntNetRoutingProtocol::ScheduleAnt, this);
}

void AntNetRoutingProtocol::Stop() {
  if (!m_running) return;
  m_running = false;
  if (m_antSocket) { m_antSocket->Close(); m_antSocket = nullptr; }
  if (m_helloSocket) { m_helloSocket->Close(); m_helloSocket = nullptr; }
  if (m_helloEvent.IsPending()) m_helloEvent.Cancel();
  if (m_antEvent.IsPending()) m_antEvent.Cancel();
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
  NS_LOG_INFO("SendHello: nIf=" << m_ipv4->GetNInterfaces());
  Ptr<Packet> p = Create<Packet>(1);
  for (uint32_t i=0; i<m_ipv4->GetNInterfaces(); ++i) {
    for (uint32_t j=0; j<m_ipv4->GetNAddresses(i); ++j) {
      Ipv4InterfaceAddress ifaddr = m_ipv4->GetAddress(i, j);
      if (ifaddr.GetMask() == Ipv4Mask::GetZero()) continue;
      Ipv4Address bcast = ifaddr.GetBroadcast();
      m_helloSocket->SendTo(p->Copy(), 0, InetSocketAddress(bcast, m_helloPort));
    }
  }
  Time now = Simulator::Now();
  std::vector<Ipv4Address> toErase;
  for (auto const& kv : m_neighbors) {
    if (now - kv.second > m_neighborTimeout) toErase.push_back(kv.first);
  }
  for (auto const& a : toErase) m_neighbors.erase(a);
  m_helloEvent = Simulator::Schedule(m_helloPeriod, &AntNetRoutingProtocol::SendHello, this);
}

void AntNetRoutingProtocol::RecvHello(Ptr<Socket> socket) {
  Address from;
  Ptr<Packet> p = socket->RecvFrom(from);
  InetSocketAddress isa = InetSocketAddress::ConvertFrom(from);
  Ipv4Address peer = isa.GetIpv4();
  m_neighbors[peer] = Simulator::Now();
  NS_LOG_INFO("RecvHello from=" << peer);
}

void AntNetRoutingProtocol::ScheduleAnt() {
  LaunchAntsForKnownDestinations();
  m_antEvent = Simulator::Schedule(m_antPeriod, &AntNetRoutingProtocol::ScheduleAnt, this);
}

void AntNetRoutingProtocol::LaunchAntsForKnownDestinations() {
  for (auto const& d : m_knownDestinations) {
    if (!IsMyAddress(d)) SendForwardAnt(d);
  }
}

void AntNetRoutingProtocol::SendForwardAnt(Ipv4Address dst) {
  if (m_neighbors.empty()) return;
  AntHeader h;
  h.SetType(ANT_FORWARD);
  h.SetSrc(GetPrimaryAddress());
  h.SetDst(dst);
  h.SetId(m_antSeq++);
  h.SetLaunchTime(Simulator::Now().GetSeconds());
  h.PushHop(GetPrimaryAddress());

  std::vector<Ipv4Address> nbs;
  nbs.reserve(m_neighbors.size());
  for (auto const& kv : m_neighbors) nbs.push_back(kv.first);
  m_ph.EnsureDest(dst, nbs);
  Ipv4Address nh = m_ph.SampleNextHop(dst, m_betaAnt, m_rng->GetInteger(1, 0x7fffffff));
  if (nh == Ipv4Address()) return;

  Ptr<Packet> p = Create<Packet>();
  p->AddHeader(h);
  m_antSocket->SendTo(p, 0, InetSocketAddress(nh, m_antPort));
  NS_LOG_INFO("SendForwardAnt id=" << h.GetId() << " dst=" << dst << " nh=" << nh);
}

void AntNetRoutingProtocol::RecvAnt(Ptr<Socket> socket) {
  Address from;
  Ptr<Packet> p = socket->RecvFrom(from);
  InetSocketAddress isa = InetSocketAddress::ConvertFrom(from);
  Ipv4Address prev = isa.GetIpv4();

  AntHeader h;
  if (p->PeekHeader(h), p->RemoveHeader(h), true) {
    if (h.GetType() == ANT_FORWARD) { // ANT_FORWARD
      if (IsMyAddress(h.GetDst())) { // arrived at destination
        NS_LOG_INFO("FWD arrives at dst=" << h.GetDst() << " -> turn BACKWARD id=" << h.GetId());
        h.SetType(ANT_BACKWARD);
        h.PushHop(GetPrimaryAddress());
        Ipv4Address back;
        if (h.PopHop(back) && h.PopHop(back)) {
          Ptr<Packet> qq = Create<Packet>();
          qq->AddHeader(h);
          m_antSocket->SendTo(qq, 0, InetSocketAddress(back, m_antPort));
        }
        return;
      } else { // relay
        auto path = h.GetPath();
        if (!path.empty() && path.size() > 16) { return; }
        h.PushHop(GetPrimaryAddress());
        std::vector<Ipv4Address> nbs;
        for (auto const& kv : m_neighbors) nbs.push_back(kv.first);
        m_ph.EnsureDest(h.GetDst(), nbs);
        Ipv4Address nh = m_ph.SampleNextHop(h.GetDst(), m_betaAnt, m_rng->GetInteger(1, 0x7fffffff));
        NS_LOG_INFO("FWD relay id=" << h.GetId() << " dst=" << h.GetDst() << " next=" << nh);
        if (nh == Ipv4Address()) return;
        Ptr<Packet> q = Create<Packet>();
        q->AddHeader(h);
        m_antSocket->SendTo(q, 0, InetSocketAddress(nh, m_antPort));
      }
    } else { // ANT_BACKWARD
      double T = Simulator::Now().GetSeconds() - h.GetLaunchTime();
      NS_LOG_INFO("BWD id=" << h.GetId() << " dst=" << h.GetDst() << " T=" << T);
      m_ph.ObserveRtt(h.GetDst(), T, m_eta);
      double r = m_ph.GetReinforcement(h.GetDst(), T);
      std::vector<Ipv4Address> nbs;
      for (auto const& kv : m_neighbors) nbs.push_back(kv.first);
      m_ph.Reinforce(h.GetDst(), prev, r, m_alphaLearn, nbs);
      Ipv4Address back;
      if (h.PopHop(back) && h.PopHop(back)) {
        Ptr<Packet> q = Create<Packet>();
        q->AddHeader(h);
        m_antSocket->SendTo(q, 0, InetSocketAddress(back, m_antPort));
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
  m_knownDestinations.insert(dst);
  std::vector<Ipv4Address> nbs;
  for (auto const& kv : m_neighbors) nbs.push_back(kv.first);
  m_ph.EnsureDest(dst, nbs);
  Ipv4Address nh = m_ph.SampleNextHop(dst, m_betaData, m_rng->GetInteger(1, 0x7fffffff));
  NS_LOG_INFO("RouteOutput dst=" << dst << " nh=" << nh);
  Ptr<Ipv4Route> rt = BuildRoute(dst, nh);
  if (rt) { sockerr = Socket::ERROR_NOTERROR; return rt; }
  sockerr = Socket::ERROR_NOROUTETOHOST; return nullptr;
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
  m_knownDestinations.insert(dst);
  std::vector<Ipv4Address> nbs;
  for (auto const& kv : m_neighbors) nbs.push_back(kv.first);
  m_ph.EnsureDest(dst, nbs);
  Ipv4Address nh = m_ph.SampleNextHop(dst, m_betaData, m_rng->GetInteger(1, 0x7fffffff));
  Ptr<Ipv4Route> rt = BuildRoute(dst, nh);
  if (rt && !ucb.IsNull()) { ucb(rt, p, header); return true; }
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
      if (ifaddr.GetMask() != Ipv4Mask::GetZero()) return ifaddr.GetLocal();
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

} // namespace ns3
