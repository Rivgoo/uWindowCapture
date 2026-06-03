#include <d3d11.h>
#include "../Unity/IUnityInterface.h"
#include "../Unity/IUnityGraphicsD3D11.h"
#include "UploadManager.h"
#include "WindowManager.h"
#include "../Core/Debug.h"
#include "../Core/Util.h"
#include "Window.h"
#include "../Core/Unity.h"
#include "Cursor.h"
#include <winrt/base.h>

#pragma comment(lib, "d3d11.lib")

using namespace Microsoft::WRL;
using DevicePtr  = Microsoft::WRL::ComPtr<ID3D11Device>;
using TexturePtr = Microsoft::WRL::ComPtr<ID3D11Texture2D>;

namespace
{
    constexpr auto kLoopMinTime = std::chrono::microseconds(100);
}

UploadManager::UploadManager()
{
    initThread_ = std::thread([this]
    {
        // FIX #1 / #5: Apartment init for shared-texture interop
        bool apartmentInitialised = false;
        try
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            apartmentInitialised = true;
        }
        catch (const winrt::hresult_error& e)
        {
            const HRESULT hr = e.code();
            if (hr != RPC_E_CHANGED_MODE && hr != S_FALSE)
            {
                Debug::Error(__FUNCTION__, " => winrt::init_apartment failed in initThread_.");
            }
            else
            {
                apartmentInitialised = true;
            }
        }

        // FIX #9: Safe device creation flag
        CreateDevice();

        if (device_)
        {
            StartUploadThread();
            isReady_ = true;
            readyCv_.notify_all();
        }
        else
        {
            Debug::Error(__FUNCTION__, " => D3D11 device creation failed.");
            isReady_ = false;
            readyCv_.notify_all();
        }

        if (apartmentInitialised)
        {
            winrt::uninit_apartment();
        }
    });
}

UploadManager::~UploadManager()
{
    if (initThread_.joinable())
    {
        initThread_.join();
    }
    StopUploadThread();
}

void UploadManager::CreateDevice()
{
    ComPtr<IDXGIDevice1> dxgiDevice;
    if (FAILED(GetUnityDevice()->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))) {
        Debug::Error(__FUNCTION__, " => QueryInterface from IUnityGraphicsD3D11 to IDXGIDevice1 failed.");
        return;
    }

    ComPtr<IDXGIAdapter> dxgiAdapter;
    if (FAILED(dxgiDevice->GetAdapter(&dxgiAdapter))) {
        Debug::Error(__FUNCTION__, " => GetAdapter from IDXGIDevice1 failed.");
        return;
    }

    const auto driverType = D3D_DRIVER_TYPE_UNKNOWN;
    const auto flags      = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    const D3D_FEATURE_LEVEL featureLevelsRequested[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };
    const UINT numLevelsRequested = sizeof(featureLevelsRequested) / sizeof(D3D_FEATURE_LEVEL);
    D3D_FEATURE_LEVEL featureLevelsSupported;

    const HRESULT hr = D3D11CreateDevice(
        dxgiAdapter.Get(),
        driverType,
        nullptr,
        flags,
        featureLevelsRequested,
        numLevelsRequested,
        D3D11_SDK_VERSION,
        &device_,
        &featureLevelsSupported,
        nullptr);

    if (FAILED(hr))
    {
        Debug::Error(__FUNCTION__, " => D3D11CreateDevice() failed with HRESULT: ", hr);
        device_ = nullptr;
    }
}

DevicePtr UploadManager::GetDevice()
{
    return device_;
}

TexturePtr UploadManager::CreateCompatibleSharedTexture(const TexturePtr& texture)
{
    if (!device_)
    {
        Debug::Error(__FUNCTION__, " => device_ is null.");
        return nullptr;
    }

    TexturePtr sharedTexture;
    D3D11_TEXTURE2D_DESC srcDesc;
    texture->GetDesc(&srcDesc);

    D3D11_TEXTURE2D_DESC desc = srcDesc;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    if (FAILED(device_->CreateTexture2D(&desc, nullptr, &sharedTexture)))
    {
        Debug::Error(__FUNCTION__, " => GetDevice()->CreateTexture2D() failed.");
        return nullptr;
    }

    return sharedTexture;
}

void UploadManager::StartUploadThread()
{
    threadLoop_.Start([this]
    {
        const int windowId = windowUploadQueue_.Dequeue();
        if (windowId >= 0)
        {
            if (auto window = WindowManager::Get().GetWindow(windowId))
            {
                window->Upload();
            }
        }

        const int iconId = iconUploadQueue_.Dequeue();
        if (iconId >= 0)
        {
            if (auto window = WindowManager::Get().GetWindow(iconId))
            {
                window->UploadIcon();
            }
        }

        if (auto& cursor = WindowManager::Get().GetCursor())
        {
            cursor->Upload();
        }
    }, kLoopMinTime);
}

void UploadManager::StopUploadThread()
{
    threadLoop_.Stop();
}

void UploadManager::RequestUploadWindow(int id)
{
    windowUploadQueue_.Enqueue(id);
}

void UploadManager::RequestUploadIcon(int id)
{
    iconUploadQueue_.Enqueue(id);
}
