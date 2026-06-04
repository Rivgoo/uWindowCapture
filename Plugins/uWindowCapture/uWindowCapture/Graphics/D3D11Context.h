#pragma once
#include "IGraphicsContext.h"
#include <d3d11_4.h>
#include <wrl/client.h>
#include <unordered_map>
#include <mutex>

namespace uWindowCapture {

class D3D11Context : public IGraphicsContext {
public:
    bool Initialize(IUnityInterfaces* unityInterfaces) override;
    void Finalize() override;
    
    void RenderEvent(int eventId) override;

    void RegisterSharedResource(void* unityTexturePtr, SharedTextureResource* sharedResource) override;
    void UnregisterSharedResource(void* unityTexturePtr) override;

private:
    Microsoft::WRL::ComPtr<ID3D11Device5> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context_;
    
    struct ResourceMap {
        SharedTextureResource* source;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> openedTexture;
        Microsoft::WRL::ComPtr<ID3D11Fence> openedFence;
        uint64_t lastProcessedFenceValue = 0;
    };

    std::unordered_map<void*, ResourceMap> resources_;
    std::mutex mutex_;
};

} 
