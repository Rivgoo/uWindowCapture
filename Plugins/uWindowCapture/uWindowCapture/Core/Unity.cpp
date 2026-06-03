#include <d3d11.h>
#include "../Unity/IUnityInterface.h"
#include "../Unity/IUnityGraphicsD3D11.h"

extern IUnityInterfaces* g_unity;

IUnityInterfaces* GetUnity()
{
    return g_unity;
}

ID3D11Device* GetUnityDevice()
{
    return GetUnity()->Get<IUnityGraphicsD3D11>()->GetDevice();
}
