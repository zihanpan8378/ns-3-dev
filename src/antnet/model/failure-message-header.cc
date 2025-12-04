#include "failure-message-header.h"

#include <vector>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("FailureMessageHeader");

NS_OBJECT_ENSURE_REGISTERED(FailureMessageHeader);

TypeId
FailureMessageHeader::GetTypeId()
{
    static TypeId tid = 
        TypeId("ns3::FailureMessageHeader")
            .SetParent<Header>()
            .AddConstructor<FailureMessageHeader>();
    return tid;
}

TypeId 
FailureMessageHeader::GetInstanceTypeId () const
{
    return GetTypeId();
}

FailureMessageHeader::FailureMessageHeader()
{
    // Initialize default values if needed
    m_evaporationEntries.clear();
}

FailureMessageHeader::FailureMessageHeader(std::map<Ipv4Address, double> evaporationEffects)
{
    m_evaporationEntries.clear();
    for (const auto& [destinationAddress, evaporationFactor] : evaporationEffects) {
        std::pair<Ipv4Address, double> evaporationEntry(destinationAddress, evaporationFactor);
        m_evaporationEntries.push_back(evaporationEntry);
    }
}

uint32_t
FailureMessageHeader::GetSerializedSize() const
{
    // Calculate the size needed for serialization
    return 4 + m_evaporationEntries.size() * (4 + 8); // Example size calculation
}

void
FailureMessageHeader::Serialize(Buffer::Iterator start) const
{
    // Serialize the number of entries
    start.WriteHtonU32(static_cast<uint32_t>(m_evaporationEntries.size()));
    // Serialize each entry
    for (const auto& entry : m_evaporationEntries) {
        start.WriteHtonU32(entry.first.Get());
        uint64_t rawValue;
        double evapFactor = entry.second;
        std::memcpy(&rawValue, &evapFactor, sizeof(double));
        start.WriteHtonU64(rawValue);
    }
}

uint32_t
FailureMessageHeader::Deserialize(Buffer::Iterator start)
{
    m_evaporationEntries.clear();
    uint32_t numEntries = start.ReadNtohU32();
    for (uint32_t i = 0; i < numEntries; ++i) {
        Ipv4Address destAddress(start.ReadNtohU32());
        uint64_t rawValue = start.ReadNtohU64();
        double evapFactor;
        std::memcpy(&evapFactor, &rawValue, sizeof(double));
        std::pair<Ipv4Address, double> entry(destAddress, evapFactor);
        m_evaporationEntries.push_back(entry);
    }
    return 4 + numEntries * (4 + 8);
}

void
FailureMessageHeader::Print(std::ostream& os) const
{
    os << "[";
    for (const auto& entry : m_evaporationEntries) {
        os << entry.first << ": " << entry.second << ", ";
    }
    os << "]";
}

std::string
FailureMessageHeader::ToString() const
{
    std::ostringstream oss;
    Print(oss);
    return oss.str();
}

} // namespace ns3