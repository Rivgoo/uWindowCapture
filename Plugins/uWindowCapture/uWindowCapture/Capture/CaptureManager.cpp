#include "CaptureManager.h"
#include "IsolatedCaptureDevice.h"
#include "WindowManager.h"
#include "Window.h"
#include "../Core/Debug.h"

namespace {
    constexpr auto kLoopMinTime = std::chrono::microseconds(100);
}

namespace uWindowCapture {

UWC_SINGLETON_INSTANCE(CaptureManager)

void CaptureManager::Initialize() {
    if (!captureDevice_) {
        captureDevice_ = std::make_unique<IsolatedCaptureDevice>();
        captureDevice_->Initialize();
    }

    windowCaptureThreadLoop_.SetWinRtApartment(true);
    iconCaptureThreadLoop_.SetWinRtApartment(true);
    uploadThreadLoop_.SetWinRtApartment(true);

    windowCaptureThreadLoop_.SetFinalizer([] {
        if (const auto& wgcManager = WindowManager::GetWindowsGraphicsCaptureManager()) {
            wgcManager->StopAllInstances();
        }
    });

    windowCaptureThreadLoop_.Start([this] {
        int id = highPriorityQueue_.Dequeue();
        if (id >= 0 && !middlePriorityQueue_.Empty()) {
            const auto midId = middlePriorityQueue_.Dequeue();
            highPriorityQueue_.Enqueue(midId);
        }
        if (id < 0) id = middlePriorityQueue_.Dequeue();
        if (id < 0) id = lowPriorityQueue_.Dequeue();

        if (id >= 0) {
            if (auto window = WindowManager::Get().GetWindow(id)) {
                window->Capture();
            }
        }

        if (const auto& wgcManager = WindowManager::GetWindowsGraphicsCaptureManager()) {
            wgcManager->UpdateFromCaptureThread();
        }
    }, kLoopMinTime);

    iconCaptureThreadLoop_.Start([this] {
        int id = iconQueue_.Dequeue();
        if (id >= 0) {
            if (auto window = WindowManager::Get().GetWindow(id)) {
                window->CaptureIcon();
            }
        }
    }, kLoopMinTime);

    uploadThreadLoop_.Start([this] {
        const int windowId = windowUploadQueue_.Dequeue();
        if (windowId >= 0) {
            if (auto window = WindowManager::Get().GetWindow(windowId)) {
                window->Upload();
            }
        }

        const int iconId = iconUploadQueue_.Dequeue();
        if (iconId >= 0) {
            if (auto window = WindowManager::Get().GetWindow(iconId)) {
                window->UploadIcon();
            }
        }

        if (auto& cursor = WindowManager::Get().GetCursor()) {
            cursor->Upload();
        }
    }, kLoopMinTime);
}

void CaptureManager::Finalize() {
    windowCaptureThreadLoop_.Stop();
    iconCaptureThreadLoop_.Stop();
    uploadThreadLoop_.Stop();

    if (captureDevice_) {
        captureDevice_->Finalize();
        captureDevice_.reset();
    }
}

void CaptureManager::RequestCapture(int id, CapturePriority priority) {
    switch (priority) {
        case CapturePriority::High: highPriorityQueue_.Enqueue(id); break;
        case CapturePriority::Middle: middlePriorityQueue_.Enqueue(id); break;
        case CapturePriority::Low: lowPriorityQueue_.Enqueue(id); break;
    }
}

void CaptureManager::RequestCaptureIcon(int id) {
    iconQueue_.Enqueue(id);
}

void CaptureManager::RequestUploadWindow(int id) {
    windowUploadQueue_.Enqueue(id);
}

void CaptureManager::RequestUploadIcon(int id) {
    iconUploadQueue_.Enqueue(id);
}

} 
