#include "../seqraii.h"
#include <gtest/gtest.h>
#include <vector>

using namespace sequentialraii;

/**
 * Test order of initialization. Make sure lambdas are executed in the
 * order they are added.
 */
TEST(seqraii, test_initialization_order)
{
    std::vector<int> counter;

    // Initialization steps pushing 0 to 9 into a vector.
    SequentialRaii seqraii;

    for (int i = 0; i < 10; ++i)
    {
        seqraii.addStep(
            [&counter, i]()
            {
                counter.push_back(i);
                return true;
            });
    }

    seqraii.initialize();
    EXPECT_EQ(10, counter.size());

    // Verify values.
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(i, counter[i]);
    }
}

/**
 * Test order of uninitialization. Make sure lambdas are run in the
 * reverse order they are added.
 */
TEST(seqraii, test_uninitialization_order)
{
    std::vector<int> counter;

    // Uninitialization steps pushing 9 to 0 into a vector.
    SequentialRaii seqraii;

    for (int i = 0; i < 10; ++i)
    {
        seqraii.addStep([](){return true;},
            [&counter, i]()
            {
                counter.push_back(i);
                return true;
            });
    }

    seqraii.initialize();
    EXPECT_EQ(0, counter.size());

    seqraii.uninitialize();

    // Verify values, note reverse order.
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(i, counter[9 - i]);
    }

    EXPECT_EQ(10, counter.size());
}

/**
 * Test failed initialization sequence where one of the initalization
 * steps fails by returning false. Make sure no further initialization steps
 * are executed.
 */
TEST(seqraii, test_failed_initialization_nothrow)
{
    std::vector<int> counter;

    SequentialRaii seqraii;

    seqraii.addStep([&]() {counter.push_back(0); return true;});
    seqraii.addStep([&]() {counter.push_back(1); return false;});
    seqraii.addStep([&]() {counter.push_back(2); return true;});

    EXPECT_NO_THROW({seqraii.initialize();});
    EXPECT_EQ(2, counter.size());

    // Verify values.
    for (int i = 0; i < 2; ++i)
    {
        EXPECT_EQ(i, counter[i]);
    }
}

/**
 * Test failed initialization sequence where one of the initalization
 * steps fails by throwing. Make sure no further initialization steps
 * are executed.
 */
TEST(seqraii, test_failed_initialization_throw)
{
    std::vector<int> counter;

    SequentialRaii seqraii;

    seqraii.addStep([&]() {counter.push_back(0); return true;});
    seqraii.addStep([&]() {counter.push_back(1); throw std::runtime_error(""); return true;});
    seqraii.addStep([&]() {counter.push_back(2); return true;});

    EXPECT_NO_THROW({seqraii.initialize();});
    EXPECT_EQ(2, counter.size());

    // Verify values.
    for (int i = 0; i < 2; ++i)
    {
        EXPECT_EQ(i, counter[i]);
    }
}

/**
 * Test automatic cleanup on failed initialization.
 * When initialize() returns false uninitialize() should be executed automatically.
 */
TEST(seqraii, test_uninitialization_on_failure)
{
    bool didCleanup = false;

    SequentialRaii seqraii;

    // We need two steps because uninitalization code is not executed for the step
    // that fails.
    seqraii.addStep([&]() {return true;}, [&]() {didCleanup = true;});
    seqraii.addStep([&]() {return false;});

    seqraii.initialize();
    EXPECT_TRUE(didCleanup);
}

/**
 * Test automatic cleanup when leaving scope.
 */
TEST(seqraii, test_uninitialization_on_scope_exit)
{
    bool didCleanup = false;

    {
        SequentialRaii seqraii;

        seqraii.addStep([&]() {return true;}, [&]() {didCleanup = true;});
        seqraii.initialize();
    }

    EXPECT_TRUE(didCleanup);
}

/**
 * Test automatic uninitalization on move operation.
 * Uninitialization code should run for the destination object.
 */
TEST(seqraii, test_uninitialization_on_move)
{
    bool didCleanup = false;

    SequentialRaii seqraii;

    seqraii.addStep([&]() {return true;}, [&]() {didCleanup = true;});
    seqraii.initialize();

    ASSERT_FALSE(didCleanup);
    seqraii = SequentialRaii{}; // Will do a move-assignment.
    EXPECT_TRUE(didCleanup);
}

/**
 * Test general behaviour of move operation.
 * State should be moved from source to destination object.
 */
TEST(seqraii, test_move_operation)
{
    bool didCleanup = false;

    SequentialRaii seqraii;

    seqraii.addStep([&]() {return true;}, [&]() {didCleanup = true;});
    seqraii.initialize();

    // Do a move construct.
    SequentialRaii targetSeqraii(std::move(seqraii));

    ASSERT_FALSE(didCleanup);
    targetSeqraii.uninitialize();
    EXPECT_TRUE(didCleanup);
}


int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
