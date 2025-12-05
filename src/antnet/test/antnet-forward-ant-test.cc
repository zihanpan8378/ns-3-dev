#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/simulator.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "../model/ipv4-antnet-routing.h"

#include <vector>
#include <cstdlib>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AntNetForwardAntTest");

// ----------------------------------------------------------------------------
// TestAntNetRouting: override SendForwardAnt and only record which destinations were chosen
// ----------------------------------------------------------------------------

class TestAntNetRouting : public Ipv4AntNetRouting
{
public:
  static TypeId GetTypeId (void) {
    static TypeId tid = TypeId ("ns3::TestAntNetRouting")
      .SetParent<Ipv4AntNetRouting> ()
      .SetGroupName ("AntNet")
      .AddConstructor<TestAntNetRouting> ();
    return tid;
  }

  TestAntNetRouting () = default;
  ~TestAntNetRouting () override = default;

  void ClearStats () {
    m_sentDests.clear ();
  }

  const std::vector<Ipv4Address> &GetSentDests () const {
    return m_sentDests;
  }

  // Small helper that forwards to the base class AddLocalTrafficStat
  void AddLocalTrafficStatEntry (Ipv4Address dest, double dataFlowMeasure) {
    AddLocalTrafficStat (dest, dataFlowMeasure);
  }

protected:
  // Override: do not actually send packets, just record the chosen destination
  void SendForwardAnt (Ipv4Address dest) override {
    m_sentDests.push_back (dest);
    NS_LOG_INFO ("TestAntNetRouting::SendForwardAnt dest=" << dest);
  }

private:
  std::vector<Ipv4Address> m_sentDests;
};

NS_OBJECT_ENSURE_REGISTERED (TestAntNetRouting);


class ForwardAntScheduleTestCase : public TestCase
{
public:
  ForwardAntScheduleTestCase () : TestCase ("Test Ipv4AntNetRouting::ScheduleForwardAnt weighted selection") {
  }

private:
  void DoRun () override{
    std::srand (1);

    Ptr<TestAntNetRouting> routing = CreateObject<TestAntNetRouting> ();
    routing->ClearStats ();


    routing->AddLocalTrafficStatEntry (Ipv4Address ("10.0.0.2"), 1.0);
    routing->AddLocalTrafficStatEntry (Ipv4Address ("10.0.0.3"), 3.0);

    const uint32_t runTimes = 1000;

    for (uint32_t i = 0; i < runTimes; ++i) {
        routing->ScheduleForwardAnt();
    }

    const std::vector<Ipv4Address> &dests = routing->GetSentDests ();

    NS_TEST_ASSERT_MSG_EQ (dests.size (), runTimes, "Each ScheduleForwardAnt call should send exactly one ant");

    uint32_t count2 = 0;
    uint32_t count3 = 0;

    for (const auto &addr : dests) {
        if (addr == Ipv4Address ("10.0.0.2")) {
            ++count2;
        } else if (addr == Ipv4Address ("10.0.0.3")) {
            ++count3;
        } else{
            NS_TEST_ASSERT_MSG_EQ (true, false, "Unexpected destination in SendForwardAnt: " << addr);
        }
    }

    NS_LOG_INFO ("count(10.0.0.2) = " << count2 << ", count(10.0.0.3) = " << count3);

    NS_TEST_ASSERT_MSG_GT (count2, 0u, "Destination 10.0.0.2 should be chosen at least once");
    NS_TEST_ASSERT_MSG_GT (count3, 0u, "Destination 10.0.0.3 should be chosen at least once");

    NS_TEST_ASSERT_MSG_GT (count3, count2, "10.0.0.3 (weight 3) should be chosen more often than 10.0.0.2 (weight 1)");

    double ratio = static_cast<double> (count3) / static_cast<double> (count2);

    NS_TEST_ASSERT_MSG_GT (ratio, 1.5, "Selection ratio count3/count2 is too small; weighting may be wrong");
    NS_TEST_ASSERT_MSG_LT (ratio, 5.0, "Selection ratio count3/count2 is too large; something may be wrong");

    Simulator::Destroy ();
  }
};

// ----------------------------------------------------------------------------
// TestSuite registration
// ----------------------------------------------------------------------------

class AntNetForwardAntSuite : public TestSuite
{
public:
  AntNetForwardAntSuite ()
    : TestSuite ("antnet-forward-ant-suite", Type::UNIT)
  {
    AddTestCase (new ForwardAntScheduleTestCase, TestCase::Duration::QUICK);
  }
};

static AntNetForwardAntSuite g_antNetForwardAntSuite;
