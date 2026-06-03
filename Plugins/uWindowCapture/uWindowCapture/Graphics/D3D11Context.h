#pragma once
#include "IGraphicsContext.h"
#include <mutex>

class D3D11Context : public IGraphicsContext {
public:
    bool Initialize() override;
    void Finalize() override;
    
    void Lock() override { mutex_.lock(); }
    void Unlock() override { mutex_.unlock(); }
    
    ID3D11Device* GetDevice() override { return device_.Get(); }
    ID3D11DeviceContext* GetContext() override { return context_.Get(); }
    
    Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateTexture(int width, int height) override;
    void UpdateUnityTexture(void* unityTexturePtr, ID3D11Texture2D* source) override;
    void ReleaseUnityTexture(void* unityTexturePtr) override;

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    std::mutex mutex_;
};
