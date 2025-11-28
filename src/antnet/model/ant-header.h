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

        /**
         * @brief An entry in the ant header stack
         * Represents a hop with its incoming addresses (which interface address it is received on) 
         *                       and outgoing addresses (which interface address it is sent out)
         *                       and the time when the ant reached this hop
         */
        class AntHeaderStackEntry
        {
            private:
                Ipv4Address addressIn;
                Ipv4Address addressOut;
                Time time;

            public:
                AntHeaderStackEntry(Ipv4Address addrIn, Ipv4Address addrOut, Time t)
                    : addressIn(addrIn), addressOut(addrOut), time(t) {}

                Ipv4Address GetAddressIn() const { return addressIn; }
                Ipv4Address GetAddressOut() const { return addressOut; }
                Time GetTime() const { return time; }
        };

    private:
        Type m_type;
        std::vector<AntHeaderStackEntry> m_forwardStack;
        std::vector<AntHeaderStackEntry> m_backwardStack;

        uint32_t m_sourceNodeId;
        Ipv4Address m_sourceAddress;
        Ipv4Address m_destinationAddress;
        uint32_t m_round;

    public:
        AntHeader();

        AntHeader(
            Type type,
            uint32_t sourceNodeId,
            Ipv4Address source,
            Ipv4Address destination,
            uint32_t round
        );

        static TypeId GetTypeId();

        virtual TypeId GetInstanceTypeId () const override;

        virtual uint32_t GetSerializedSize() const override;

        virtual void Serialize(Buffer::Iterator start) const override;

        virtual uint32_t Deserialize(Buffer::Iterator start) override;

        virtual void Print(std::ostream& os) const override;

        std::string ToString() const;

        /**
         * @brief Add a hop to the stack when relaying a forward ant
         * @param hopInAddress The incoming address of the current hop to add
         * @param hopOutAddress The outgoing address of the current hop to add
         * @param time The time when reached this hop
         */
        void AddForwardHop(Ipv4Address hopInAddress, Ipv4Address hopOutAddress, Time time);

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