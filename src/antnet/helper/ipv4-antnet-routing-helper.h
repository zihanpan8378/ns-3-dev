#ifndef IPV4_ANTNET_ROUTING_HELPER_H
#define IPV4_ANTNET_ROUTING_HELPER_H

#include "ns3/ipv4-routing-helper.h"

#include "ns3/node-container.h"

namespace ns3
{

class AntNetTopolocyDb: public Object
{ 
    public:
        static TypeId GetTypeId();

};

class Ipv4AntNetRoutingHelper : public Ipv4RoutingHelper
{
    public:

        Ipv4AntNetRoutingHelper();

        Ipv4AntNetRoutingHelper(const Ipv4AntNetRoutingHelper& o);

        Ipv4AntNetRoutingHelper& operator=(const Ipv4AntNetRoutingHelper&) = delete;

        Ipv4AntNetRoutingHelper* Copy() const override;

        Ptr<Ipv4RoutingProtocol> Create(Ptr<Node> node) const override;

        static void BuildAntNetTopology();

        static void InitializeNodeRoutingTables();
        
    private:
        static std::map<ns3::Ptr<ns3::Node>, std::list<std::pair<Ipv4Address, Ipv4Address>>> m_neighbourAdjList;
        static std::list<std::pair<Ipv4Address, Ipv4Address>> m_nodeList;
};

} // namespace ns3

#endif /* IPV4_ANTNET_ROUTING_HELPER_H */