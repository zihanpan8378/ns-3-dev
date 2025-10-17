#ifndef ANT_HEADERS_H
#define ANT_HEADERS_H

#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include <vector>

namespace ns3 {

enum AntType : uint8_t { ANT_FORWARD = 1, ANT_BACKWARD = 2 };

class AntHeader : public Header
{
public:
  AntHeader();
  static TypeId GetTypeId();
  virtual TypeId GetInstanceTypeId() const;
  virtual void Print(std::ostream &os) const;
  virtual uint32_t GetSerializedSize() const;
  virtual void Serialize(Buffer::Iterator start) const;
  virtual uint32_t Deserialize(Buffer::Iterator start);

  void SetType(AntType t);
  AntType GetType() const;

  void SetSrc(Ipv4Address a);
  void SetDst(Ipv4Address a);
  Ipv4Address GetSrc() const;
  Ipv4Address GetDst() const;

  void SetId(uint32_t id);
  uint32_t GetId() const;

  void SetLaunchTime(double t);
  double GetLaunchTime() const;

  void PushHop(Ipv4Address addr);
  bool PopHop(Ipv4Address &addr);
  const std::vector<Ipv4Address>& GetPath() const;
  void SetPath(const std::vector<Ipv4Address>& p);

private:
  AntType m_type;
  Ipv4Address m_src;
  Ipv4Address m_dst;
  uint32_t m_id;
  double m_launchTime; // seconds
  std::vector<Ipv4Address> m_path; // reverse path for backward ants (top is back())
};

} // namespace ns3

#endif // ANT_HEADERS_H
