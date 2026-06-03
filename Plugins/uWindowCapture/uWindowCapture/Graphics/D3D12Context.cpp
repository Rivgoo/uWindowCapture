#include "D3D12Context.h"
#include "../Core/Unity.h"
#include "../Unity/IUnityGraphicsD3D12.h"
#include "../Core/Debug.h"

#pragma comment(lib, "d3d11.lib")

bool D3D12Context::Initialize() {
    auto d3d12 = GetUnity()->Get<IUnityGraphicsD3D12v5>();
    if (!d3d12) {
        Debug::Error("D3D12Context: IUnityGraphicsD3D12v5 interface not found.");
        return false;
    }
    
    d3d12Device_ = d3d12->GetDevice();
    IUnknown* queues[] = { d3d12->GetCommandQueue() };
    
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11On12CreateDevice(d3d12Device_.Get(), flags, nullptr, 0, queues, 1, 0, &d3d11Device_, &d3d11Context_, nullptr);
    if (FAILED(hr)) {
        Debug::Error("D3D12Context: D3D11On12CreateDevice failed with HRESULT: ", hr);
        return false;
    }
    
    hr = d3d11Device_.As(&d3d11On12Device_);
    if (FAILED(hr)) {
        Debug::Error("D3D12Context: Failed to cast to ID3D11On12Device.");
        return false;
    }
    
    return true;
}

void D3D12Context::Finalize() {
    std::lock_guard<std::mutex> lock(mutex_);
    wrappedResources_.clear();
    d3d11On12Device_.Reset();
    d3d11Context_.Reset();
    d3d11Device_.Reset();
    d3d12Device_.Reset();
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> D3D12Context::CreateTexture(int width, int height) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = d3d11Device_->CreateTexture2D(&desc, nullptr, &tex);
    if (FAILED(hr)) {
        Debug::Error("D3D12Context: Failed to create capture texture.");
        return nullptr;
    }
    return tex;
}

void D3D12Context::UpdateUnityTexture(void* unityTexturePtr, ID3D11Texture2D* source) {
    if (!unityTexturePtr || !source) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    ID3D11Resource* wrappedRes = nullptr;
    
    auto it = wrappedResources_.find(unityTexturePtr);
    if (it != wrappedResources_.end()) {
        wrappedRes = it->second.Get();
    } else {
        D3D11_RESOURCE_FLAGS flags{};
        flags.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        
        HRESULT hr = d3d11On12Device_->CreateWrappedResource(
            (IUnknown*)unityTexturePtr,
            &flags,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            IID_PPV_ARGS(&wrappedRes)
        );
        
        if (SUCCEEDED(hr)) {
            wrappedResources_[unityTexturePtr] = wrappedRes;
        } else {
            Debug::Error("D3D12Context: CreateWrappedResource failed with HRESULT: ", hr);
            return;
        }
    }

    d3d11On12Device_->AcquireWrappedResources(&wrappedRes, 1);
    d3d11Context_->CopyResource(wrappedRes, source);
    d3d11On12Device_->ReleaseWrappedResources(&wrappedRes, 1);
    d3d11Context_->Flush();
}

void D3D12Context::ReleaseUnityTexture(void* unityTexturePtr) {
    if (!unityTexturePtr) return;
    std::lock_guard<std::mutex> lock(mutex_);
    wrappedResources_.erase(unityTexturePtr);
}
