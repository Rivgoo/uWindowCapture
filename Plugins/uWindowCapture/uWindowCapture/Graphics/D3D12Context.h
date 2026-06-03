#pragma once
#include "IGraphicsContext.h"
#include <d3d12.h>
#include <d3d11on12.h>
#include <unordered_map>
#include <mutex>

class D3D12Context : public IGraphicsContext {
public:
    bool Initialize() override;
    void Finalize() override;
    
    void Lock() override { mutex_.lock(); }
    void Unlock() override { mutex_.unlock(); }
    
    ID3D11Device* GetDevice() override { return d3d11Device_.Get(); }
    ID3D11DeviceContext* GetContext() override { return d3d11Context_.Get(); }
    
    Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateTexture(int width, int height) override;
    void UpdateUnityTexture(void* unityTexturePtr, ID3D11Texture2D* source) override;
    void ReleaseUnityTexture(void* unityTexturePtr) override;

private:
    Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device_;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context_;
    Microsoft::WRL::ComPtr<ID3D11On12Device> d3d11On12Device_;
    
    std::unordered_map<void*, Microsoft::WRL::ComPtr<ID3D11Resource>> wrappedResources_;
    std::mutex mutex_;
};
