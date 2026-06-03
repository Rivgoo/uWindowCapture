#include <algorithm>
#include "CaptureManager.h"
#include "WindowManager.h"
#include "Window.h"
#include "../Core/Debug.h"
#include "../Core/Util.h"

using namespace Microsoft::WRL;

namespace
{
    constexpr auto kLoopMinTime = std::chrono::microseconds(100);
}

CaptureManager::CaptureManager()
{
    // FIX #1 / #4: Initialize MTA apartment for WGC threads
    windowCaptureThreadLoop_.SetWinRtApartment(true);
    iconCaptureThreadLoop_.SetWinRtApartment(true);

    windowCaptureThreadLoop_.SetFinalizer([]
    {
        if (const auto& wgcManager = WindowManager::GetWindowsGraphicsCaptureManager())
        {
            wgcManager->StopAllInstances();
        }
    });

    windowCaptureThreadLoop_.Start([this]
    {
        int id = highPriorityQueue_.Dequeue();

        if (id >= 0 && !middlePriorityQueue_.Empty())
        {
            const auto midId = middlePriorityQueue_.Dequeue();
            highPriorityQueue_.Enqueue(midId);
        }

        if (id < 0)
        {
            id = middlePriorityQueue_.Dequeue();
        }

        if (id < 0)
        {
            id = lowPriorityQueue_.Dequeue();
        }

        if (id >= 0)
        {
            if (auto window = WindowManager::Get().GetWindow(id))
            {
                window->Capture();
            }
        }

        if (const auto& wgcManager = WindowManager::GetWindowsGraphicsCaptureManager())
        {
            wgcManager->UpdateFromCaptureThread();
        }
    }, kLoopMinTime);

    iconCaptureThreadLoop_.Start([this]
    {
        int id = iconQueue_.Dequeue();
        if (id >= 0)
        {
            if (auto window = WindowManager::Get().GetWindow(id))
            {
                window->CaptureIcon();
            }
        }
    }, kLoopMinTime);
}

CaptureManager::~CaptureManager()
{
    iconCaptureThreadLoop_.Stop();
    windowCaptureThreadLoop_.Stop();
}

void CaptureManager::RequestCapture(int id, CapturePriority priority)
{
    switch (priority)
    {
        case CapturePriority::High:
        {
            highPriorityQueue_.Enqueue(id);
            break;
        }
        case CapturePriority::Middle:
        {
            middlePriorityQueue_.Enqueue(id);
            break;
        }
        case CapturePriority::Low:
        {
            lowPriorityQueue_.Enqueue(id);
            break;
        }
    }
}

void CaptureManager::RequestCaptureIcon(int id)
{
    iconQueue_.Enqueue(id);
}
