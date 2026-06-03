#include "Thread.h"
#include "Debug.h"
#include "Util.h"

// FIX #1: WinRT base for apartment initialization
#include <winrt/base.h>

class ScopedThreadSleeper : public ScopedTimer
{
public:
    template <class T>
    explicit ScopedThreadSleeper(const T& duration) :
        ScopedTimer([duration](microseconds us)
        {
            const auto waitTime = duration - us;
            if (waitTime > microseconds::zero())
            {
                std::this_thread::sleep_for(waitTime);
            }
        })
    {}
};

ThreadLoop::ThreadLoop(const std::wstring& name)
    : name_(name)
{
}

ThreadLoop::~ThreadLoop()
{
    Stop();
}

void ThreadLoop::Start(const ThreadFunc& func, const microseconds& interval)
{
    if (isRunning_) return;

    loopFunc_  = func;
    interval_  = interval;
    isRunning_ = true;

    if (thread_.joinable())
    {
        Debug::Error(__FUNCTION__, " => Thread is running");
        thread_.join();
    }

    const bool doWinRt = initWinRt_;

    thread_ = std::thread([this, doWinRt]
    {
        // FIX #1: Initialize WinRT MTA apartment safely
        if (doWinRt)
        {
            try
            {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
            }
            catch (const winrt::hresult_error& e)
            {
                const HRESULT hr = e.code();
                if (hr != RPC_E_CHANGED_MODE && hr != S_FALSE)
                {
                    Debug::Error(__FUNCTION__, " => winrt::init_apartment failed: 0x",
                                 reinterpret_cast<const void*>(static_cast<uintptr_t>(static_cast<uint32_t>(hr))));
                }
            }
        }

        if (initializerFunc_)
        {
            initializerFunc_();
        }

        while (isRunning_)
        {
            ScopedThreadSleeper sleeper(interval_);
            loopFunc_();
        }

        if (finalizerFunc_)
        {
            finalizerFunc_();
        }

        // FIX #1: Uninitialize apartment
        if (doWinRt)
        {
            winrt::uninit_apartment();
        }
    });

    if (!name_.empty())
    {
        const auto hThread = static_cast<HANDLE>(thread_.native_handle());
        ::SetThreadDescription(hThread, name_.c_str());
    }
}

void ThreadLoop::Restart()
{
    Start(loopFunc_, interval_);
}

void ThreadLoop::Stop()
{
    if (!isRunning_) return;

    isRunning_ = false;

    if (thread_.joinable())
    {
        thread_.join();
    }
}

bool ThreadLoop::IsRunning() const
{
    return isRunning_;
}

bool ThreadLoop::HasFunction() const
{
    return loopFunc_ != nullptr;
}
