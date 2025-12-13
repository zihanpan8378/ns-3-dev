#include "ipv4-antnet-routing-table-entry.h"

#include "ns3/log.h"
#include "ns3/object.h"

#include <vector>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("Ipv4AntNetRoutingTableEntry");

int Ipv4AntNetRoutingTableEntry::Nk = 0;

Ipv4AntNetRoutingTableEntry::Ipv4AntNetRoutingTableEntry()
{
    NS_LOG_FUNCTION(this);
}

Ipv4AntNetRoutingTableEntry::Ipv4AntNetRoutingTableEntry(const Ipv4AntNetRoutingTableEntry& route)
    : m_dest(route.m_dest),
      m_destNetworkMask(route.m_destNetworkMask),
      m_pheromoneList(route.m_pheromoneList),
        m_visitedNextHops(route.m_visitedNextHops)
{
    NS_LOG_FUNCTION(this << route);
}

Ipv4AntNetRoutingTableEntry::Ipv4AntNetRoutingTableEntry(const Ipv4AntNetRoutingTableEntry* route)
    : m_dest(route->m_dest),
      m_destNetworkMask(route->m_destNetworkMask),
      m_pheromoneList(route->m_pheromoneList),
      m_visitedNextHops(route->m_visitedNextHops)
{
    NS_LOG_FUNCTION(this << route);
}

Ipv4AntNetRoutingTableEntry::Ipv4AntNetRoutingTableEntry(Ipv4Address dest,
                                                         Ipv4Mask destNetworkMask,
                                                         PheromoneList pheromoneList)
    : m_dest(dest),
      m_destNetworkMask(destNetworkMask),
      m_pheromoneList(pheromoneList),
      m_visitedNextHops()
{
    NS_LOG_FUNCTION(this << dest << destNetworkMask);
    for (const auto& entry : pheromoneList) {
        m_visitedNextHops[entry.first] = false;
    }
}

std::string 
Ipv4AntNetRoutingTableEntry::ToString() const
{
    std::ostringstream oss;

    oss << "destination address=" << GetDestAddr() << "/" << GetDestMask().GetPrefixLength() << ":\n";
    for (const auto& entry : GetPheromoneList()) {
        oss << "            next hop=" << entry.first.first << ", interface=" << entry.first.second << ", pheromone=" << entry.second << std::endl;
    }
    return oss.str();
}

Ipv4Address
Ipv4AntNetRoutingTableEntry::GetDestAddr() const
{
    NS_LOG_FUNCTION(this);
    return m_dest;
}

Ipv4Mask
Ipv4AntNetRoutingTableEntry::GetDestMask() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask;
}

Ipv4AntNetRoutingTableEntry::PheromoneList
Ipv4AntNetRoutingTableEntry::GetPheromoneList() const
{
    NS_LOG_FUNCTION(this);
    return m_pheromoneList;
}

bool
Ipv4AntNetRoutingTableEntry::HasNextHop() const
{
    NS_LOG_FUNCTION(this);
    for (const auto& entry : m_pheromoneList) {
        if (entry.first.first != Ipv4Address::GetZero()) {
            return true;
        }
    }
    return false;
}

