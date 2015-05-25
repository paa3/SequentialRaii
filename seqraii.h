/**
 * Sequential resource allocation and initialization.
 * 
 * Copyright 2015 Torjus Breisjøberg.
 * License: MIT license. See separate file for full disclaimer.
 * Compile with C++14 enabled.
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
