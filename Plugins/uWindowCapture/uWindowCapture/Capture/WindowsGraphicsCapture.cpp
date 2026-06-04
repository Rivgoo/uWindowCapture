#include "WindowsGraphicsCapture.h"
#include "WindowManager.h"
#include "CaptureManager.h"
#include "IsolatedCaptureDevice.h"
#include "../Interop/SharedTextureResource.h"
#include "../Core/Debug.h"

#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Metadata.h>

#pragma comment(lib, "windowsapp.lib")

using namespace winrt;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

namespace
{
    bool CallWinRtApiWithExceptionCheck(const std::function<void()>& func, const std::string& name) noexcept
    {
        try {
            func();
        }
        catch (const winrt::hresult_error& e) {
            char buf[256];
            sprintf_s(buf, 256, "0x%x", static_cast<uint32_t>(e.code()));
            Debug::Error(name, " threw a WinRT exception: ", buf, " ", winrt::to_string(e.message()));
            return false;
        }
        catch (const std::exception& e) {
            Debug::Error(name, " threw an std exception: ", e.what());
            return false;
        }
        catch (...) {
            Debug::Error(name, " threw an unknown exception.");
            return false;
        }
        return true;
    }
}

bool WindowsGraphicsCapture::IsSupported()
{
    static std::once_flag s_flag;
    static bool s_available = false;
    std::call_once(s_flag, [&] {
        CallWinRtApiWithExceptionCheck([&] {
            s_available = winrt::Windows::Foundation::Metadata::ApiInformation::IsApiContractPresent(L"Windows.Foundation.UniversalApiContract", 8);
            }, "WindowsGraphicsCapture::IsSupported()");
        });
    return s_available;
}

bool WindowsGraphicsCapture::IsCursorCaptureEnabledApiSupported()
{
    if (!IsSupported()) return false;
    static std::once_flag s_flag;
    static bool s_enabled = false;
    std::call_once(s_flag, [&] {
        CallWinRtApiWithExceptionCheck([&] {
            s_enabled = winrt::Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent(L"Windows.Graphics.Capture.GraphicsCaptureSession", L"IsCursorCaptureEnabled");
            }, "WindowsGraphicsCapture::IsCursorCaptureEnabledApiSupported()");
        });
    return s_enabled;
}

WindowsGraphicsCapture::WindowsGraphicsCapture(HWND hWnd) : hWnd_(hWnd), hMonitor_(NULL)
{
    CreateItem();
}

WindowsGraphicsCapture::WindowsGraphicsCapture(HMONITOR hMonitor) : hMonitor_(hMonitor), hWnd_(NULL)
{
    CreateItem();
}

WindowsGraphicsCapture::~WindowsGraphicsCapture()
{
    Stop();
}

bool WindowsGraphicsCapture::CreateItem()
{
    if (!IsSupported()) return false;
    std::scoped_lock lock(itemMutex_);
    CallWinRtApiWithExceptionCheck([&] {
        auto factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>();
        auto interop = factory.as<IGraphicsCaptureItemInterop>();
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };
        if (hWnd_) {
            interop->CreateForWindow(hWnd_, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(item));
        }
        else {
            interop->CreateForMonitor(hMonitor_, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(), winrt::put_abi(item));
        }
        item_ = item;
        }, "WindowsGraphicsCapture::CreateItem()");

    if (!static_cast<bool>(item_)) return false;
    size_ = item_.get().Size();
    CallWinRtApiWithExceptionCheck([&] { displayName_ = std::wstring(item_.get().DisplayName()); }, "WindowsGraphicsCapture::CreateItem() - DisplayName");
    return true;
}

bool WindowsGraphicsCapture::CreatePoolAndSession()
{
    std::scoped_lock lock(itemMutex_);
    if (!static_cast<bool>(item_)) return false;

    bool ret = CallWinRtApiWithExceptionCheck([&] { size_ = item_.get().Size(); }, "WindowsGraphicsCapture::CreatePoolAndSession() - Size");
    if (!ret || size_.Width == 0 || size_.Height == 0) return false;

    const auto& wgcManager = WindowManager::GetWindowsGraphicsCaptureManager();
    if (!wgcManager) return false;
    auto device = wgcManager->GetDevice();
    if (!device) return false;
    winrtDevice_ = device;

    RecreateSharedResource(size_.Width, size_.Height);

    return CallWinRtApiWithExceptionCheck([&] {
        std::scoped_lock resLock(resourceMutex_);
        pool_ = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
            winrtDevice_,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            2,
            size_);

        frameArrivedToken_ = pool_.FrameArrived(winrt::Windows::Foundation::TypedEventHandler<winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool, winrt::Windows::Foundation::IInspectable>(this, &WindowsGraphicsCapture::OnFrameArrived));

        session_ = pool_.CreateCaptureSession(item_.get());
        session_.StartCapture();
        }, "WindowsGraphicsCapture::CreatePoolAndSession() - Capture");
}

