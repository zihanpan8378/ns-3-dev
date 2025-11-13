#include "ns3/pheromone-table.h"
#include "ns3/log.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

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
  // Log table sizes
  NS_LOG_INFO("m_tbl size=" << m_tbl.size() << " m_stats size=" << m_stats.size());

  // Log pheromone bucket for this destination
  {
    auto it = m_tbl.find(Key(dest));
    if (it != m_tbl.end()) {
      NS_LOG_INFO("m_tbl dest=" << dest << " entries=" << it->second.size());
      for (const auto &e : it->second) {
        NS_LOG_INFO("  nh=" << e.nh << " p=" << e.p);
      }
    } else {
      NS_LOG_INFO("m_tbl dest=" << dest << " not found");
    }
  }

  // Log stats for this destination
  {
    auto it = m_stats.find(Key(dest));
    if (it != m_stats.end()) {
      const auto &st = it->second;
      NS_LOG_INFO("m_stats dest=" << dest
                   << " mu=" << st.mu
                   << " sigma2=" << st.sigma2
                   << " best=" << st.wbest
                   << " wcount=" << st.wcount
                   << " flow=" << st.flow);
    } else {
      NS_LOG_INFO("m_stats dest=" << dest << " not found");
    }
  }
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

void PheromoneTable::AccumulateFlow(Ipv4Address dest, double amount) {
  if (amount <= 0.0) {
    return;
  }
  auto &st = m_stats[Key(dest)];
  st.flow += amount;
}

double PheromoneTable::GetFlowWeight(Ipv4Address dest) const {
  auto it = m_stats.find(Key(dest));
  if (it == m_stats.end()) {
    return 0.0;
  }
  return it->second.flow;
}

std::string PheromoneTable::DebugString(const std::map<Ipv4Address, uint32_t>* addressToNodeId) const {
  std::ostringstream oss;
  oss.setf(std::ios::fixed);
  oss << std::setprecision(6);
  
  // Lambda to convert address to node ID string if mapping is available
  auto addrToString = [addressToNodeId](const Ipv4Address& addr) -> std::string {
    if (addressToNodeId) {
      auto it = addressToNodeId->find(addr);
      if (it != addressToNodeId->end()) {
        std::ostringstream ss;
        ss << addr << " (node " << it->second << ")";
        return ss.str();
      }
    }
    std::ostringstream ss;
    ss << addr;
    return ss.str();
  };
  
  // Node ID based tables
  oss << "  m_tblByNode:\n";
  if (m_tblByNode.empty()) {
    oss << "    (empty)\n";
  } else {
    // Create sorted vector of node IDs
    std::vector<uint32_t> sortedNodeIds;
    sortedNodeIds.reserve(m_tblByNode.size());
    for (auto const& kv : m_tblByNode) {
      sortedNodeIds.push_back(kv.first);
    }
    std::sort(sortedNodeIds.begin(), sortedNodeIds.end());
    
    // Output in sorted order
    for (uint32_t nodeId : sortedNodeIds) {
      auto it = m_tblByNode.find(nodeId);
      if (it != m_tblByNode.end()) {
        oss << "    destNode=" << nodeId << "\n";
        for (auto const& entry : it->second) {
          oss << "      nh=" << addrToString(entry.nh) << " p=" << entry.p << "\n";
        }
      }
    }
  }
  oss << "  m_statsByNode:\n";
  if (m_statsByNode.empty()) {
    oss << "    (empty)\n";
  } else {
    // Create sorted vector of node IDs
    std::vector<uint32_t> sortedNodeIds;
    sortedNodeIds.reserve(m_statsByNode.size());
    for (auto const& kv : m_statsByNode) {
      sortedNodeIds.push_back(kv.first);
    }
    std::sort(sortedNodeIds.begin(), sortedNodeIds.end());
    
    // Output in sorted order
    for (uint32_t nodeId : sortedNodeIds) {
      auto it = m_statsByNode.find(nodeId);
      if (it != m_statsByNode.end()) {
        const auto& st = it->second;
        oss << "    destNode=" << nodeId
            << " mu=" << st.mu
            << " sigma2=" << st.sigma2
            << " wbest=" << st.wbest
            << " wcount=" << st.wcount
            << " flow=" << st.flow
            << "\n";
      }
    }
  }
  
  // Legacy IP-based tables (if not empty)
  if (!m_tbl.empty()) {
    oss << "  m_tbl (legacy):\n";
    for (auto const& kv : m_tbl) {
      Ipv4Address dest(kv.first);
      oss << "    dest=" << dest << "\n";
      for (auto const& entry : kv.second) {
        oss << "      nh=" << addrToString(entry.nh) << " p=" << entry.p << "\n";
      }
    }
  }
  if (!m_stats.empty()) {
    oss << "  m_stats (legacy):\n";
    for (auto const& kv : m_stats) {
      Ipv4Address dest(kv.first);
      const auto& st = kv.second;
      oss << "    dest=" << dest
          << " mu=" << st.mu
          << " sigma2=" << st.sigma2
          << " wbest=" << st.wbest
          << " wcount=" << st.wcount
          << " flow=" << st.flow
          << "\n";
    }
  }
  return oss.str();
}

