#pragma once
#include "IGraphicsContext.h"
#include "../Unity/IUnityGraphicsD3D12.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <unordered_map>
#include <mutex>

namespace uWindowCapture {

class SharedTextureResource;

class D3D12Context : public IGraphicsContext {
public:
    bool Initialize(IUnityInterfaces* unityInterfaces) override;
    void Finalize() override;
    
    void RenderEvent(int eventId) override;

    void RegisterSharedResource(void* unityTexturePtr, SharedTextureResource* sharedResource) override;
    void UnregisterSharedResource(void* unityTexturePtr) override;

private:
    bool CreateCommandObjects();

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    IUnityGraphicsD3D12v5* d3d12Unity_ = nullptr;
    
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    struct ResourceMap {
        SharedTextureResource* source;
        Microsoft::WRL::ComPtr<ID3D12Resource> openedTexture;
        Microsoft::WRL::ComPtr<ID3D12Fence> openedFence;
        uint64_t lastProcessedFenceValue = 0;
    };

    std::unordered_map<void*, ResourceMap> resources_;
    std::mutex mutex_;
};

} 
