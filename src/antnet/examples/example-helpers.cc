#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include "ns3/ipv4-antnet-routing-helper.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

void FailNode(Ptr<Node> node) {
    NS_LOG_INFO("Failing node " << node->GetId());
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i) {
        ipv4->SetDown(i);
    }
}

void RecoverNode(Ptr<Node> node) {
    NS_LOG_INFO("Recovering node " << node->GetId());
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i) {
        ipv4->SetUp(i);
    }
}

void FailInterface(Ptr<Node> node, uint32_t interfaceIndex) {
    NS_LOG_INFO("Failing interface " << interfaceIndex << " with address " << node->GetObject<Ipv4>()->GetAddress(interfaceIndex, 0).GetLocal() << " on node " << node->GetId());
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    ipv4->SetDown(interfaceIndex);
}

void RecoverInterface(Ptr<Node> node, uint32_t interfaceIndex) {
    NS_LOG_INFO("Recovering interface " << interfaceIndex << " with address " << node->GetObject<Ipv4>()->GetAddress(interfaceIndex, 0).GetLocal() << " on node " << node->GetId());
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    ipv4->SetUp(interfaceIndex);
}

