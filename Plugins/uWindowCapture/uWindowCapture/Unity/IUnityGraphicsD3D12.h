#pragma once
#include "IUnityInterface.h"
#include <d3d12.h>

UNITY_DECLARE_INTERFACE(IUnityGraphicsD3D12v5)
{
    ID3D12Device* (UNITY_INTERFACE_API * GetDevice)();
    ID3D12Fence* (UNITY_INTERFACE_API * GetFrameFence)();
    UINT64(UNITY_INTERFACE_API * GetNextFrameFenceValue)();
    bool (UNITY_INTERFACE_API * ExecuteCommandList)(ID3D12GraphicsCommandList * commandList, int stateCount, D3D12_RESOURCE_STATES * states);
    void (UNITY_INTERFACE_API * SetState)(ID3D12Resource * resource, D3D12_RESOURCE_STATES state);
    int (UNITY_INTERFACE_API * GetState)(ID3D12Resource * resource, D3D12_RESOURCE_STATES * state);
    ID3D12CommandQueue* (UNITY_INTERFACE_API * GetCommandQueue)();
};
UNITY_REGISTER_INTERFACE_GUID(0x1992797614A5408DULL, 0xA26D3F96324D6386ULL, IUnityGraphicsD3D12v5)