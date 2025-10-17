#ifndef ANNET_HELPER_H
#define ANNET_HELPER_H

#include "ns3/ipv4-routing-helper.h"
#include "ns3/object-factory.h"

namespace ns3 {

class AntNetRoutingProtocol;

class AntNetHelper : public Ipv4RoutingHelper
{
public:
  AntNetHelper();
  AntNetHelper(const AntNetHelper &);
  AntNetHelper* Copy() const override;

  Ptr<Ipv4RoutingProtocol> Create(Ptr<Node> node) const override;

  void Set(std::string name, const AttributeValue &value);

private:
  ObjectFactory m_agentFactory;
};

} // namespace ns3

#endif // ANNET_HELPER_H
