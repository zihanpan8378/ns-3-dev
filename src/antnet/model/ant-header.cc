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
    // 1 byte for type + 4 bytes for stack size + (4 bytes for IP + 8 bytes for delay) per entry
    return 1 + 4 + m_stack.size() * (4 + 8); 
}

void
AntHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteU8(static_cast<uint8_t>(m_type));
    uint32_t n = m_stack.size();
    start.WriteU32(n);
    for (uint32_t i = 0; i < n; ++i) {
        start.WriteU32(m_stack[i].first.Get());
        int64_t t = m_stack[i].second.GetNanoSeconds ();
        start.WriteHtonU64 (static_cast<uint64_t>(t));
    }
}

uint32_t
AntHeader::Deserialize(Buffer::Iterator start)
{
    m_type = static_cast<AntHeader::Type>(start.ReadU8());

    m_stack.clear();
    uint32_t n = start.ReadU32();
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t ipInt = start.ReadU32();
        uint64_t v = start.ReadNtohU64 ();
        int64_t t = static_cast<int64_t> (v);
        uint32_t delayMs = start.ReadU32();
        m_stack.push_back(std::make_pair(Ipv4Address(ipInt), Time(NanoSeconds(t))));
    }

    return 1 + 4 + n * (4 + 8);
}

void
AntHeader::Print(std::ostream& os) const
{
    os << "AntHeader[path=";
    for (uint32_t k = 0; k < m_stack.size(); ++k) {
        os << "(" << m_stack[k].first << ":" << m_stack[k].second << "ms)";
        if (k != m_stack.size() - 1) {
            os << " -> ";
        }
    }
    os << "]";
}

void
AntHeader::AddHop(Ipv4Address hopAddress, Time delay)
{
    m_stack.push_back(std::make_pair(hopAddress, delay));
}

AntHeader::STACK_ENTRY
AntHeader::PopHop()
{
    if (m_stack.empty()) {
        return std::make_pair(Ipv4Address("0.0.0.0"), Time(Seconds(0)));
    }
    STACK_ENTRY top = m_stack.back();
    m_stack.pop_back();
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

const std::vector<AntHeader::STACK_ENTRY>&
AntHeader::GetStack() const
{
    return m_stack;
}

}