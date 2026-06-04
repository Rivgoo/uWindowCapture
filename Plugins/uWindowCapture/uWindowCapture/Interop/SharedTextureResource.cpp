#include "SharedTextureResource.h"
#include "../Core/Debug.h"

namespace uWindowCapture {

SharedTextureResource::SharedTextureResource() = default;

SharedTextureResource::~SharedTextureResource() {
    Finalize();
}

bool SharedTextureResource::Initialize(ID3D11Device5* captureDevice, int width, int height) {
    if (!captureDevice || width <= 0 || height <= 0) return false;

    width_ = width;
    height_ = height;

    // 1. Create Texture with NT Handle Export flag
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

    HRESULT hr = captureDevice->CreateTexture2D(&desc, nullptr, &texture_);
    if (FAILED(hr)) {
        Debug::Error("SharedTextureResource: CreateTexture2D failed. HRESULT: ", hr);
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIResource1> dxgiResource;
    hr = texture_.As(&dxgiResource);
    if (FAILED(hr)) return false;

    hr = dxgiResource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &sharedTextureHandle_);
    if (FAILED(hr)) {
        Debug::Error("SharedTextureResource: CreateSharedHandle for Texture failed. HRESULT: ", hr);
        return false;
    }

    // 2. Create Shared Fence
    hr = captureDevice->CreateFence(fenceValue_, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&fence_));
    if (FAILED(hr)) {
        Debug::Error("SharedTextureResource: CreateFence failed. HRESULT: ", hr);
        return false;
    }

    hr = fence_->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &sharedFenceHandle_);
    if (FAILED(hr)) {
        Debug::Error("SharedTextureResource: CreateSharedHandle for Fence failed. HRESULT: ", hr);
        return false;
    }

    return true;
}

void SharedTextureResource::SignalFence(ID3D11DeviceContext4* captureContext) {
    if (!captureContext || !fence_) return;
    fenceValue_++;
    captureContext->Signal(fence_.Get(), fenceValue_);
}

void SharedTextureResource::Finalize() {
    if (sharedTextureHandle_) {
        CloseHandle(sharedTextureHandle_);
        sharedTextureHandle_ = nullptr;
    }
    if (sharedFenceHandle_) {
        CloseHandle(sharedFenceHandle_);
        sharedFenceHandle_ = nullptr;
    }
    fence_.Reset();
    texture_.Reset();
}

} // namespace uWindowCapture
