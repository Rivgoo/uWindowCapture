#pragma once
#include "../Core/Singleton.h"
#include "IGraphicsContext.h"
#include <memory>

namespace uWindowCapture {

class GraphicsManager {
    UWC_SINGLETON(GraphicsManager)
public:
    void Initialize(IUnityInterfaces* unityInterfaces);
    void Finalize();
    
    IGraphicsContext* GetContext() const;
    bool IsInitialized() const { return context_ != nullptr; }

private:
    std::unique_ptr<IGraphicsContext> context_;
};

} 
