#include "GraphicsManager.h"
#include "D3D11Context.h"
#include "D3D12Context.h"
#include "../Core/Debug.h"

UWC_SINGLETON_INSTANCE(GraphicsManager)

void GraphicsManager::Initialize(int apiType) {
    if (context_) return;

    if (apiType == 1) {
        Debug::Log("GraphicsManager: Initializing D3D11 Context");
        context_ = std::make_unique<D3D11Context>();
    } else if (apiType == 2) {
        Debug::Log("GraphicsManager: Initializing D3D12 Context");
        context_ = std::make_unique<D3D12Context>();
    } else {
        Debug::Error("GraphicsManager: Unsupported Graphics API type: ", apiType);
        return;
    }

    if (!context_->Initialize()) {
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
