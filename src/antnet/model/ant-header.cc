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

AntHeader::AntHeader()
{
    // Initialize default values if needed
    m_type = FORWARD_ANT;
    m_sourceNodeId = 0;
    m_sourceAddress = Ipv4Address("0.0.0.0");
    m_destinationAddress = Ipv4Address("0.0.0.0");
    m_round = 0;
    m_forwardStack.clear();
    m_backwardStack.clear();
}

AntHeader::AntHeader(
    Type type,
    uint32_t sourceNodeId,
    Ipv4Address source,
    Ipv4Address destination,
    uint32_t round
)  : m_type(type),
    m_forwardStack(),
    m_backwardStack(),
    m_sourceNodeId(sourceNodeId),
    m_sourceAddress(source),
    m_destinationAddress(destination),
    m_destinationAlternativeAddresses(),
    m_round(round)
{
}

std::string 
AntHeader::ToString() const
{
    std::ostringstream oss;
    Print(oss);
    return oss.str();
}

uint32_t
AntHeader::GetSerializedSize() const
{
    // 1 byte for type + 4 bytes for stack size * 2 for two stacks + (4 bytes for node ID + 4 bytes for IP + 8 bytes for time) per entry
    uint32_t typeSize = 1;
    uint32_t stackSizeSize = 4;
    uint32_t stackEntrySize = 4 + 4 + 4 + 8;
    uint32_t sourceNodeIdSize = 4;
    uint32_t addressSize = 4;
    uint32_t roundSize = 4;
    uint32_t destinationAddrListSize = 4;

    uint32_t totalSize = typeSize
                         + stackSizeSize * 2
                         + (m_forwardStack.size() + m_backwardStack.size()) * stackEntrySize
                         + sourceNodeIdSize
                         + addressSize * 2
                         + roundSize
                         + destinationAddrListSize
                         + m_destinationAlternativeAddresses.size() * addressSize;
    return totalSize;
}

void
AntHeader::Serialize(Buffer::Iterator start) const
{
    // Serialize ant type
    start.WriteU8(static_cast<uint8_t>(m_type));

    // Serialize forward stack
    start.WriteHtonU32(m_forwardStack.size());
    for (uint32_t i = 0; i < m_forwardStack.size(); ++i) {
        // Serialize node ID
        start.WriteHtonU32(m_forwardStack[i].GetNodeId());

        // Serialize address
        start.WriteHtonU32(m_forwardStack[i].GetAddressIn().Get());
        start.WriteHtonU32(m_forwardStack[i].GetAddressOut().Get());

        // Serialize time
        int64_t t = m_forwardStack[i].GetTime().GetNanoSeconds ();
        start.WriteHtonU64 (static_cast<uint64_t>(t));
    }

    // Serialize backward stack
    start.WriteHtonU32(m_backwardStack.size());
    for (uint32_t i = 0; i < m_backwardStack.size(); ++i) {
        // Serialize node ID
        start.WriteHtonU32(m_backwardStack[i].GetNodeId());

        // Serialize address
        start.WriteHtonU32(m_backwardStack[i].GetAddressIn().Get());
        start.WriteHtonU32(m_backwardStack[i].GetAddressOut().Get());

        // Serialize time
        int64_t t = m_backwardStack[i].GetTime().GetNanoSeconds ();
        start.WriteHtonU64 (static_cast<uint64_t>(t));
    }

    start.WriteHtonU32(m_destinationAlternativeAddresses.size());
    for (const auto& addr : m_destinationAlternativeAddresses) {
        start.WriteHtonU32(addr.Get());
    }

    // Serialize source node ID
    start.WriteHtonU32(m_sourceNodeId);

    // Serialize source address
    start.WriteHtonU32(m_sourceAddress.Get());

    // Serialize destination address
    start.WriteHtonU32(m_destinationAddress.Get());

    // Serialize round
    start.WriteHtonU32(m_round);
}

