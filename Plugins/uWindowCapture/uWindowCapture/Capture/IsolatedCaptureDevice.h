#pragma once
#include <d3d11_4.h>
#include <wrl/client.h>

namespace uWindowCapture {

class IsolatedCaptureDevice {
public:
    IsolatedCaptureDevice();
    ~IsolatedCaptureDevice();

    bool Initialize();
    void Finalize();

    ID3D11Device5* GetDevice() const { return device_.Get(); }
    ID3D11DeviceContext4* GetContext() const { return context_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11Device5> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> context_;
};

} 
