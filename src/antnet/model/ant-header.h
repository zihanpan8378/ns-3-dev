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

        class AntHeaderStackEntry
        {
            Ipv4Address address;
            Time time;

            public:
                AntHeaderStackEntry(Ipv4Address addr, Time t)
                    : address(addr), time(t) {}

                Ipv4Address GetAddress() const { return address; }
                Time GetTime() const { return time; }
        };

    private:
        Type m_type;
        std::vector<AntHeaderStackEntry> m_forwardStack;
        std::vector<AntHeaderStackEntry> m_backwardStack;
        

    public:
        static TypeId GetTypeId();

        virtual TypeId GetInstanceTypeId () const override;

        virtual uint32_t GetSerializedSize() const override;

        virtual void Serialize(Buffer::Iterator start) const override;

        virtual uint32_t Deserialize(Buffer::Iterator start) override;

        virtual void Print(std::ostream& os) const override;

        /**
         * @brief Add a hop to the stack when relaying a forward ant
         * @param hopAddress The address of the hop to add
         * @param time The time when reached this hop
         */
        void AddForwardHop(Ipv4Address hopAddress, Time time);

        /**
         * @brief Pop the top hop from the forward stack, and add it to the backward stack
         * @return The top hop entry
         */
        AntHeaderStackEntry PopForwardStackEntryToBackwardStack();

        void SetAntType(Type type);

        Type GetAntType() const;

        const std::vector<AntHeaderStackEntry>& GetForwardStack() const;

        const std::vector<AntHeaderStackEntry>& GetBackwardStack() const;
};

} // namespace ns3

#endif /* ANTNET_HEADER_H */