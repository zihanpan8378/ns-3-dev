#include "ipv4-antnet-local-traffic-statistics-entry.h"

#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/nstime.h"

#include <vector>
#include <cmath>
#include <limits>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Ipv4AntNetLocalTrafficStatisticsEntry");

Ipv4AntNetLocalTrafficStatisticsEntry::Ipv4AntNetLocalTrafficStatisticsEntry()
  : m_dest(Ipv4Address("0.0.0.0")),
    m_destNetworkMask(Ipv4Mask("/0")),
    m_dataFlowMeasure(0.0),
    m_meanDelay(0.0),
    m_delayVariance(0.0)
{
    // window list empty by default
}

Ipv4AntNetLocalTrafficStatisticsEntry::Ipv4AntNetLocalTrafficStatisticsEntry()
{
    m_meanDelay     = 0.0;
    m_delayVariance = 0.0;  // σ^2
    m_dataFlowMeasure = 0;

    // AntNet recommends |W|max = 5 * (C/η)
    m_maxWindowSize = static_cast<size_t>(5 * (C / ETA));
}



void Ipv4AntNetLocalTrafficStatisticsEntry::UpdateStatistics(Time delay)
{
    double delayMs = delay.GetMilliSeconds();

    // --- Update EMA mean (μ) ---
    m_meanDelay = m_meanDelay + ETA * (delayMs - m_meanDelay);

    // --- Update variance ---
    double diff = delayMs - m_meanDelay;
    m_delayVariance = m_delayVariance + ETA * ((diff * diff) - m_delayVariance);

    // --- Update sliding window ---
    if (m_delayWindow.size() >= m_maxWindowSize)
    {
        m_delayWindow.pop_front();
    }
    m_delayWindow.push_back(delayMs);
}


void Ipv4AntNetLocalTrafficStatisticsEntry::AddDataFlowMeasure()
{
    m_dataFlowMeasure += 1;
}


// ============================
//  Wbest (minimum in window)
// ============================
double Ipv4AntNetLocalTrafficStatisticsEntry::GetBestDelayFromWindow() const
{
    if (m_delayWindow.empty())
    {
        return m_meanDelay;  // fallback
    }

    double best = std::numeric_limits<double>::max();
    for (double d : m_delayWindow)
    {
        best = std::min(best, d);
    }
    return best;
}


// ==========================================
//  Isup = μ + Z * σ / sqrt(|W|)
// ==========================================
double Ipv4AntNetLocalTrafficStatisticsEntry::GetUpperBoundDelayFromWindow() const
{
    if (m_delayWindow.empty())
    {
        // fallback: assume mean as upper bound
        return m_meanDelay;
    }

    double sigma = std::sqrt(std::max(m_delayVariance, 0.0));
    double windowSize = static_cast<double>(m_delayWindow.size());

    double isup = m_meanDelay + Z * (sigma / std::sqrt(windowSize));
    return isup;
}


// ============================
// Getters
// ============================
double Ipv4AntNetLocalTrafficStatisticsEntry::GetMeanDelay() const
{
    return m_meanDelay;
}

double Ipv4AntNetLocalTrafficStatisticsEntry::GetDelayStdDev() const
{
    return std::sqrt(std::max(m_delayVariance, 0.0));
}

size_t Ipv4AntNetLocalTrafficStatisticsEntry::GetWindowSize() const
{
    return m_delayWindow.size();
}

} // namespace ns3

