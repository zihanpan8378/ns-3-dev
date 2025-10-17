#include "ns3/ant-headers.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("AntHeaders");
NS_OBJECT_ENSURE_REGISTERED(AntHeader);

AntHeader::AntHeader()
  : m_type(ANT_FORWARD), m_src(), m_dst(), m_id(0), m_launchTime(0.0) {}

TypeId AntHeader::GetTypeId() {
  static TypeId tid = TypeId("ns3::AntHeader")
    .SetParent<Header>()
    .SetGroupName("Internet");
  return tid;
}

TypeId AntHeader::GetInstanceTypeId() const { return GetTypeId(); }

void AntHeader::Print(std::ostream& os) const {
  os << (m_type == ANT_FORWARD ? "FWD" : "BWD")
     << " id=" << m_id << " " << m_src << "->" << m_dst
     << " t0=" << m_launchTime << " hops=" << m_path.size();
}

uint32_t AntHeader::GetSerializedSize() const {
  return 1 + 4 + 8 + 4 + 4 + 2 + 4 * m_path.size();
}

void AntHeader::Serialize(Buffer::Iterator i) const {
  i.WriteU8(static_cast<uint8_t>(m_type));
  i.WriteHtonU32(m_id);
  union { double d; uint64_t u; } u;
  u.d = m_launchTime;
  i.WriteHtonU64(u.u);
  WriteTo(i, m_src);
  WriteTo(i, m_dst);
  i.WriteHtonU16(static_cast<uint16_t>(m_path.size()));
  for (auto const& a : m_path) {
    WriteTo(i, a);
  }
}

uint32_t AntHeader::Deserialize(Buffer::Iterator i) {
  uint8_t t = i.ReadU8();
  m_type = static_cast<AntType>(t);
  m_id = i.ReadNtohU32();
  union { double d; uint64_t u; } u;
  u.u = i.ReadNtohU64();
  m_launchTime = u.d;
  ReadFrom(i, m_src);
  ReadFrom(i, m_dst);
  uint16_t len = i.ReadNtohU16();
  m_path.clear();
  for (uint16_t k = 0; k < len; ++k) {
    Ipv4Address a; ReadFrom(i, a); m_path.push_back(a);
  }
  return GetSerializedSize();
}

void AntHeader::SetType(AntType t) { m_type = t; }
AntType AntHeader::GetType() const { return m_type; }

void AntHeader::SetSrc(Ipv4Address a) { m_src = a; }
void AntHeader::SetDst(Ipv4Address a) { m_dst = a; }
Ipv4Address AntHeader::GetSrc() const { return m_src; }
Ipv4Address AntHeader::GetDst() const { return m_dst; }

void AntHeader::SetId(uint32_t id) { m_id = id; }
uint32_t AntHeader::GetId() const { return m_id; }

void AntHeader::SetLaunchTime(double t) { m_launchTime = t; }
double AntHeader::GetLaunchTime() const { return m_launchTime; }

void AntHeader::PushHop(Ipv4Address addr) { m_path.push_back(addr); }
bool AntHeader::PopHop(Ipv4Address &addr) {
  if (m_path.empty()) return false;
  addr = m_path.back();
  m_path.pop_back();
  return true;
}
const std::vector<Ipv4Address>& GetPath();
const std::vector<Ipv4Address>& AntHeader::GetPath() const { return m_path; }
void AntHeader::SetPath(const std::vector<Ipv4Address>& p) { m_path = p; }

} // namespace ns3
