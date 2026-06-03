#pragma once
#include <d3d11.h>
#include <wrl/client.h>

class IGraphicsContext {
public:
    virtual ~IGraphicsContext() = default;
    
    virtual bool Initialize() = 0;
    virtual void Finalize() = 0;
    
    virtual void Lock() = 0;
    virtual void Unlock() = 0;
    
    virtual ID3D11Device* GetDevice() = 0;
    virtual ID3D11DeviceContext* GetContext() = 0;
    
    virtual Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateTexture(int width, int height) = 0;
    virtual void UpdateUnityTexture(void* unityTexturePtr, ID3D11Texture2D* source) = 0;
    virtual void ReleaseUnityTexture(void* unityTexturePtr) = 0;
};
