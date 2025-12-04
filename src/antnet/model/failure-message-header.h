#ifndef FAILURE_MESSAGE_HEADER_H
#define FAILURE_MESSAGE_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/timestamp-tag.h"
#include <map>
#include <list>

namespace ns3
{

class FailureMessageHeader : public Header
{
    private:
        /**
         * @brief List of destination addresses and their evaporation factors
         */
        std::list<std::pair<Ipv4Address, double>> m_evaporationEntries;

    public:
        FailureMessageHeader();

        FailureMessageHeader(std::map<Ipv4Address, double> evaporationEffects);

        static TypeId GetTypeId();

        virtual TypeId GetInstanceTypeId () const override;

        virtual uint32_t GetSerializedSize() const override;

        virtual void Serialize(Buffer::Iterator start) const override;

        virtual uint32_t Deserialize(Buffer::Iterator start) override;

        virtual void Print(std::ostream& os) const override;

        std::string ToString() const;
        
        const std::list<std::pair<Ipv4Address, double>>& GetEvaporationEntries() const { return m_evaporationEntries; }
};

} // namespace ns3

#endif /* FAILURE_MESSAGE_HEADER_H */