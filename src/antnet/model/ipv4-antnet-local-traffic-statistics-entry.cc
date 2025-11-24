#include "ipv4-antnet-local-traffic-statistics-entry.h"

#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/nstime.h"

#include <vector>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("Ipv4AntNetLocalTrafficStatisticsEntry");

void Ipv4AntNetLocalTrafficStatisticsEntry::UpdateStatistics(Time delay) {
    double delayMs = delay.GetMilliSeconds();
    // Update mean delay using exponential moving average
    m_meanDelay = m_meanDelay + MU * (delayMs - m_meanDelay);

    // Update delay variance
    m_delayVariance = m_delayVariance + MU * ((delayMs - m_meanDelay) * (delayMs - m_meanDelay) - m_delayVariance);

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
    double bestDelay = std::numeric_limits<double>::max();
    for (const auto& delayMs : m_delayWindow) {
        if (delayMs < bestDelay) {
            bestDelay = delayMs;
        }
    }
    return bestDelay;
}

double Ipv4AntNetLocalTrafficStatisticsEntry::GetUpperBoundDelayFromWindow() const {
    // Placeholder for upper bound delay calculation



    
    return 0.0;
}

} // namespace ns3
