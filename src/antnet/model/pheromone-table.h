#ifndef PHEROMONE_TABLE_H
#define PHEROMONE_TABLE_H

#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
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
};

class PheromoneTable {
public:
  void EnsureDest(Ipv4Address dest, const std::vector<Ipv4Address>& neighbors);
  Ipv4Address SampleNextHop(Ipv4Address dest, double beta, uint32_t seed) const;
  void Reinforce(Ipv4Address dest, Ipv4Address fromPrevHop, double r, double alpha,
                 const std::vector<Ipv4Address>& neighbors);

  const std::vector<NextHopEntry>* GetBucket(Ipv4Address dest) const;

  void ObserveRtt(Ipv4Address dest, double T, double eta);
  double GetReinforcement(Ipv4Address dest, double T) const;

private:
  std::unordered_map<uint32_t, std::vector<NextHopEntry>> m_tbl;
  std::unordered_map<uint32_t, LocalStats> m_stats;

  static uint32_t Key(Ipv4Address a) { return a.Get(); }
  void Normalize(std::vector<NextHopEntry>& v) const;
};

} // namespace ns3

#endif // PHEROMONE_TABLE_H