void WindowsGraphicsCapture::DestroyPoolAndSession()
{
    std::scoped_lock lock(resourceMutex_);
    if (pool_) {
        CallWinRtApiWithExceptionCheck([&] { pool_.FrameArrived(frameArrivedToken_); pool_.Close(); }, "DestroyPoolAndSession() - Pool");
        pool_ = nullptr;
    }
    if (session_) {
        CallWinRtApiWithExceptionCheck([&] { session_.Close(); }, "DestroyPoolAndSession() - Session");
        session_ = nullptr;
    }
    sharedResource_.reset();
}

void WindowsGraphicsCapture::RecreateSharedResource(int width, int height)
{
    std::lock_guard<std::mutex> lock(resourceMutex_);
    auto dev = uWindowCapture::CaptureManager::Get().GetCaptureDevice();
    if (!dev) return;
    sharedResource_ = std::make_shared<uWindowCapture::SharedTextureResource>();
    sharedResource_->Initialize(dev->GetDevice(), width, height);
}

std::shared_ptr<uWindowCapture::SharedTextureResource> WindowsGraphicsCapture::GetSharedResource() const
{
    std::lock_guard<std::mutex> lock(resourceMutex_);
    return sharedResource_;
}

void WindowsGraphicsCapture::OnFrameArrived(winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender, winrt::Windows::Foundation::IInspectable const&)
{
    auto frame = sender.TryGetNextFrame();
    if (!frame) return;

    auto size = frame.ContentSize();
    std::shared_ptr<uWindowCapture::SharedTextureResource> res;
    {
        std::lock_guard<std::mutex> lock(resourceMutex_);
        if (!sharedResource_ || sharedResource_->GetWidth() != size.Width || sharedResource_->GetHeight() != size.Height) {
            auto dev = uWindowCapture::CaptureManager::Get().GetCaptureDevice();
            if (dev) {
                sharedResource_ = std::make_shared<uWindowCapture::SharedTextureResource>();
                sharedResource_->Initialize(dev->GetDevice(), size.Width, size.Height);
            }
            sender.Recreate(winrtDevice_, winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);
        }
        res = sharedResource_;
    }

    if (!res) return;

    auto surface = frame.Surface();
    auto access = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<ID3D11Texture2D> d3d11Texture;
    access->GetInterface(winrt::guid_of<ID3D11Texture2D>(), d3d11Texture.put_void());

    if (d3d11Texture) {
        auto dev = uWindowCapture::CaptureManager::Get().GetCaptureDevice();
        if (dev) {
            auto context = dev->GetContext();
            context->CopyResource(res->GetTexture(), d3d11Texture.get());
            context->Flush();
            res->SignalFence(context);
        }
    }
    stopTimer_ = 0.f;
}

void WindowsGraphicsCapture::RequestStart()
{
    isStartRequested_ = true;
}

void WindowsGraphicsCapture::Start()
{
    isStartRequested_ = false;
    stopTimer_ = 0.f;
    if (isStarted_) return;
    if (CreatePoolAndSession()) {
        isStarted_ = true;
        restartTimer_ = 0.f;
    }
}

void WindowsGraphicsCapture::Stop()
{
    stopTimer_ = 0.f;
    if (!isStarted_) return;
    isStarted_ = false;
    DestroyPoolAndSession();
}

bool WindowsGraphicsCapture::ShouldStop() const
{
    return stopTimer_ > 1.f;
}

bool WindowsGraphicsCapture::ShouldRestart() const
{
    return isRestartRequested_;
}

void WindowsGraphicsCapture::Restart()
{
    restartTimer_ = 0.f;
    isRestartRequested_ = false;
    Stop();
    if (!CreateItem()) return;
    Start();
}

