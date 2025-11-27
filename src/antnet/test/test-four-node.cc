#include "ns3/test.h"

using namespace ns3;


class FourNodeCase : public TestCase
{
  public:
    /**
     * The constructor of the test case
     */
    FourNodeCase()
        : TestCase("FourNode test case")
    {
    }

  private:
    /**
     * Run the test
     */
    void DoRun() override;
};

class FourNodeSuite : public TestSuite
{
  public:
    FourNodeSuite();
};

FourNodeSuite::FourNodeSuite()
    : TestSuite("four-node-suite", Type::UNIT)
{
    AddTestCase(new FourNodeCase(), TestCase::Duration::QUICK);
}

static FourNodeSuite staticFourNodeSuite;

void FourNodeCase
    ::DoRun()
{
    NS_TEST_ASSERT_MSG_EQ(1, 2, "try fail test");
    // NS_TEST_ASSERT_MSG_EQ(1, 1, "try success test");
}