Ipv4AntNetRoutingTableEntry::PheromoneKey
Ipv4AntNetRoutingTableEntry::GetNextHop(std::map<Ipv4Address, double> queueLengthMap, Ptr<NetDevice> oif) const
{
    NS_LOG_FUNCTION(this);

    // If no next hops available, return zero address and interface 0 (should not happen)
    if (m_pheromoneList.empty()) {
        return Ipv4AntNetRoutingTableEntry::PheromoneKey(Ipv4Address::GetZero(), 0);
    }

    PheromoneList adjustedPheromoneList;
    if (queueLengthMap.empty()) {
        adjustedPheromoneList = m_pheromoneList;
        NS_LOG_INFO("No queue length info provided, using original pheromone values");
    } else {
        double averageQueueLength = 0.0;
        for (const auto& qlenEntry : queueLengthMap) {
            averageQueueLength += qlenEntry.second;
        }
        averageQueueLength /= static_cast<double>(queueLengthMap.size());

        
        for (const auto& entry : m_pheromoneList) {
            PheromoneKey nextHopKey = entry.first;

            // Find the corresponding queue length
            double queueLength = 0.0;
            auto it = queueLengthMap.find(entry.first.first);
            if (it != queueLengthMap.end()) {
                queueLength = it->second;
            }
            NS_LOG_INFO("Next hop " << nextHopKey.first << " on interface " << nextHopKey.second
                         << " has queue length " << queueLength << " (average: " << averageQueueLength << ")");

            double ln = 1.0 - queueLength / (averageQueueLength + 1e-6); // Avoid division by zero
            double adjustedPheromone = entry.second + (ALPHA * ln) / (1 + ALPHA * (queueLengthMap.size() - 1));
            
            adjustedPheromoneList.push_back(std::make_pair(nextHopKey, adjustedPheromone));
        }
    }

    if (oif != nullptr) {
        // Filter adjustedPheromoneList to only include entries matching the specified output interface
        PheromoneList filteredList;
        for (const auto& entry : adjustedPheromoneList) {
            if (entry.first.second == oif->GetIfIndex()) {
                filteredList.push_back(entry);
            }
        }
        if (!filteredList.empty()) {
            adjustedPheromoneList = filteredList;
        }
    }


    // Check if all entries are visited before (not implemented yet)
    // If all visited, use all entries, otherwise use only unvisited entries
    PheromoneList candidates;
    for (const auto& entry : adjustedPheromoneList) {
        if (!m_visitedNextHops.at(entry.first)) {
            candidates.push_back(entry);
        }
    }
    if (candidates.empty()) {
        candidates = adjustedPheromoneList;
    }

    // Calculate total probability
    double totalProb = 0.0;
    for (const auto& entry : candidates) {
        totalProb += entry.second;
    }

    // Generate random number between 0 and totalProb
    double randomValue = static_cast<double>(rand()) / RAND_MAX * totalProb;

    // Select based on cumulative probability
    double cumulativeProb = 0.0;
    for (const auto& entry : candidates) {
        cumulativeProb += entry.second;
        if (randomValue <= cumulativeProb) {
            m_visitedNextHops[entry.first] = true; // Mark as visited
            return entry.first;
        }
    }
    // Return the last entry if none selected (should not happen)
    m_visitedNextHops[candidates.back().first] = true; // Mark as visited
    return candidates.back().first;
}

Ipv4AntNetRoutingTableEntry::PheromoneKey 
Ipv4AntNetRoutingTableEntry::GetDeterministicNextHop(Ipv4Address nextHopAddr) const
{
    NS_LOG_FUNCTION(this << nextHopAddr);
    for (const auto& entry : m_pheromoneList) {
        if (entry.first.first == nextHopAddr) {
            return entry.first;
        }
    }
    // If not found, return zero address and interface 0 (should not happen)
    return Ipv4AntNetRoutingTableEntry::PheromoneKey(Ipv4Address::GetZero(), 0);
}

void 
Ipv4AntNetRoutingTableEntry::UpdatePheromone(Ipv4Address nextHop, double delayMillisecond, Ipv4AntNetLocalTrafficStatisticsEntry trafficStat)
{
    NS_LOG_FUNCTION(this << nextHop);

    // Assert that nextHop is in m_pheromoneList
    bool found = false;
    // entry.first.firstを保存する用のベクター
    std::vector<Ipv4Address> nextHopAddresses;
    for (const auto& entry : m_pheromoneList) {
        nextHopAddresses.push_back(entry.first.first);
        if (entry.first.first == nextHop) {
            found = true;
            break;
        }
    }
    // NS_ASSERT_MSG(found, "nextHop " << nextHop << " not found in pheromone list:" << nextHopAddresses);
    // ベクタを文字列に変換
    std::ostringstream oss;
    for (const auto& addr : nextHopAddresses) {
        oss << addr << " ";
    }

    NS_ASSERT_MSG(found,
                "nextHop " << nextHop
                << " not found in pheromone list: " << oss.str());
                

    double bestDelay = trafficStat.GetBestDelayFromWindow();
    double iInf = bestDelay;
    double iSup = trafficStat.GetUpperBoundDelayFromWindow();

    double reward = C1 * (bestDelay / (delayMillisecond + 1e-6)) + C2 * ((iSup - iInf) / ((iSup - iInf) + (delayMillisecond + 1e-6 - iInf))); // Avoid division by zero

    double a = 7.0;
    double sr = 1.0 / (1.0 + std::exp(a / (reward*Nk))); // Sigmoid function to bound reward between 0 and 1
    double s1 = 1.0 / (1.0 + std::exp(a / Nk));
    reward = sr/s1; // Normalize to [0, 1]

    if (reward > 0.95) {
        reward = 0.95;
    } else if (reward < 0.05) {
        reward = 0.05;
    }


    double sum = 0.0;
    for (auto& entry : m_pheromoneList) {
        double pheromoneBefore = entry.second;
        if (entry.first.first == nextHop) {
            // Update pheromone for the chosen next hop
            entry.second = entry.second + reward * (1 - entry.second);
        } else {
            // Evaporate pheromone for other next hops
            entry.second = entry.second - reward * entry.second;
        }
        sum += entry.second;
        NS_ASSERT_MSG(entry.second >= 0.0 && entry.second <= 1.0, "Pheromone value out of range: " << entry.second << " (before: " << pheromoneBefore << ", reward: " << reward << ")");
    }
    NS_ASSERT_MSG(sum > 1.0 - 1e-6 && sum < 1.0 + 1e-6, "Sum of pheromone values out of range: " << sum);
}

