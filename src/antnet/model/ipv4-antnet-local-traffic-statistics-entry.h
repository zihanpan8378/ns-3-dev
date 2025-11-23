#ifndef IPV4_ANTNET_LOCAL_TRAFFIC_STATISTICS_ENTRY_H
#define IPV4_ANTNET_LOCAL_TRAFFIC_STATISTICS_ENTRY_H

#include "ns3/ipv4-address.h"
#include "ns3/net-device.h"

#include <list>
#include <ostream>
#include <vector>

namespace ns3
{

class Ipv4AntNetLocalTrafficStatisticsEntry
{
    private:
        Ipv4Address m_dest;
        Ipv4Mask m_destNetworkMask;

        double m_dataFlowMeasure; // Measure of data flow (currently use sample count)
        double m_meanDelay; // Mean delay to destination
        double m_delayVariance; // Variance of delay to destination
        double m_bestWindowDelay; // Best delay observed in a time window

        static const double MU = 0.005;
        static const double C = 0.3;
        static const double MAX_WINDOW_SIZE = 5 * (C / MU);

    public:
        void UpdateStatistics(double delay);
        
        Ipv4Address GetDestAddr() const { return m_dest; }
        Ipv4Mask GetDestMask() const { return m_destNetworkMask; }
        double GetDataFlowMeasure() const { return m_dataFlowMeasure; }
        double GetMeanDelay() const { return m_meanDelay; }
        double GetDelayVariance() const { return m_delayVariance; }
        double GetBestWindowDelay() const { return m_bestWindowDelay; }

};

} // namespace ns3
#endif /* IPV4_ANTNET_LOCAL_TRAFFIC_STATISTICS_ENTRY_H */
