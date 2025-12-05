/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Tests for Ipv4AntNetRoutingTableEntry pheromone update and evaporation.
 *
 * This file focuses purely on the routing table entry logic:
 *  - UpdatePheromone(nextHop, delayMs, localStats)
 *  - EvaporatePheromone(neighbour, evaporationFactor)
 *
 * We DO NOT assert specific absolute pheromone values.
 * We check robust properties instead:
 *  - UpdatePheromone keeps pheromones non-negative and actually changes them.
 *  - For the same initial state, a smaller delay (better path) yields a
 *    larger pheromone for that next hop than a larger delay.
 *  - EvaporatePheromone reduces the pheromone for the given neighbour and
 *    keeps all pheromones non-negative.
 */

#include "ns3/test.h"
#include "ns3/core-module.h"
#include "ns3/ipv4-address.h"

#include "../model/ipv4-antnet-routing-table-entry.h"
#include "../model/ipv4-antnet-local-traffic-statistics-entry.h"

#include <cmath>

using namespace ns3;

static double
GetPheromoneFor (const Ipv4AntNetRoutingTableEntry::PheromoneList &list, Ipv4Address neighbour, uint32_t iface) {
  for (const auto &p : list)
    {
      const auto &key = p.first;
      if (key.first == neighbour && key.second == iface)
        {
          return p.second;
        }
    }
  return -1.0;
}

class PheromoneUpdateTestCase : public TestCase
{
public:
  PheromoneUpdateTestCase () : TestCase ("Test pheromone reinforcement for good vs bad delay (trend only)") {
  }

private:
  void
  DoRun () override {
    Ipv4Address dest ("10.0.0.9");
    Ipv4Mask mask ("255.255.255.255");

    Ipv4Address n1 ("10.0.0.1");
    Ipv4Address n2 ("10.0.0.2");
    uint32_t iface1 = 1;
    uint32_t iface2 = 2;

    Ipv4AntNetRoutingTableEntry::PheromoneList baseList;
    baseList.push_back (std::make_pair (Ipv4AntNetRoutingTableEntry::PheromoneKey (n1, iface1), 0.5));
    baseList.push_back (std::make_pair (Ipv4AntNetRoutingTableEntry::PheromoneKey (n2, iface2), 0.5));


    Ipv4AntNetLocalTrafficStatisticsEntry stats (dest, mask);
    for (int i = 0; i < 20; ++i) {
        stats.UpdateStatistics (10.0);
    }

    const double eps = 1e-12;

    double initialN1 = GetPheromoneFor (baseList, n1, iface1);
    NS_TEST_ASSERT_MSG_GT (initialN1, -eps, "Initial pheromone for n1 should be non-negative");

    Ipv4AntNetRoutingTableEntry entryGood (dest, mask, baseList);
    Ipv4AntNetRoutingTableEntry entryBad  (dest, mask, baseList);

    double goodDelayMs = 5.0;
    double badDelayMs  = 100.0;

    entryGood.UpdatePheromone (n1, goodDelayMs, stats);
    entryBad.UpdatePheromone  (n1, badDelayMs,  stats);

    auto listGood = entryGood.GetPheromoneList ();
    auto listBad  = entryBad.GetPheromoneList ();

    double goodN1 = GetPheromoneFor (listGood, n1, iface1);
    double badN1  = GetPheromoneFor (listBad,  n1, iface1);
    double goodN2 = GetPheromoneFor (listGood, n2, iface2);
    double badN2  = GetPheromoneFor (listBad,  n2, iface2);

    NS_TEST_ASSERT_MSG_GT (goodN1, -eps, "Pheromone for n1 after good delay should be non-negative");
    NS_TEST_ASSERT_MSG_GT (goodN2, -eps, "Pheromone for n2 after good delay should be non-negative");
    NS_TEST_ASSERT_MSG_GT (badN1, -eps, "Pheromone for n1 after bad delay should be non-negative");
    NS_TEST_ASSERT_MSG_GT (badN2, -eps, "Pheromone for n2 after bad delay should be non-negative");

    NS_TEST_ASSERT_MSG_NE (goodN1, initialN1, "Good delay update should change pheromone for n1");
    NS_TEST_ASSERT_MSG_NE (badN1, initialN1, "Bad delay update should change pheromone for n1");

    // With the same initial state, a smaller delay should give a higher pheromone.
    NS_TEST_ASSERT_MSG_GT (goodN1, badN1,"For the same starting distribution, a smaller delay must yield a larger pheromone for that next hop than a larger delay");
  }
};

/**
 * TestCase: verify that EvaporatePheromone reduces pheromone for that neighbour
 * and does not make pheromones negative.
 */
class PheromoneEvaporationTestCase : public TestCase
{
public:
  PheromoneEvaporationTestCase () : TestCase ("Test pheromone evaporation for a given neighbour") {
  }

private:
  void
  DoRun () override {
    Ipv4Address dest ("10.0.0.9");
    Ipv4Mask mask ("255.255.255.255");

    Ipv4Address n1 ("10.0.0.1");
    Ipv4Address n2 ("10.0.0.2");
    uint32_t iface1 = 1;
    uint32_t iface2 = 2;

    Ipv4AntNetRoutingTableEntry::PheromoneList list;
    list.push_back (std::make_pair (Ipv4AntNetRoutingTableEntry::PheromoneKey (n1, iface1), 0.6));
    list.push_back (std::make_pair (Ipv4AntNetRoutingTableEntry::PheromoneKey (n2, iface2), 0.4));

    Ipv4AntNetRoutingTableEntry entry (dest, mask, list);

    auto before = entry.GetPheromoneList ();
    double beforeN1 = GetPheromoneFor (before, n1, iface1);
    double beforeN2 = GetPheromoneFor (before, n2, iface2);

    const double eps = 1e-12;
    NS_TEST_ASSERT_MSG_GT (beforeN1, -eps, "Initial pheromone for n1 should be non-negative");
    NS_TEST_ASSERT_MSG_GT (beforeN2, -eps, "Initial pheromone for n2 should be non-negative");

    double factor = 0.2;
    double effect = entry.EvaporatePheromone (n1, factor);

    auto after = entry.GetPheromoneList ();
    double afterN1 = GetPheromoneFor (after, n1, iface1);
    double afterN2 = GetPheromoneFor (after, n2, iface2);

    NS_TEST_ASSERT_MSG_LT (afterN1, beforeN1, "Evaporation should reduce pheromone on neighbour n1");

    NS_TEST_ASSERT_MSG_GT (afterN1, -eps, "Pheromone for n1 after evaporation should stay non-negative");
    NS_TEST_ASSERT_MSG_GT (afterN2, -eps, "Pheromone for n2 after evaporation should stay non-negative");

    NS_TEST_ASSERT_MSG_GT (effect, -eps, "Evaporation effect should be >= 0");
  }
};

/**
 * TestSuite registration.
 */
class AntNetPheromoneSuite : public TestSuite
{
public:
  AntNetPheromoneSuite ()
    : TestSuite ("antnet-pheromone-suite", Type::UNIT)
  {
    AddTestCase (new PheromoneUpdateTestCase, TestCase::Duration::QUICK);
    AddTestCase (new PheromoneEvaporationTestCase, TestCase::Duration::QUICK);
  }
};

static AntNetPheromoneSuite g_antNetPheromoneSuite;
