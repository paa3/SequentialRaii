/**
 * Sequential resource allocation and initialization is commonly encountered when working with C provided APIs.
 * In general multiple steps must be taken when aquiring and preparing a resource for use. A common usecase would
 * be to create e.g. an UDP socket for communication:
 *   First the socket must be created, then we may need to configure it using ioctl or other API calls. Perhaps
 *   we must also do a server name look-up too, or memory must be allocated for a buffer before we are ready to
 *   use the socket.
 * All of these steps in preparing the resource must be done in a more or less strict order. They can also fail at
 * any point and we must be ready to perform a full cleanup of the preceeding steps to avoid locking up any resources.
 *
 * Ultimately we want to check for success or failure at each step and upon failure we want to undo all the previous steps
 * in the reverse order. When we're done using the resource, cleanup should be run as well. Common implementations
 * often result in a convoluted, error-prone approach. Enter SequentialRaii.
 *
 * Through extensive use of lambda expressions it offers the following advantages:
 * - All the code for initializing and cleaning up a resource is localized in space and time. The code is also specified
 *   only once. This results in increased maintainability and significantly reduced possibility for bugs.
 * - Offers automatic cleanup in correct inverse order.
 *
 * Lambda expressions aren't without a disadvantage:
 * - As with any lambda use special care must be taken to manage the lifetime of any variables captured by reference.
 *   For example, the following code will generate a runtime exception since c's destructor is run before seqraii's
 *   when leaving the scope:
 *     {
 *       SequentialRaii seqraii;
 *       std::vector<int> c;
 *       seqraii.addStep([&](){c.push_back(1); return true;}, [&](){c.pop_back();});
 *       seqraii.initialize();
 *     }
 *   The modified code below will run just fine within the limits defined by C++:
 *     {
 *       std::vector<int> c;    // Swapped this line with the one below
 *       SequentialRaii seqraii;
 *       seqraii.addStep([&](){c.push_back(1); return true;}, [&](){c.pop_back();});
 *       seqraii.initialize();
 *     }
 *
 * Other information:
 *   Copyright 2015 Torjus Breisjøberg.
 *   License: MIT license. See separate file for full disclaimer.
 *   Compile with C++14 enabled.
 */
#pragma once

#include <vector>
#include <memory>

namespace sequentialraii
{
/**
 * Base class used for storing templated objects inside a vector.
 */
class StepBase
{
public:
    virtual ~StepBase() noexcept = default;

    virtual bool init() noexcept = 0;
    virtual void uninit() noexcept = 0;
};

// Forward declaration.
template <class Init, class Uninit> class Step;

/**
 * Container class for sequential raii steps.
 */
class SequentialRaii
{
public:
    /**
     * Constructs a container for sequential initialization.
     */
    SequentialRaii() noexcept
        : m_steps()
    {}

    /**
     * Add that feeling of RAII by always cleaning up after us.
     */
    ~SequentialRaii() noexcept
    {
        uninitialize();
    }

    // Disable copy constructor and copy-assignment operator.
    SequentialRaii(const SequentialRaii&) = delete;
    SequentialRaii& operator=(const SequentialRaii&) = delete;

    // Enable move-semantics.
    SequentialRaii(SequentialRaii&& rhs) noexcept
    {
        uninitialize();
        std::swap(m_steps, rhs.m_steps);
    }

    SequentialRaii& operator=(SequentialRaii&& rhs) noexcept
    {
        uninitialize();
        std::swap(m_steps, rhs.m_steps);
        return *this;
    }

    /**
     * Adds a step to the initialization queue. Call initialize() to run any queued steps.
     * @param init Initialization lambda. Must return true on success, false or throw otherwise.
     * @param uninit Uninitialization lambda, can be omitted. Will be executed if the initialization
     *               lambda was successfully run.
     */
    template <class Init, class Uninit>
    void addStep(Init&& init, Uninit&& uninit)
    {
        m_steps.emplace_back(std::make_unique<Step<Init, Uninit>>(std::forward<Init>(init), std::forward<Uninit>(uninit)));
    }

    template <class Init>
    void addStep(Init&& init)
    {
        addStep(std::forward<Init>(init), [](){});
    }

    /**
     * Runs the initialization steps in the order they were added.
     * @return True if all steps were applied successfully, false otherwise. On error the
     *         corresponding uninitialization steps will be run to ensure a clean state.
     */
    bool initialize() const noexcept
    {
        for (const auto& step : m_steps)
        {
            if (!step->init())
            {
                uninitialize();
                return false;
            }
        }

        return true;
    }

    /**
     * Runs the uninitialization steps in the reverse order as they were added.
     */
    void uninitialize() const noexcept
    {
        for (auto it = m_steps.rbegin(); it != m_steps.rend(); ++it)
        {
            it->get()->uninit();
        }
    }

private:
    /**
     * Keep track of the steps and the order they are added in. Uses unique_ptr to enable
     * polymorphism and to ensure no copying is taking place during memory reallocations.
     */
    std::vector<std::unique_ptr<StepBase>> m_steps;
};


/**
 * Utility class holding onto the initialization and uninitialization code for each step. 
 */
template <class Init, class Uninit>
class Step final : public StepBase
{
public:
    Step(Init&& init_, Uninit&& uninit_) noexcept
        : m_init(std::forward<Init>(init_))
        , m_uninit(std::forward<Uninit>(uninit_))
    {}

    /**
     * Runs the initialization code for this step.
     * @return Returns true if initialization code ran without any errors, false otherwise.
     */
    virtual bool init() noexcept override
    {
        if (!m_isInitialized)
        {
            try
            {
                m_isInitialized = m_init();
            }
            catch (...)
            {
            }
        }

        return m_isInitialized;
    }

    /**
     * Runs the uninitialization code if initialization code has been executed previously.
     */
    virtual void uninit() noexcept override
    {
        if (m_isInitialized)
        {
            try
            {
                m_uninit();
            }
            catch (...)
            {
            }
        }

        m_isInitialized = false;
        return;
    }

private:
    /// State flag indicating whether initialization code has been executed or not.
    bool m_isInitialized = false;

    Init m_init;
    Uninit m_uninit;
};

} // Namespace sequentialraii
