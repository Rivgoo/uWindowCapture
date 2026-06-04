#include "GraphicsManager.h"
#include "D3D11Context.h"
#include "D3D12Context.h"
#include "../Core/Debug.h"
#include "../Unity/IUnityGraphics.h"

namespace uWindowCapture {

UWC_SINGLETON_INSTANCE(GraphicsManager)

void GraphicsManager::Initialize(IUnityInterfaces* unityInterfaces) {
    if (context_) return;

    auto graphics = unityInterfaces->Get<IUnityGraphics>();
    if (!graphics) {
        Debug::Error("GraphicsManager: IUnityGraphics not found.");
        return;
    }

    auto renderer = graphics->GetRenderer();

    if (renderer == kUnityGfxRendererD3D11) {
        Debug::Log("GraphicsManager: Initializing D3D11 Context");
        context_ = std::make_unique<D3D11Context>();
    } else if (renderer == kUnityGfxRendererD3D12) {
        Debug::Log("GraphicsManager: Initializing D3D12 Context");
        context_ = std::make_unique<D3D12Context>();
    } else {
        Debug::Error("GraphicsManager: Unsupported Graphics API type.");
        return;
    }

    if (!context_->Initialize(unityInterfaces)) {
        Debug::Error("GraphicsManager: Failed to initialize graphics context.");
        context_.reset();
    }
}

void GraphicsManager::Finalize() {
    if (context_) {
        context_->Finalize();
        context_.reset();
    }
}

IGraphicsContext* GraphicsManager::GetContext() const {
    return context_.get();
}

} 
