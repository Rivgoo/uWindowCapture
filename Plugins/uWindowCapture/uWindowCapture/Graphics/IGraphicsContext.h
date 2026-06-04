#pragma once
#include "../Unity/IUnityInterface.h"

namespace uWindowCapture {

class SharedTextureResource;

class IGraphicsContext {
public:
    virtual ~IGraphicsContext() = default;
    
    virtual bool Initialize(IUnityInterfaces* unityInterfaces) = 0;
    virtual void Finalize() = 0;
    
    virtual void RenderEvent(int eventId) = 0;

    // Called by Unity components to register a shared resource for updating a specific Unity texture
    virtual void RegisterSharedResource(void* unityTexturePtr, SharedTextureResource* sharedResource) = 0;
    virtual void UnregisterSharedResource(void* unityTexturePtr) = 0;
};

} 
