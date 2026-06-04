#pragma once
#include <d3d11_4.h>
#include <wrl/client.h>
#include <cstdint>

namespace uWindowCapture {

class SharedTextureResource {
public:
    SharedTextureResource();
    ~SharedTextureResource();

    bool Initialize(ID3D11Device5* captureDevice, int width, int height);
    void Finalize();

    HANDLE GetSharedTextureHandle() const { return sharedTextureHandle_; }
    HANDLE GetSharedFenceHandle() const { return sharedFenceHandle_; }
    uint64_t GetCurrentFenceValue() const { return fenceValue_; }

    // Called by Producer (D3D11 Capture Thread)
    void SignalFence(ID3D11DeviceContext4* captureContext);

    ID3D11Texture2D* GetTexture() const { return texture_.Get(); }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    HANDLE sharedTextureHandle_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Fence> fence_;
    HANDLE sharedFenceHandle_ = nullptr;

    uint64_t fenceValue_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace uWindowCapture
