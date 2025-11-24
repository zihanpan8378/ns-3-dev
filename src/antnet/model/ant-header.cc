#include "ant-header.h"

#include <vector>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("AntHeader");

NS_OBJECT_ENSURE_REGISTERED(AntHeader);

TypeId
AntHeader::GetTypeId()
{
    static TypeId tid = 
        TypeId("ns3::AntHeader")
            .SetParent<Header>()
            .AddConstructor<AntHeader>();
    return tid;
}

TypeId 
AntHeader::GetInstanceTypeId () const
{
    return GetTypeId();
}

uint32_t
AntHeader::GetSerializedSize() const
{
    // 1 byte for type + 4 bytes for stack size + (4 bytes for IP + 8 bytes for time) per entry
    return 1 + 4 + (m_forwardStack.size() + m_backwardStack.size()) * (4 + 8); 
}

void
AntHeader::Serialize(Buffer::Iterator start) const
{
    // Serialize ant type
    start.WriteU8(static_cast<uint8_t>(m_type));

    // Serialize forward stack
    start.WriteU32(m_forwardStack.size());
    for (uint32_t i = 0; i < m_forwardStack.size(); ++i) {
        // Serialize address
        start.WriteU32(m_forwardStack[i].GetAddress().Get());

        // Serialize time
        int64_t t = m_forwardStack[i].GetTime().GetNanoSeconds ();
        start.WriteHtonU64 (static_cast<uint64_t>(t));
    }

    // Serialize backward stack
    start.WriteU32(m_backwardStack.size());
    for (uint32_t i = 0; i < m_backwardStack.size(); ++i) {
        // Serialize address
        start.WriteU32(m_backwardStack[i].GetAddress().Get());

        // Serialize time
        int64_t t = m_backwardStack[i].GetTime().GetNanoSeconds ();
        start.WriteHtonU64 (static_cast<uint64_t>(t));
    }
}

uint32_t
AntHeader::Deserialize(Buffer::Iterator start)
{
    // Deserialize ant type
    m_type = static_cast<AntHeader::Type>(start.ReadU8());

    // Deserialize forward stack
    m_forwardStack.clear();
    // Deserialize size
    uint32_t forwardStackSize = start.ReadU32();
    for (uint32_t i = 0; i < forwardStackSize; ++i) {
        // Deserialize address
        uint32_t ipInt = start.ReadU32();

        // Deserialize time
        uint64_t v = start.ReadNtohU64 ();
        int64_t t = static_cast<int64_t> (v);

        // Add entry back to m_forwardStack
        m_forwardStack.push_back(
            AntHeaderStackEntry(
                Ipv4Address(ipInt), 
                Time(NanoSeconds(t))
            )
        );
    }

    // Deserialize backward stack
    m_backwardStack.clear();
    // Deserialize size
    uint32_t backwardStackSize = start.ReadU32();
    for (uint32_t i = 0; i < backwardStackSize; ++i) {
        // Deserialize address
        uint32_t ipInt = start.ReadU32();
        // Deserialize time
        uint64_t v = start.ReadNtohU64 ();
        int64_t t = static_cast<int64_t> (v);

        // Add entry back to m_backwardStack
        m_backwardStack.push_back(
            AntHeaderStackEntry(
                Ipv4Address(ipInt), 
                Time(NanoSeconds(t))
            )
        );
    }

    // Return total size consumed
    return 1 + 4 + (m_forwardStack.size() + m_backwardStack.size()) * (4 + 8);
}

void
AntHeader::Print(std::ostream& os) const
{
    os << "AntHeader[forwardPath=";
    for (uint32_t k = 0; k < m_forwardStack.size(); ++k) {
        os << "(" << m_forwardStack[k].GetAddress() << ":" << m_forwardStack[k].GetTime() << "ms)";
        if (k != m_forwardStack.size() - 1) {
            os << " -> ";
        }
    }
    os << "]\n";

    os << "AntHeader[returnPath=";
    for (uint32_t k = 0; k < m_backwardStack.size(); ++k) {
        os << "(" << m_backwardStack[k].GetAddress() << ":" << m_backwardStack[k].GetTime() << "ms)";
        if (k != m_backwardStack.size() - 1) {
            os << " -> ";
        }
    }
    os << "]";
}

void
AntHeader::AddForwardHop(Ipv4Address hopAddress, Time time)
{
    m_forwardStack.push_back(AntHeaderStackEntry(hopAddress, time));
}

AntHeader::AntHeaderStackEntry
AntHeader::PopForwardStackEntryToBackwardStack()
{
    if (m_forwardStack.empty()) {
        return AntHeaderStackEntry(Ipv4Address("0.0.0.0"), Time(Seconds(0)));
    }

    AntHeaderStackEntry top = m_forwardStack.back();

    // Remove from m_forwardStack and add to m_backwardStack
    m_forwardStack.pop_back();
    m_backwardStack.push_back(top);

    return top;
}

void
AntHeader::SetAntType(AntHeader::Type type)
{
    m_type = type;
}

AntHeader::Type
AntHeader::GetAntType() const
{
    return m_type;
}

const std::vector<AntHeader::AntHeaderStackEntry>&
AntHeader::GetForwardStack() const
{
    return m_forwardStack;
}

const std::vector<AntHeader::AntHeaderStackEntry>&
AntHeader::GetBackwardStack() const
{
    return m_backwardStack;
}

} // namespace ns3