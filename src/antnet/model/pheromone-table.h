#ifndef PHEROMONE_TABLE_H
#define PHEROMONE_TABLE_H

#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include <string>
#include <map>
#include <unordered_map>
#include <vector>

namespace ns3 {

struct NextHopEntry {
  Ipv4Address nh;
  double p; // probability mass in [0,1]
};

struct LocalStats {
  double mu = 0.0;
  double sigma2 = 0.0;
  double wbest = 1e12; // best observed RTT
  uint32_t wcount = 0;
  double flow = 0.0;
};

class PheromoneTable {
public:
  // Node ID based API
  void EnsureDestNode(uint32_t destNodeId, const std::vector<Ipv4Address>& neighbors);
  Ipv4Address SampleNextHopForNode(uint32_t destNodeId, double beta, uint32_t seed) const;
  void ReinforceNode(uint32_t destNodeId, Ipv4Address fromPrevHop, double r, double alpha,
                     const std::vector<Ipv4Address>& neighbors);
  void ObserveRttForNode(uint32_t destNodeId, double T, double eta);
  double GetReinforcementForNode(uint32_t destNodeId, double T) const;
  void AccumulateFlowForNode(uint32_t destNodeId, double amount);
  double GetFlowWeightForNode(uint32_t destNodeId) const;
  
  void DecayPheromones(double factor);
  
  // Legacy IP-based API
  void EnsureDest(Ipv4Address dest, const std::vector<Ipv4Address>& neighbors);
  Ipv4Address SampleNextHop(Ipv4Address dest, double beta, uint32_t seed) const;
  void Reinforce(Ipv4Address dest, Ipv4Address fromPrevHop, double r, double alpha,
                 const std::vector<Ipv4Address>& neighbors);

  const std::vector<NextHopEntry>* GetBucket(Ipv4Address dest) const;
  const std::vector<NextHopEntry>* GetBucketForNode(uint32_t destNodeId) const;

  void ObserveRtt(Ipv4Address dest, double T, double eta);
  double GetReinforcement(Ipv4Address dest, double T) const;
  void AccumulateFlow(Ipv4Address dest, double amount);
  double GetFlowWeight(Ipv4Address dest) const;
  std::string DebugString(const std::map<Ipv4Address, uint32_t>* addressToNodeId = nullptr) const;

private:
  // Node ID based storage
  std::unordered_map<uint32_t, std::vector<NextHopEntry>> m_tblByNode;
  std::unordered_map<uint32_t, LocalStats> m_statsByNode;
  
  // Legacy IP-based storage
  std::unordered_map<uint32_t, std::vector<NextHopEntry>> m_tbl;
  std::unordered_map<uint32_t, LocalStats> m_stats;

  static uint32_t Key(Ipv4Address a) { return a.Get(); }
  void Normalize(std::vector<NextHopEntry>& v) const;
};

} // namespace ns3

#endif // PHEROMONE_TABLE_H
