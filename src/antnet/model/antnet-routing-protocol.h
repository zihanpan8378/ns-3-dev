#ifndef ANNET_ROUTING_PROTOCOL_H
#define ANNET_ROUTING_PROTOCOL_H

#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4.h"
#include "ns3/socket.h"
#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"
#include "ns3/event-id.h"
#include "ns3/pheromone-table.h"
#include "ns3/ant-headers.h"
#include <set>
#include <map>

namespace ns3 {

class AntNetRoutingProtocol : public Ipv4RoutingProtocol
{
public:
  static TypeId GetTypeId();
  AntNetRoutingProtocol();
  virtual ~AntNetRoutingProtocol();

  Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p, const Ipv4Header &header,
                             Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) override;

  bool RouteInput(Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                  const UnicastForwardCallback &ucb, const MulticastForwardCallback &mcb,
                  const LocalDeliverCallback &lcb, const ErrorCallback &ecb) override;

  void NotifyInterfaceUp(uint32_t interface) override;
  void NotifyInterfaceDown(uint32_t interface) override;
  void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
  void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) override;
  void SetIpv4(Ptr<Ipv4> ipv4) override;
  void PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit = Time::S) const override;

private:
  void Start();
  void Stop();

  void CreateSockets();
  void SendHello();
  void RecvHello(Ptr<Socket> socket);
  void SendForwardAnt(Ipv4Address dst);
  void RecvAnt(Ptr<Socket> socket);
  void ScheduleAnt();
  void LaunchAntsForKnownDestinations();

  bool IsMyAddress(Ipv4Address a) const;
  Ipv4Address GetPrimaryAddress() const;
  int32_t FindInterfaceForAddress(Ipv4Address a) const;
  int32_t FindInterfaceForNextHop(Ipv4Address nh) const;
  Ptr<Ipv4Route> BuildRoute(Ipv4Address dest, Ipv4Address nextHop) const;

  bool m_running;

  Ptr<Ipv4> m_ipv4;
  Ptr<Socket> m_antSocket;
  Ptr<Socket> m_helloSocket;
  EventId m_helloEvent;
  EventId m_antEvent;

  uint16_t m_antPort;
  uint16_t m_helloPort;
  double m_betaAnt;
  double m_betaData;
  double m_alphaLearn;
  double m_eta;
  double m_phi;
  Time m_helloPeriod;
  Time m_neighborTimeout;
  Time m_antPeriod;

  std::map<Ipv4Address, Time> m_neighbors;
  std::set<Ipv4Address> m_knownDestinations;

  PheromoneTable m_ph;
  uint32_t m_antSeq;
  Ptr<UniformRandomVariable> m_rng;
};

} // namespace ns3

#endif // ANNET_ROUTING_PROTOCOL_H
