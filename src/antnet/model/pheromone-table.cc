#include "ns3/pheromone-table.h"
#include "ns3/log.h"
#include <cmath>
#include <algorithm>
#include <random>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("PheromoneTable");

void PheromoneTable::EnsureDest(Ipv4Address dest, const std::vector<Ipv4Address>& neighbors) {
  auto &bucket = m_tbl[Key(dest)];
  if (bucket.empty()) {
    if (neighbors.empty()) return;
    double u = 1.0 / neighbors.size();
    bucket.reserve(neighbors.size());
    for (auto const& nh : neighbors) bucket.push_back({nh, u});
  } else {
    for (auto const& nh : neighbors) {
      bool found = false;
      for (auto &e : bucket) if (e.nh == nh) { found = true; break; }
      if (!found) bucket.push_back({nh, 1e-6});
    }
    Normalize(bucket);
  }
}

Ipv4Address PheromoneTable::SampleNextHop(Ipv4Address dest, double beta, uint32_t seed) const {
  auto it = m_tbl.find(Key(dest));
  if (it == m_tbl.end() || it->second.empty()) return Ipv4Address(); // invalid
  std::vector<double> w; w.reserve(it->second.size());
  double sum = 0.0;
  for (auto const& e : it->second) {
    double val = std::pow(std::max(e.p, 1e-12), beta);
    w.push_back(val); sum += val;
  }
  if (sum <= 0) return it->second.front().nh;
  std::mt19937 rng(seed);
  std::uniform_real_distribution<> U(0.0, sum);
  double r = U(rng);
  double acc = 0.0;
  for (size_t i=0;i<w.size();++i) {
    acc += w[i];
    if (r <= acc) return it->second[i].nh;
  }
  return it->second.back().nh;
}

void PheromoneTable::Reinforce(Ipv4Address dest, Ipv4Address fromPrevHop, double r, double alpha,
                               const std::vector<Ipv4Address>& neighbors) {
  auto &bucket = m_tbl[Key(dest)];
  if (bucket.empty()) {
    EnsureDest(dest, neighbors);
  }
  if (bucket.empty()) return;
  double rr = std::max(0.0, std::min(1.0, r)) * std::max(0.0, std::min(1.0, alpha));
  for (auto &e : bucket) {
    if (e.nh == fromPrevHop) {
      e.p = e.p + rr * (1.0 - e.p);
    } else {
      e.p = e.p - rr * e.p;
    }
  }
  Normalize(bucket);
  NS_LOG_INFO("Reinforce dest=" << dest << " via=" << fromPrevHop << " r=" << r << " alpha=" << alpha);
}

const std::vector<NextHopEntry>* PheromoneTable::GetBucket(Ipv4Address dest) const {
  auto it = m_tbl.find(Key(dest));
  if (it == m_tbl.end()) return nullptr;
  return &it->second;
}

void PheromoneTable::Normalize(std::vector<NextHopEntry>& v) const {
  double s = 0.0;
  for (auto const& e : v) s += e.p;
  if (s <= 0) {
    double u = 1.0 / v.size();
    for (auto &e : v) e.p = u;
  } else {
    for (auto &e : v) e.p /= s;
  }
}

void PheromoneTable::ObserveRtt(Ipv4Address dest, double T, double eta) {
  auto &st = m_stats[Key(dest)];
  if (st.wcount == 0) {
    st.mu = T; st.sigma2 = 0.0; st.wbest = T; st.wcount = 1;
    return;
  }
  double mu_old = st.mu;
  st.mu += eta * (T - st.mu);
  st.sigma2 += eta * (((T - mu_old)*(T - mu_old)) - st.sigma2);
  if (T < st.wbest) st.wbest = T;
  if (st.wcount < 1000000000u) st.wcount++;
  NS_LOG_INFO("ObserveRtt dest=" << dest << " T=" << T << " mu=" << st.mu << " best=" << st.wbest);
}

double PheromoneTable::GetReinforcement(Ipv4Address dest, double T) const {
  auto it = m_stats.find(Key(dest));
  if (it == m_stats.end()) return 0.5;
  const auto &st = it->second;
  if (T <= 0) return 1.0;
  double r1 = st.wbest / T;
  double denom = (st.mu - st.wbest) + (T - st.wbest) + 1e-9;
  double r2 = (st.mu - st.wbest) / denom;
  double r = 0.7 * r1 + 0.3 * r2;
  double s = 1.0 / (1.0 + std::exp(-6.0*(r-0.5)));
  return s;
}

} // namespace ns3
