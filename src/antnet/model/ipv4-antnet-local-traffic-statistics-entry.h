#ifndef IPV4_ANTNET_LOCAL_TRAFFIC_STATISTICS_ENTRY_H
#define IPV4_ANTNET_LOCAL_TRAFFIC_STATISTICS_ENTRY_H

#include "ns3/ipv4-address.h"
#include "ns3/ipv4-mask.h"
#include "ns3/nstime.h"

#include <list>
#include <vector>
#include <ostream>

namespace ns3
{

class Ipv4AntNetLocalTrafficStatisticsEntry
{
  private:
    Ipv4Address m_dest;
    Ipv4Mask m_destNetworkMask;

    double m_dataFlowMeasure;     // Number of data packets for this destination
    double m_meanDelay;           // Exponential moving average delay
    double m_delayVariance;       // Variance of delay
    std::list<double> m_delayWindow;

    // Algorithm constants
    static inline constexpr double ETA = 0.005;   // learning rate
    static inline constexpr double Z   = 1.70;    // confidence coefficient
    static inline constexpr double C   = 0.3;     // constant used for window size
    static inline constexpr double MU  = 0.005;   // mean update rate

    static inline constexpr uint32_t MAX_WINDOW_SIZE = 5 * (C / MU);

  public:
    Ipv4AntNetLocalTrafficStatisticsEntry();
    ~Ipv4AntNetLocalTrafficStatisticsEntry() = default;

    /**
     * @brief Update statistics with new delay measurement.
     */
    void UpdateStatistics(Time delay);

    /**
     * @brief Increment flow count
     */
    void AddDataFlowMeasure();

    inline Ipv4Address GetDestAddr() const { return m_dest; }
    inline Ipv4Mask GetDestMask() const { return m_destNetworkMask; }
    inline double GetDataFlowMeasure() const { return m_dataFlowMeasure; }
    inline double GetMeanDelay() const { return m_meanDelay; }
    inline double GetDelayVariance() const { return m_delayVariance; }

    /**
     * @return minimum (best) delay in window
     */
    double GetBestDelayFromWindow() const;

    /**
     * @return upper bound delay (TBD)
     */
    double GetUpperBoundDelayFromWindow() const;
};

} // namespace ns3

#endif /* IPV4_ANTNET_LOCAL_TRAFFIC_STATISTICS_ENTRY_H */

