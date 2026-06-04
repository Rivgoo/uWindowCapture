#pragma once

#include <Windows.h>
#include <memory>
#include "../Core/Singleton.h"
#include "../Core/WindowQueue.h"
#include "../Core/Thread.h"

#include "IsolatedCaptureDevice.h"

enum class CapturePriority
{
    High = 0,
    Middle = 1,
    Low = 2,
};

namespace uWindowCapture {

    class CaptureManager {
        UWC_SINGLETON(CaptureManager)
    public:
        void Initialize();
        void Finalize();

        IsolatedCaptureDevice* GetCaptureDevice() const { return captureDevice_.get(); }

        void RequestCapture(int id, CapturePriority priority);
        void RequestCaptureIcon(int id);
        void RequestUploadWindow(int id);
        void RequestUploadIcon(int id);

    private:
        std::unique_ptr<IsolatedCaptureDevice> captureDevice_;

        ThreadLoop windowCaptureThreadLoop_ = { L"uWindowCapture - Window Capture Thread" };
        ThreadLoop iconCaptureThreadLoop_ = { L"uWindowCapture - Icon Capture Thread" };
        ThreadLoop uploadThreadLoop_ = { L"uWindowCapture - Upload Thread" };

        WindowQueue highPriorityQueue_;
        WindowQueue middlePriorityQueue_;
        WindowQueue lowPriorityQueue_;
        WindowQueue iconQueue_;

        WindowQueue windowUploadQueue_;
        WindowQueue iconUploadQueue_;
    };

}