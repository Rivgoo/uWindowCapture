#include "D3D11Context.h"
#include "../Core/Unity.h"
#include "../Unity/IUnityGraphicsD3D11.h"
#include "../Core/Debug.h"

bool D3D11Context::Initialize() {
    auto d3d11 = GetUnity()->Get<IUnityGraphicsD3D11>();
    if (!d3d11) {
        Debug::Error("D3D11Context: IUnityGraphicsD3D11 interface not found.");
        return false;
    }
    device_ = d3d11->GetDevice();
    if (!device_) {
        Debug::Error("D3D11Context: Failed to get D3D11 Device from Unity.");
        return false;
    }
    device_->GetImmediateContext(&context_);
    return true;
}

void D3D11Context::Finalize() {
    std::lock_guard<std::mutex> lock(mutex_);
    context_.Reset();
    device_.Reset();
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> D3D11Context::CreateTexture(int width, int height) {
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
    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &tex);
    if (FAILED(hr)) {
        Debug::Error("D3D11Context: CreateTexture2D failed.");
        return nullptr;
    }
    return tex;
}

void D3D11Context::UpdateUnityTexture(void* unityTexturePtr, ID3D11Texture2D* source) {
    if (!unityTexturePtr || !source) return;
    std::lock_guard<std::mutex> lock(mutex_);
    context_->CopyResource((ID3D11Resource*)unityTexturePtr, source);
}

void D3D11Context::ReleaseUnityTexture(void* unityTexturePtr) {
    // No resources require explicitly wrapping or releasing in pure D3D11
}