uint32_t
AntHeader::Deserialize(Buffer::Iterator start)
{
    // Deserialize ant type
    m_type = static_cast<AntHeader::Type>(start.ReadU8());

    // Deserialize forward stack
    m_forwardStack.clear();
    // Deserialize size
    uint32_t forwardStackSize = start.ReadNtohU32();
    for (uint32_t i = 0; i < forwardStackSize; ++i) {
        // Deserialize node ID
        uint32_t nodeId = start.ReadNtohU32();

        // Deserialize address
        uint32_t ipInInt = start.ReadNtohU32();
        uint32_t ipOutInt = start.ReadNtohU32();
        // Deserialize time
        uint64_t v = start.ReadNtohU64 ();
        int64_t t = static_cast<int64_t> (v);

        // Add entry back to m_forwardStack
        m_forwardStack.push_back(
            AntHeaderStackEntry(
                nodeId,
                Ipv4Address(ipInInt), 
                Ipv4Address(ipOutInt),
                Time(NanoSeconds(t))
            )
        );
    }

    // Deserialize backward stack
    m_backwardStack.clear();
    // Deserialize size
    uint32_t backwardStackSize = start.ReadNtohU32();
    for (uint32_t i = 0; i < backwardStackSize; ++i) {
        // Deserialize node ID
        uint32_t nodeId = start.ReadNtohU32();

        // Deserialize address
        uint32_t ipInInt = start.ReadNtohU32();
        uint32_t ipOutInt = start.ReadNtohU32();
        // Deserialize time
        uint64_t v = start.ReadNtohU64 ();
        int64_t t = static_cast<int64_t> (v);

        // Add entry back to m_backwardStack
        m_backwardStack.push_back(
            AntHeaderStackEntry(
                nodeId,
                Ipv4Address(ipInInt), 
                Ipv4Address(ipOutInt),
                Time(NanoSeconds(t))
            )
        );
    }

    // Deserialize alternative destination addresses
    m_destinationAlternativeAddresses.clear();
    uint32_t altAddrCount = start.ReadNtohU32();
    for (uint32_t i = 0; i < altAddrCount; ++i) {
        uint32_t altIpInt = start.ReadNtohU32();
        m_destinationAlternativeAddresses.push_back(Ipv4Address(altIpInt));
    }

    // Deserialize source node ID
    m_sourceNodeId = start.ReadNtohU32();

    // Deserialize source address
    uint32_t srcIpInt = start.ReadNtohU32();
    m_sourceAddress.Set(srcIpInt);

    // Deserialize destination address
    uint32_t destIpInt = start.ReadNtohU32();
    m_destinationAddress.Set(destIpInt);

    // Deserialize round
    m_round = start.ReadNtohU32();

    // Return total size consumed
    return 1 + 4 * 2 + (forwardStackSize + backwardStackSize) * (4 + 4 + 8) + 4 + 4 * 2 + 4 + 4 + altAddrCount * 4;
}

void
AntHeader::Print(std::ostream& os) const
{
    AntHeader::Type type = m_type;
    if (type == FORWARD_ANT) {
        os << "AntHeader[type=FORWARD_ANT, ";
    } else if (type == BACKWARD_ANT) {
        os << "AntHeader[type=BACKWARD_ANT, ";
    } else if (type == BEACON_ANT) {
        os << "AntHeader[type=BEACON_ANT, ";
    } else if (type == FAILURE_MESSAGE_ANT) {
        os << "AntHeader[type=FAILURE_MESSAGE_ANT, ";
    } else {
        os << "AntHeader[type=UNKNOWN, ";
    }
    
    os << "sourceNodeId=" << m_sourceNodeId << ", ";
    os << "sourceAddress=" << m_sourceAddress << ", ";
    os << "destinationAddress=" << m_destinationAddress << ", ";
    if (m_type == BACKWARD_ANT) {
        os << "destinationAlternativeAddresses=[";
        for (auto it = m_destinationAlternativeAddresses.begin(); it != m_destinationAlternativeAddresses.end(); ++it) {
            os << *it;
            if (std::next(it) != m_destinationAlternativeAddresses.end()) {
                os << ", ";
            }
        }
        os << "], ";
    }
    os << "round=" << m_round << "]\n";

    if (type == FORWARD_ANT || type == BACKWARD_ANT) {
        os << "    forwardPath=";
        for (uint32_t k = 0; k < m_forwardStack.size(); ++k) {
            os << "(" << m_forwardStack[k].GetNodeId() << ":" << m_forwardStack[k].GetAddressIn() << "-" << m_forwardStack[k].GetAddressOut() << ":" << m_forwardStack[k].GetTime() << "ms)";
            if (k != m_forwardStack.size() - 1) {
                os << " -> ";
            }
        }
        os << "\n";

        os << "    returnPath=";
        for (uint32_t k = 0; k < m_backwardStack.size(); ++k) {
            os << "(" << m_backwardStack[k].GetNodeId() << ":" << m_backwardStack[k].GetAddressIn() << "-" << m_backwardStack[k].GetAddressOut() << ":" << m_backwardStack[k].GetTime() << "ms)";
            if (k != m_backwardStack.size() - 1) {
                os << " -> ";
            }
        }
        os << "";
    }
}

