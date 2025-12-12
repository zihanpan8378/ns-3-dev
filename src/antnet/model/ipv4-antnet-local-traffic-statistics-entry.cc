#include "ipv4-antnet-local-traffic-statistics-entry.h"

#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/nstime.h"

#include <vector>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("Ipv4AntNetLocalTrafficStatisticsEntry");

Ipv4AntNetLocalTrafficStatisticsEntry::Ipv4AntNetLocalTrafficStatisticsEntry(Ipv4Address dest, Ipv4Mask destNetworkMask) 
    : m_dest(dest),
      m_destNetworkMask(destNetworkMask),
      m_dataFlowMeasure(1.0),
      m_meanDelay(0.0),
      m_delayVariance(0.0),
      m_delayWindow({}),
      m_receivedSamplesCount(0)
{
}

std::string 
Ipv4AntNetLocalTrafficStatisticsEntry::ToString() const
{
    std::ostringstream oss;
    oss << "destination=" << m_dest << "/" << m_destNetworkMask.GetPrefixLength() << ", "
        << "data flow measure=" << m_dataFlowMeasure << ", "
        << "mean delay=" << m_meanDelay << "ms, "
        << "delay variance=" << m_delayVariance << "ms^2, "
        << "received samples count=" << m_receivedSamplesCount
        << std::endl;
    return oss.str();
}

void Ipv4AntNetLocalTrafficStatisticsEntry::UpdateStatistics(double delayMs) {
    m_receivedSamplesCount += 1;

    // Update mean delay using exponential moving average
    if (m_meanDelay <= 0.0) {
        m_meanDelay = delayMs;
    } else {
        m_meanDelay = m_meanDelay + ETA * (delayMs - m_meanDelay);
    }

    // Update delay variance
    double delta = delayMs - m_meanDelay;
    m_delayVariance = m_delayVariance + ETA * ((delta * delta) - m_delayVariance);

    // Update window delays
    if (m_delayWindow.size() >= MAX_WINDOW_SIZE) {
        m_delayWindow.pop_front();
    }
    m_delayWindow.push_back(delayMs);
}

void Ipv4AntNetLocalTrafficStatisticsEntry::AddDataFlowMeasure() { 
    m_dataFlowMeasure += 1; 
}

double Ipv4AntNetLocalTrafficStatisticsEntry::GetBestDelayFromWindow() const {
    if (m_delayWindow.empty()) {
        return m_meanDelay; // Fallback to mean delay if window is empty
    }

    double bestDelay = std::numeric_limits<double>::max();
    for (const auto& delayMs : m_delayWindow) {
        bestDelay = std::min(bestDelay, delayMs);
    }
    return bestDelay;
}

double Ipv4AntNetLocalTrafficStatisticsEntry::GetUpperBoundDelayFromWindow() const {
    if (m_delayWindow.empty()) {
        return m_meanDelay;
    }

    double sigma = std::sqrt(std::max(m_delayVariance, 0.0));
    double windowSize = static_cast<double>(m_delayWindow.size());
    return m_meanDelay + Z * (sigma / std::sqrt(windowSize));
}

bool
operator==(const Ipv4AntNetLocalTrafficStatisticsEntry a, const Ipv4AntNetLocalTrafficStatisticsEntry b)
{
    return (a.GetDestAddr() == b.GetDestAddr() && a.GetDestMask() == b.GetDestMask());
}


} // namespace ns3
