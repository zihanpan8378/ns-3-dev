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
        std::list<double> m_delayWindow; // Sliding window of delays

        static const double MU = 0.005;
        static const double C = 0.3;
        static const double MAX_WINDOW_SIZE = 5 * (C / MU);

    public:
        // The constructor and destructor are not implemented yet
        Ipv4AntNetLocalTrafficStatisticsEntry();
        ~Ipv4AntNetLocalTrafficStatisticsEntry();


        /**
         * @brief Update statistics with a new delay measurement
         * @param delay The measured delay from current node to the destination m_dest
         */
        void UpdateStatistics(Time delay);
        /**
         * @brief Increment data flow measure when a data packet is sent to m_dest
         * For simplicity, we just increment by 1 for each data packet
         */
        void AddDataFlowMeasure();
        
        /**
         * @return The destination address of this entry
         */
        Ipv4Address GetDestAddr() const { return m_dest; }

        /**
         * @return The destination network mask of this entry
         */
        Ipv4Mask GetDestMask() const { return m_destNetworkMask; }

        /**
         * @return The data flow measure (number of data packets sent to the destination m_dest)
         */
        double GetDataFlowMeasure() const { return m_dataFlowMeasure; }

        /**
         * @return The mean delay to the destination m_dest
         */
        double GetMeanDelay() const { return m_meanDelay; }

        /**
         * @return The delay variance to the destination m_dest
         */
        double GetDelayVariance() const { return m_delayVariance; }

        /**
         * @return The best (minimum) delay from the sliding window with size MAX_WINDOW_SIZE
         */
        double GetBestDelayFromWindow() const;

        /**
         * @return The upper bound delay (not implemented yet)
         */
        double GetUpperBoundDelayFromWindow() const;
};

} // namespace ns3
#endif /* IPV4_ANTNET_LOCAL_TRAFFIC_STATISTICS_ENTRY_H */
