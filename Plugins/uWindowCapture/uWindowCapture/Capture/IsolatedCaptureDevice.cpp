#include "IsolatedCaptureDevice.h"
#include "../Core/Debug.h"

#pragma comment(lib, "d3d11.lib")

namespace uWindowCapture {

IsolatedCaptureDevice::IsolatedCaptureDevice() = default;
IsolatedCaptureDevice::~IsolatedCaptureDevice() { Finalize(); }

bool IsolatedCaptureDevice::Initialize() {
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    // creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1, // Required for NT Handles and Fences
        D3D_FEATURE_LEVEL_11_0
    };

    Microsoft::WRL::ComPtr<ID3D11Device> baseDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> baseContext;

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        creationFlags, featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION, &baseDevice, nullptr, &baseContext);

    if (FAILED(hr)) {
        Debug::Error("IsolatedCaptureDevice: D3D11CreateDevice failed. HRESULT: ", hr);
        return false;
    }

    hr = baseDevice.As(&device_);
    if (FAILED(hr)) {
        Debug::Error("IsolatedCaptureDevice: Failed to query ID3D11Device5 (Windows 10 Creators Update required).");
        return false;
    }

    hr = baseContext.As(&context_);
    if (FAILED(hr)) {
        Debug::Error("IsolatedCaptureDevice: Failed to query ID3D11DeviceContext4.");
        return false;
    }

    Debug::Log("IsolatedCaptureDevice: Successfully initialized background D3D11 device.");
    return true;
}

void IsolatedCaptureDevice::Finalize() {
    context_.Reset();
    device_.Reset();
}

} 
