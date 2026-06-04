#include "D3D12Context.h"
#include "../Interop/SharedTextureResource.h"
#include "../Core/Debug.h"

namespace uWindowCapture {

bool D3D12Context::Initialize(IUnityInterfaces* unityInterfaces) {
    d3d12Unity_ = unityInterfaces->Get<IUnityGraphicsD3D12v5>();
    if (!d3d12Unity_) return false;

    device_ = d3d12Unity_->GetDevice();
    if (!device_) return false;

    if (!CreateCommandObjects()) return false;

    return true;
}

bool D3D12Context::CreateCommandObjects() {
    HRESULT hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
    if (FAILED(hr)) return false;

    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
    if (FAILED(hr)) return false;

    commandList_->Close(); // Close initially
    return true;
}

void D3D12Context::Finalize() {
    std::lock_guard<std::mutex> lock(mutex_);
    resources_.clear();
    commandList_.Reset();
    commandAllocator_.Reset();
    device_.Reset();
}

void D3D12Context::RegisterSharedResource(void* unityTexturePtr, SharedTextureResource* sharedResource) {
    if (!unityTexturePtr || !sharedResource) return;

    std::lock_guard<std::mutex> lock(mutex_);
    ResourceMap map{};
    map.source = sharedResource;

    device_->OpenSharedHandle(sharedResource->GetSharedTextureHandle(), IID_PPV_ARGS(&map.openedTexture));
    device_->OpenSharedHandle(sharedResource->GetSharedFenceHandle(), IID_PPV_ARGS(&map.openedFence));

    resources_[unityTexturePtr] = map;
}

void D3D12Context::UnregisterSharedResource(void* unityTexturePtr) {
    std::lock_guard<std::mutex> lock(mutex_);
    resources_.erase(unityTexturePtr);
}

void D3D12Context::RenderEvent(int eventId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!d3d12Unity_ || resources_.empty()) return;

    ID3D12CommandQueue* queue = d3d12Unity_->GetCommandQueue();
    if (!queue) return;

    bool needsExecute = false;
    commandAllocator_->Reset();
    commandList_->Reset(commandAllocator_.Get(), nullptr);

    for (auto& pair : resources_) {
        auto unityRes = static_cast<ID3D12Resource*>(pair.first);
        auto& map = pair.second;

        if (map.openedTexture && map.openedFence) {
            uint64_t targetFence = map.source->GetCurrentFenceValue();
            if (targetFence > map.lastProcessedFenceValue) {
                // 1. Wait for Producer (D3D11)
                queue->Wait(map.openedFence.Get(), targetFence);

                // 2. Set Barriers
                D3D12_RESOURCE_BARRIER barriers[2] = {};
                barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barriers[0].Transition.pResource = unityRes;
                barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

                barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barriers[1].Transition.pResource = map.openedTexture.Get();
                barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
                barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

                commandList_->ResourceBarrier(2, barriers);

                // 3. Copy
                commandList_->CopyResource(unityRes, map.openedTexture.Get());

                // 4. Revert Barriers
                barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

                barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
                barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

                commandList_->ResourceBarrier(2, barriers);

                map.lastProcessedFenceValue = targetFence;
                needsExecute = true;
            }
        }
    }

    commandList_->Close();

    if (needsExecute) {
        ID3D12CommandList* ppCommandLists[] = { commandList_.Get() };
        queue->ExecuteCommandLists(1, ppCommandLists);
    }
}

} 