void
AntHeader::AddForwardHop(uint32_t hopNodeId, Ipv4Address hopInAddress, Ipv4Address hopOutAddress, Time time)
{
    m_forwardStack.push_back(AntHeaderStackEntry(hopNodeId, hopInAddress, hopOutAddress, time));
}

AntHeader::AntHeaderStackEntry
AntHeader::PopForwardStackEntryToBackwardStack()
{
    if (m_forwardStack.empty()) {
        NS_LOG_ERROR("Forward stack is empty, cannot pop entry.");
        return AntHeaderStackEntry(0, Ipv4Address("0.0.0.0"), Ipv4Address("0.0.0.0"), Time(Seconds(0)));
    }

    AntHeaderStackEntry top = m_forwardStack.back();

    // Remove from m_forwardStack and add to m_backwardStack
    m_forwardStack.pop_back();
    m_backwardStack.push_back(top);

    return top;
}

std::pair<bool, Ipv4Address> 
AntHeader::DetectAndPopForwardStackCycle(uint32_t currentNodeId, Time totalAntTravelTime)
{
    if (m_forwardStack.empty()) {
        // No entries in forward stack, no cycle
        return std::make_pair(false, Ipv4Address("0.0.0.0"));
    }

    if (m_sourceNodeId == currentNodeId) {
        // Forward ant has returned to source node, drop the ant since it it a full cycle
        return std::make_pair(true, Ipv4Address("0.0.0.0"));
    }

    // Iterate from top to bottom of the stack to find cycles
    bool cycleFound = false;
    Ipv4Address cycleStartAddress = Ipv4Address("0.0.0.0");
    Time cycleStartTime = Time(Seconds(0));
    for (int32_t i = m_forwardStack.size() - 1; i >= 0; --i) {
        if (m_forwardStack[i].GetNodeId() == currentNodeId) {
            cycleFound = true;
            cycleStartTime = m_forwardStack[i].GetTime();
            cycleStartAddress = m_forwardStack[i].GetAddressIn();
            // Remove all entries from i to end
            m_forwardStack.erase(m_forwardStack.begin() + i, m_forwardStack.end());
            break;
        }
    }

    if (cycleFound) {
        // Check if cycle time is more than half of total ant travel time
        Time cycleTime = totalAntTravelTime - cycleStartTime;
        if (cycleTime.GetNanoSeconds() > (totalAntTravelTime.GetNanoSeconds() / 2)) {
            return std::make_pair(false, Ipv4Address("0.0.0.0")); // Drop the ant
        } else {
            // If cycle is short, return true with cycle start address (incoming address at which cycle started)
            return std::make_pair(true, cycleStartAddress);
        }
    }

    // No cycle detected, return false
    return std::make_pair(false, cycleStartAddress);
}

void
AntHeader::SetAntType(AntHeader::Type type)
{
    m_type = type;
}

void
AntHeader::AddDestinationAlternativeAddress(Ipv4Address addr)
{
    if (std::find(m_destinationAlternativeAddresses.begin(), m_destinationAlternativeAddresses.end(), addr) == m_destinationAlternativeAddresses.end()) {
        m_destinationAlternativeAddresses.push_back(addr);
    }
}

std::list<Ipv4Address>
AntHeader::GetDestinationAlternativeAddresses() const
{
    return m_destinationAlternativeAddresses;
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