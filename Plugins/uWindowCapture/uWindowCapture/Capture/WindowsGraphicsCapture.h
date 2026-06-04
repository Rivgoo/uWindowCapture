#pragma once

#include <functional>
#include <chrono>
#include <mutex>
#include <set>
#include <list>
#include <string>
#include <atomic>
#include <memory>
#include <dxgi.h>
#include <d3d11.h>
#include <winrt/base.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.Capture.h>

namespace uWindowCapture {
    class SharedTextureResource;
    class IsolatedCaptureDevice;
}

class WindowsGraphicsCapture : public std::enable_shared_from_this<WindowsGraphicsCapture>
{
    friend class WindowsGraphicsCaptureManager;
public:
    static bool IsSupported();
    static bool IsCursorCaptureEnabledApiSupported();

    explicit WindowsGraphicsCapture(HWND hWnd);
    explicit WindowsGraphicsCapture(HMONITOR hMonitor);
    ~WindowsGraphicsCapture();

    void Update(float dt);
    int  GetHeight() const { return size_.Height; }
    int  GetWidth()  const { return size_.Width; }
    void RequestStart();
    bool IsAvailable()  const;
    bool IsStarted()    const { return isStarted_; }
    void EnableCursorCapture(bool enabled);

    std::shared_ptr<uWindowCapture::SharedTextureResource> GetSharedResource() const;
    const wchar_t* GetDisplayName() const;

private:
    bool ShouldStop()    const;
    bool ShouldRestart() const;
    void Restart();
    void Start();
    void Stop();
    bool CreateItem();
    bool CreatePoolAndSession();
    void DestroyPoolAndSession();
    void RecreateSharedResource(int width, int height);
    void OnFrameArrived(winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const& args);

    const HWND    hWnd_;
    const HMONITOR hMonitor_;

    winrt::agile_ref<winrt::Windows::Graphics::Capture::GraphicsCaptureItem> item_{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool   pool_{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession       session_{ nullptr };
    winrt::event_token frameArrivedToken_;
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice winrtDevice_{ nullptr };

    winrt::Windows::Graphics::SizeInt32 size_ = { 0, 0 };
    mutable std::wstring displayName_;

    std::shared_ptr<uWindowCapture::SharedTextureResource> sharedResource_;
    mutable std::mutex resourceMutex_;
    mutable std::mutex itemMutex_;

    std::atomic<bool>  isStarted_ = false;
    std::atomic<bool>  isCursorCaptureEnabled_ = { true };
    std::atomic<bool>  isStartRequested_ = { false };
    std::atomic<float> stopTimer_ = { 0.f };
    std::atomic<bool>  isRestartRequested_ = { false };
    std::atomic<float> restartTimer_ = { 0.f };
};

class WindowsGraphicsCaptureManager final
{
public:
    std::shared_ptr<WindowsGraphicsCapture> Create(HWND hWnd);
    std::shared_ptr<WindowsGraphicsCapture> Create(HMONITOR hMonitor);
    void Destroy(const std::shared_ptr<WindowsGraphicsCapture>& instance);

    void UpdateFromMainThread(float dt);
    void UpdateFromCaptureThread();
    void StopAllInstances();

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice GetDevice();

private:
    void StartInstances();
    void RestartInstances();
    void StopInstances();

    using Ptr = std::shared_ptr<WindowsGraphicsCapture>;
    std::list<Ptr> allInstances_;
    std::list<Ptr> activeInstances_;
    std::mutex     allInstancesMutex_;
    std::mutex     activeInstancesMutex_;

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice deviceWinRt_{ nullptr };
    std::mutex deviceInitMutex_;
};