#pragma once

#include "stdafx.h"

class RenderTexture
{
public:
    RenderTexture(DXGI_FORMAT format) noexcept;

    void SetDevice(_In_ ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srvDescriptor, D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptor);

    void SizeResources(size_t width, size_t height);

    void ReleaseDevice() noexcept;

    void TransitionTo(_In_ ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES afterState);

    void BeginScene(_In_ ID3D12GraphicsCommandList* commandList);

    void EndScene(_In_ ID3D12GraphicsCommandList* commandList);

public:
    void SetClearColor(DirectX::FXMVECTOR color);

    ID3D12Resource* GetResource() const noexcept;
    D3D12_RESOURCE_STATES GetCurrentState() const noexcept;

    void SetWindow(const RECT& rect);

    DXGI_FORMAT GetFormat() const noexcept;

private:
    Microsoft::WRL::ComPtr<ID3D12Device>                m_device;
    Microsoft::WRL::ComPtr<ID3D12Resource>              m_resource;
    D3D12_RESOURCE_STATES                               m_state;
    D3D12_CPU_DESCRIPTOR_HANDLE                         m_srvDescriptor;
    D3D12_CPU_DESCRIPTOR_HANDLE                         m_rtvDescriptor;
    float                                               m_clearColor[4];

    DXGI_FORMAT                                         m_format;

    size_t                                              m_width;
    size_t                                              m_height;
};