bool WindowsGraphicsCapture::IsAvailable() const
{
    std::scoped_lock lock(itemMutex_);
    return static_cast<bool>(item_);
}

void WindowsGraphicsCapture::Update(float dt)
{
    stopTimer_ = stopTimer_ + dt;
    restartTimer_ = restartTimer_ + dt;
}

void WindowsGraphicsCapture::EnableCursorCapture(bool enabled)
{
    if (isCursorCaptureEnabled_ == enabled) return;
    isCursorCaptureEnabled_ = enabled;
    if (!IsCursorCaptureEnabledApiSupported()) return;
    std::scoped_lock lock(resourceMutex_);
    if (session_) session_.IsCursorCaptureEnabled(enabled);
}

const wchar_t* WindowsGraphicsCapture::GetDisplayName() const
{
    std::scoped_lock lock(itemMutex_);
    return displayName_.c_str();
}

// Manager Implementation
winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice WindowsGraphicsCaptureManager::GetDevice()
{
    std::unique_lock<std::mutex> initLock(deviceInitMutex_);
    if (deviceWinRt_) return deviceWinRt_;

    if (uWindowCapture::CaptureManager::IsNull()) return deviceWinRt_;

    auto device = uWindowCapture::CaptureManager::Get().GetCaptureDevice();
    if (device)
    {
        winrt::com_ptr<IDXGIDevice> dxgiDevice;
        if (SUCCEEDED(device->GetDevice()->QueryInterface(IID_PPV_ARGS(&dxgiDevice))))
        {
            winrt::com_ptr<::IInspectable> inspectable;
            ::CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put());
            deviceWinRt_ = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
        }
    }
    return deviceWinRt_;
}

std::shared_ptr<WindowsGraphicsCapture> WindowsGraphicsCaptureManager::Create(HWND hWnd)
{
    auto instance = std::make_shared<WindowsGraphicsCapture>(hWnd);
    if (!instance->IsAvailable()) return nullptr;
    std::scoped_lock lock(allInstancesMutex_);
    allInstances_.push_back(instance);
    return instance;
}

std::shared_ptr<WindowsGraphicsCapture> WindowsGraphicsCaptureManager::Create(HMONITOR hMonitor)
{
    auto instance = std::make_shared<WindowsGraphicsCapture>(hMonitor);
    if (!instance->IsAvailable()) return nullptr;
    std::scoped_lock lock(allInstancesMutex_);
    allInstances_.push_back(instance);
    return instance;
}

void WindowsGraphicsCaptureManager::Destroy(const std::shared_ptr<WindowsGraphicsCapture>& instance)
{
    std::scoped_lock lock(allInstancesMutex_);
    allInstances_.remove(instance);
}

void WindowsGraphicsCaptureManager::UpdateFromMainThread(float dt)
{
    std::scoped_lock lock(activeInstancesMutex_);
    for (const auto& instance : activeInstances_) {
        instance->Update(dt);
    }
}

void WindowsGraphicsCaptureManager::UpdateFromCaptureThread()
{
    StartInstances();
    RestartInstances();
    StopInstances();
}

void WindowsGraphicsCaptureManager::StartInstances()
{
    std::scoped_lock lock(allInstancesMutex_);
    for (const auto& instance : allInstances_) {
        if (instance->isStartRequested_) {
            instance->Start();
            if (instance->IsStarted()) {
                std::scoped_lock alock(activeInstancesMutex_);
                activeInstances_.push_back(instance);
            }
        }
    }
}

void WindowsGraphicsCaptureManager::RestartInstances()
{
    std::scoped_lock lock(activeInstancesMutex_);
    for (const auto& instance : activeInstances_) {
        if (instance->ShouldRestart()) {
            instance->Restart();
        }
    }
}

void WindowsGraphicsCaptureManager::StopInstances()
{
    std::scoped_lock lock(activeInstancesMutex_);
    for (auto it = activeInstances_.begin(); it != activeInstances_.end();) {
        if ((*it)->ShouldStop()) {
            (*it)->Stop();
            it = activeInstances_.erase(it);
        }
        else {
            ++it;
        }
    }
}

void WindowsGraphicsCaptureManager::StopAllInstances()
{
    std::scoped_lock lock(activeInstancesMutex_);
    for (const auto& instance : activeInstances_) {
        instance->Stop();
    }
    activeInstances_.clear();
}