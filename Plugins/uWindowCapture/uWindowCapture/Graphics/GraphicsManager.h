#pragma once
#include "../Core/Singleton.h"
#include "IGraphicsContext.h"
#include <memory>

class GraphicsManager {
    UWC_SINGLETON(GraphicsManager)
public:
    void Initialize(int apiType); // 1 = D3D11, 2 = D3D12
    void Finalize();
    
    IGraphicsContext* GetContext() const;
    bool IsInitialized() const { return context_ != nullptr; }

private:
    std::unique_ptr<IGraphicsContext> context_;
};