// ============ Node ID based implementations ============

void PheromoneTable::EnsureDestNode(uint32_t destNodeId, const std::vector<Ipv4Address>& neighbors) {
  auto &bucket = m_tblByNode[destNodeId];
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

Ipv4Address PheromoneTable::SampleNextHopForNode(uint32_t destNodeId, double beta, uint32_t seed) const {
  auto it = m_tblByNode.find(destNodeId);
  if (it == m_tblByNode.end() || it->second.empty()) return Ipv4Address(); // invalid
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

void PheromoneTable::ReinforceNode(uint32_t destNodeId, Ipv4Address fromPrevHop, double r, double alpha,
                                   const std::vector<Ipv4Address>& neighbors) {
  auto &bucket = m_tblByNode[destNodeId];
  if (bucket.empty()) {
    EnsureDestNode(destNodeId, neighbors);
  }
  if (bucket.empty()) return;
  
  // Debug: print before
  NS_LOG_DEBUG("ReinforceNode BEFORE destNode=" << destNodeId << " via=" << fromPrevHop);
  for (const auto &e : bucket) {
    NS_LOG_DEBUG("  nh=" << e.nh << " p=" << e.p);
  }
  
  double rr = std::max(0.0, std::min(1.0, r)) * std::max(0.0, std::min(1.0, alpha));
  for (auto &e : bucket) {
    if (e.nh == fromPrevHop) {
      e.p = e.p + rr * (1.0 - e.p);
    } else {
      e.p = e.p - rr * e.p;
    }
  }
  Normalize(bucket);
  NS_LOG_INFO("ReinforceNode destNode=" << destNodeId << " via=" << fromPrevHop << " r=" << r << " alpha=" << alpha << " rr=" << rr);
  
  // Debug: print after
  for (const auto &e : bucket) {
    NS_LOG_DEBUG("  AFTER nh=" << e.nh << " p=" << e.p);
  }
}

const std::vector<NextHopEntry>* PheromoneTable::GetBucketForNode(uint32_t destNodeId) const {
  auto it = m_tblByNode.find(destNodeId);
  if (it == m_tblByNode.end()) return nullptr;
  return &it->second;
}

void PheromoneTable::ObserveRttForNode(uint32_t destNodeId, double T, double eta) {
  auto &st = m_statsByNode[destNodeId];
  if (st.wcount == 0) {
    st.mu = T; st.sigma2 = 0.0; st.wbest = T; st.wcount = 1;
    return;
  }
  double mu_old = st.mu;
  st.mu += eta * (T - st.mu);
  st.sigma2 += eta * (((T - mu_old)*(T - mu_old)) - st.sigma2);
  if (T < st.wbest) st.wbest = T;
  if (st.wcount < 1000000000u) st.wcount++;
  NS_LOG_INFO("ObserveRttForNode destNode=" << destNodeId << " T=" << T << " mu=" << st.mu << " best=" << st.wbest);
}

double PheromoneTable::GetReinforcementForNode(uint32_t destNodeId, double T) const {
  auto it = m_statsByNode.find(destNodeId);
  if (it == m_statsByNode.end()) return 0.5;
  const auto &st = it->second;
  if (T <= 0) return 1.0;
  double r1 = st.wbest / T;
  double denom = (st.mu - st.wbest) + (T - st.wbest) + 1e-9;
  double r2 = (st.mu - st.wbest) / denom;
  double r = 0.7 * r1 + 0.3 * r2;
  double s = 1.0 / (1.0 + std::exp(-6.0*(r-0.5)));
  return s;
}

void PheromoneTable::AccumulateFlowForNode(uint32_t destNodeId, double amount) {
  if (amount <= 0.0) {
    return;
  }
  auto &st = m_statsByNode[destNodeId];
  st.flow += amount;
}

double PheromoneTable::GetFlowWeightForNode(uint32_t destNodeId) const {
  auto it = m_statsByNode.find(destNodeId);
  if (it == m_statsByNode.end()) {
    return 0.0;
  }
  return it->second.flow;
}

void PheromoneTable::DecayPheromones(double factor) {
  if (factor <= 0.0 || factor >= 1.0) return;
  
  for (auto& kv : m_tblByNode) {
    auto& bucket = kv.second;
    for (auto& entry : bucket) {
      entry.p *= factor;
    }
    Normalize(bucket);
  }
  
  for (auto& kv : m_tbl) {
    auto& bucket = kv.second;
    for (auto& entry : bucket) {
      entry.p *= factor;
    }
    Normalize(bucket);
  }
}

} // namespace ns3
