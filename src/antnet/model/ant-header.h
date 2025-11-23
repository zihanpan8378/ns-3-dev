#ifndef ANT_HEADER_H
#define ANT_HEADER_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/timestamp-tag.h"

namespace ns3
{



class AntHeader : public Header
{
    public:
        enum Type
        {
            FORWARD_ANT = 0,
            BACKWARD_ANT = 1
        };

    private:
        typedef std::pair<Ipv4Address, Time> STACK_ENTRY;

        Type m_type;
        std::vector<STACK_ENTRY> m_stack;
        

    public:
        static TypeId GetTypeId();

        virtual TypeId GetInstanceTypeId () const override;

        virtual uint32_t GetSerializedSize() const override;

        virtual void Serialize(Buffer::Iterator start) const override;

        virtual uint32_t Deserialize(Buffer::Iterator start) override;

        virtual void Print(std::ostream& os) const override;

        void AddHop(Ipv4Address hopAddress, Time delay);

        STACK_ENTRY PopHop();

        void SetAntType(Type type);

        Type GetAntType() const;

        const std::vector<STACK_ENTRY>& GetStack() const;
};

} // namespace ns3

#endif /* ANTNET_HEADER_H */