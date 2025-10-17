#include "antnet-helper.h"

#include "ns3/antnet-routing-protocol.h"
#include "ns3/names.h"
#include "ns3/log.h"
#include "ns3/node.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("AntNetHelper");

AntNetHelper::AntNetHelper() {
  m_agentFactory.SetTypeId("ns3::AntNetRoutingProtocol");
}

AntNetHelper::AntNetHelper(const AntNetHelper &o) : Ipv4RoutingHelper(o) {
  m_agentFactory = o.m_agentFactory;
}

AntNetHelper* AntNetHelper::Copy() const {
  return new AntNetHelper(*this);
}

Ptr<Ipv4RoutingProtocol> AntNetHelper::Create(Ptr<Node> node) const {
  Ptr<AntNetRoutingProtocol> agent = m_agentFactory.Create<AntNetRoutingProtocol>();
  node->AggregateObject(agent);
  return agent;
}

void AntNetHelper::Set(std::string name, const AttributeValue &value) {
  m_agentFactory.Set(name, value);
}

} // namespace ns3
