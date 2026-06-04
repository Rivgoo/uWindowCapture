#include "D3D11Context.h"
#include "../Unity/IUnityGraphicsD3D11.h"
#include "../Interop/SharedTextureResource.h"
#include "../Core/Debug.h"

namespace uWindowCapture {

bool D3D11Context::Initialize(IUnityInterfaces* unityInterfaces) {
    auto d3d11 = unityInterfaces->Get<IUnityGraphicsD3D11>();
    if (!d3d11) return false;

    Microsoft::WRL::ComPtr<ID3D11Device> baseDevice = d3d11->GetDevice();
    if (!baseDevice) return false;

    if (FAILED(baseDevice.As(&device_))) {
        Debug::Error("D3D11Context: Unity device does not support ID3D11Device5.");
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> baseContext;
    baseDevice->GetImmediateContext(&baseContext);
    if (FAILED(baseContext.As(&context_))) return false;

    return true;
}

void D3D11Context::Finalize() {
    std::lock_guard<std::mutex> lock(mutex_);
    resources_.clear();
    context_.Reset();
    device_.Reset();
}

void D3D11Context::RegisterSharedResource(void* unityTexturePtr, SharedTextureResource* sharedResource) {
    if (!unityTexturePtr || !sharedResource) return;

    std::lock_guard<std::mutex> lock(mutex_);
    ResourceMap map{};
    map.source = sharedResource;

    Microsoft::WRL::ComPtr<ID3D11Device1> dev1;
    device_.As(&dev1);

    dev1->OpenSharedResource1(sharedResource->GetSharedTextureHandle(), IID_PPV_ARGS(&map.openedTexture));
    dev1->OpenSharedResource1(sharedResource->GetSharedFenceHandle(), IID_PPV_ARGS(&map.openedFence));

    resources_[unityTexturePtr] = map;
}

void D3D11Context::UnregisterSharedResource(void* unityTexturePtr) {
    std::lock_guard<std::mutex> lock(mutex_);
    resources_.erase(unityTexturePtr);
}

void D3D11Context::RenderEvent(int eventId) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : resources_) {
        auto unityTex = static_cast<ID3D11Texture2D*>(pair.first);
        auto& map = pair.second;

        if (map.openedTexture && map.openedFence) {
            uint64_t targetFence = map.source->GetCurrentFenceValue();
            if (targetFence > map.lastProcessedFenceValue) {
                // Wait for Producer
                context_->Wait(map.openedFence.Get(), targetFence);
                // Copy
                context_->CopyResource(unityTex, map.openedTexture.Get());
                map.lastProcessedFenceValue = targetFence;
            }
        }
    }
}

} 
