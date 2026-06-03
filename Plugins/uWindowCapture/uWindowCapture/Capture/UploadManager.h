#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <d3d11.h>
#include <wrl/client.h>

#include "../Core/WindowQueue.h"
#include "../Core/Thread.h"

class Window;

class UploadManager
{
public:
    using DevicePtr  = Microsoft::WRL::ComPtr<ID3D11Device>;
    using TexturePtr = Microsoft::WRL::ComPtr<ID3D11Texture2D>;

    UploadManager();
    ~UploadManager();

    bool IsReady() const { return isReady_; }

    // FIX #9: Condition variable for safe waiting
    std::condition_variable& GetReadyCv()    { return readyCv_; }
    std::mutex&              GetReadyMutex() { return readyMutex_; }

    DevicePtr  GetDevice();
    TexturePtr CreateCompatibleSharedTexture(const TexturePtr& texture);
    void RequestUploadWindow(int id);
    void RequestUploadIcon(int id);
    void StartUploadThread();
    void StopUploadThread();

private:
    void CreateDevice();

    std::atomic<bool>       isReady_ = false;
    std::mutex              readyMutex_;
    std::condition_variable readyCv_;

    DevicePtr               device_;
    std::thread             initThread_;
    ThreadLoop              threadLoop_ = { L"uWindowCapture - Upload Thread" };
    WindowQueue             windowUploadQueue_;
    WindowQueue             iconUploadQueue_;
};
