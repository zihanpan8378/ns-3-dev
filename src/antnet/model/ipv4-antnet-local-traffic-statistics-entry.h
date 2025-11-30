#ifndef IPV4_ANTNET_LOCAL_TRAFFIC_STATISTICS_ENTRY_H
#define IPV4_ANTNET_LOCAL_TRAFFIC_STATISTICS_ENTRY_H

#include "ns3/ipv4-address.h"
#include "ns3/net-device.h"
#include "ns3/nstime.h"

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

        double m_dataFlowMeasure;                                   // Measure of data flow (currently use sample count)
        double m_meanDelay;                                         // Mean delay to destination
        double m_delayVariance;                                     // Variance of delay to destination
        std::list<double> m_delayWindow;                            // Sliding window of delays

        static constexpr double ETA = 0.005;                        // mean update rate
        static constexpr double C = 0.3;                            // constant for window size calculation
        static constexpr double Z = 1.70;                           // confidence coefficient
        
        static constexpr uint32_t MAX_WINDOW_SIZE = 5 * (C / ETA);    // Maximum size of the sliding window

    public:
        Ipv4AntNetLocalTrafficStatisticsEntry();

        Ipv4AntNetLocalTrafficStatisticsEntry(Ipv4Address dest, Ipv4Mask destNetworkMask);

        std::string ToString() const;


        /**
         * @brief Update statistics with a new delay measurement
         * @param delay The measured delay from current node to the destination m_dest
         */
        void UpdateStatistics(double delayMs);
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
         * @return The standard deviation of delay to the destination m_dest
         */
        double GetStandardDeviation() const { return std::sqrt(std::max(m_delayVariance, 0.0)); }

        /**
         * @return The size of the sliding window
         */
        int GetWindowSize() const { return m_delayWindow.size(); }

        /**
         * @return The best (minimum) delay from the sliding window with size MAX_WINDOW_SIZE
         * This is I_inf in the paper
         */
        double GetBestDelayFromWindow() const;

        /**
         * @return The upper bound delay
         * This is I_sup in the paper
         */
        double GetUpperBoundDelayFromWindow() const;
};

} // namespace ns3
#endif /* IPV4_ANTNET_LOCAL_TRAFFIC_STATISTICS_ENTRY_H */