double 
Ipv4AntNetRoutingTableEntry::EvaporatePheromone(Ipv4Address nextHop, double evaporationFactor)
{
    NS_LOG_FUNCTION(this << nextHop);

    NS_ASSERT_MSG(evaporationFactor > 0.0 && evaporationFactor < 1.0, "Evaporation factor out of range: " << evaporationFactor);

    double originalPheromone = 0.0;
    // Evaporate pheromone for the given next hop address
    for (auto& entry : m_pheromoneList) {
        if (entry.first.first == nextHop) {
            originalPheromone = entry.second;
            entry.second *= evaporationFactor;
        }
        NS_ASSERT_MSG(entry.second >= 0.0 && entry.second <= 1.0, "Pheromone value out of range: " << entry.second);
    }

    // Normalize pheromone values to sum to 1
    double totalPheromone = 0.0;
    for (const auto& entry : m_pheromoneList) {
        totalPheromone += entry.second;
    }
    for (auto& entry : m_pheromoneList) {
        entry.second /= totalPheromone;
        NS_ASSERT_MSG(entry.second >= 0.0 && entry.second <= 1.0, "Pheromone value out of range: " << entry.second);
    }

    // Return the effect of evaporation on the pheromone value
    // This value is used for failure message propagation
    return originalPheromone * (1.0 - evaporationFactor);
}

bool
Ipv4AntNetRoutingTableEntry::IsNetwork() const
{
    NS_LOG_FUNCTION(this);
    return !IsHost();
}

bool
Ipv4AntNetRoutingTableEntry::IsDefault() const
{
    NS_LOG_FUNCTION(this);
    return m_dest == Ipv4Address::GetZero();
}

bool
Ipv4AntNetRoutingTableEntry::IsHost() const
{
    NS_LOG_FUNCTION(this);
    return m_destNetworkMask == Ipv4Mask::GetOnes();
}

Ipv4AntNetRoutingTableEntry
Ipv4AntNetRoutingTableEntry::CreateDefaultRoute(Ipv4Address nextHop, uint32_t interface)
{
    NS_LOG_FUNCTION(nextHop << interface);
    return Ipv4AntNetRoutingTableEntry(
        Ipv4Address::GetZero(), 
        Ipv4Mask::GetZero(), 
        std::list<std::pair<PheromoneKey, double>>{ 
            { PheromoneKey(nextHop, interface), 1.0 } 
        }
    );
}

Ipv4AntNetRoutingTableEntry
Ipv4AntNetRoutingTableEntry::CreateNetworkRouteTo(Ipv4Address network,
                                                  Ipv4Mask networkMask,
                                                  std::list<PheromoneKey> nextHops)
{
    NS_LOG_FUNCTION(network << networkMask);
    Ipv4AntNetRoutingTableEntry::PheromoneList pheromoneList;
    double initialPheromone = 1.0 / static_cast<double>(nextHops.size());
    for (const auto& nextHop : nextHops) {
        pheromoneList.push_back(std::make_pair(nextHop, initialPheromone));
    }
    return Ipv4AntNetRoutingTableEntry(network, networkMask, pheromoneList);
}

std::ostream&
operator<<(std::ostream& os, const Ipv4AntNetRoutingTableEntry& route) {
    if (route.IsDefault()) {
        Ipv4AntNetRoutingTableEntry::PheromoneKey nextHop = route.GetNextHop();
        os << "default out=" << nextHop.first << ", next hop=" << nextHop.second << "\n";
    } else if (route.IsHost()) {
        os << "host=" << route.GetDestAddr() << ":\n";
        for (const auto& entry : route.GetPheromoneList()) {
            os << "   next hop=" << entry.first.first << ", interface=" << entry.first.second
               << ", pheromone=" << entry.second << "\n";
        }
    } else if (route.IsNetwork()) {
        os << "network=" << route.GetDestAddr() << ", mask=" << route.GetDestMask()
           << ":\n";
        for (const auto& entry : route.GetPheromoneList()) {
            os << "   next hop=" << entry.first.first << ", interface=" << entry.first.second
               << ", pheromone=" << entry.second << "\n";
        }
    } else {
        NS_ASSERT(false);
    }
    return os;
}

bool
operator==(const Ipv4AntNetRoutingTableEntry a, const Ipv4AntNetRoutingTableEntry b)
{
    return (a.GetDestAddr() == b.GetDestAddr() && a.GetDestMask() == b.GetDestMask());
}

} // namespace ns3
