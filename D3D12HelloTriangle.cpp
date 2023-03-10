//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "D3D12HelloTriangle.h"

D3D12HelloTriangle::D3D12HelloTriangle(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frameIndex(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtvDescriptorSize(0)
{
}

void D3D12HelloTriangle::OnInit()
{
    LoadPipeline();
    LoadAssets();
}

// Load the rendering pipeline dependencies.
void D3D12HelloTriangle::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a shader resource view (SRV) heap for the shader parameters.
        for (int i = 0; i < FrameCount; ++i)
        {
            D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
            srvHeapDesc.NumDescriptors = 2;
            srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_descriptorHeap[i])));

            m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount * 2;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

// Load the sample assets.
void D3D12HelloTriangle::LoadAssets()
{
    // Create Root signature
    {
        // Descriptor range (one type cbv)
        D3D12_DESCRIPTOR_RANGE  descriptorTableRanges[2]; // only one range right now
        descriptorTableRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; // this is a range of constant buffer views (descriptors)
        descriptorTableRanges[0].NumDescriptors = 1; // we only have one constant buffer, so the range is only 1
        descriptorTableRanges[0].BaseShaderRegister = 0; // start index of the shader registers in the range
        descriptorTableRanges[0].RegisterSpace = 0; // space 0. can usually be zero
        descriptorTableRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables

        descriptorTableRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // this is a range of constant buffer views (descriptors)
        descriptorTableRanges[1].NumDescriptors = 1; // we only have one texture, so the range is only 1
        descriptorTableRanges[1].BaseShaderRegister = 0; // start index of the shader registers in the range
        descriptorTableRanges[1].RegisterSpace = 0; // space 0. can usually be zero
        descriptorTableRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables

        // create a descriptor table (multiple type)
        D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
        descriptorTable.NumDescriptorRanges = _countof(descriptorTableRanges); // we only have one range
        descriptorTable.pDescriptorRanges = &descriptorTableRanges[0]; // the pointer to the beginning of our ranges array

        // create a static sampler
        D3D12_STATIC_SAMPLER_DESC staticSampler = {};
        staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        staticSampler.MipLODBias = 0;
        staticSampler.MaxAnisotropy = 0;
        staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        staticSampler.MinLOD = 0.0f;
        staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
        staticSampler.ShaderRegister = 0;
        staticSampler.RegisterSpace = 0;
        staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // Create root signature.
        {
            // create a root parameter and fill it out
            D3D12_ROOT_PARAMETER  rootParameters[1];
            rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // this is a descriptor table
            rootParameters[0].DescriptorTable = descriptorTable; // this is our descriptor table for this root parameter
            rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init(_countof(rootParameters), 
                rootParameters,
                1, 
                &staticSampler,
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
            );

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
            ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        }
    }

    m_shaderData.solidColor = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);

    // Create the constant buffers.
    {
        // create a resource heap, descriptor heap, and pointer to cbv for each frame
        for (int i = 0; i < FrameCount; ++i)
        {
            ThrowIfFailed(m_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
                D3D12_HEAP_FLAG_NONE, // no flags
                &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64), // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
                D3D12_RESOURCE_STATE_GENERIC_READ, // will be data that is read from so we keep it in the generic read state
                nullptr, // we do not have use an optimized clear value for constant buffers
                IID_PPV_ARGS(&m_ressourcesMemory[i])
            ));
            m_ressourcesMemory[i]->SetName(L"Constant Buffer Upload Resource Heap");

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = m_ressourcesMemory[i]->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = (sizeof(ShaderData) + 255) & ~255;    // CB size is required to be 256-byte aligned.
            m_device->CreateConstantBufferView(&cbvDesc, m_descriptorHeap[i]->GetCPUDescriptorHandleForHeapStart());

            // Get cpu mappable address
            CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU. (End is less than or equal to begin)
            ThrowIfFailed(m_ressourcesMemory[i]->Map(0, &readRange, reinterpret_cast<void**>(&m_writableAdresses[i])));
            memcpy(m_writableAdresses[i], &m_shaderData, sizeof(ShaderData));
        }
    }

    // Create RenderTexture
    {
        RECT dimension;
        dimension.left = m_viewport.TopLeftX;
        dimension.top = m_viewport.TopLeftY;
        dimension.right = m_viewport.TopLeftX + m_viewport.Width;
        dimension.bottom = m_viewport.TopLeftY + m_viewport.Height;

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
        // Render Target offset
        rtvHandle.Offset(2, m_rtvDescriptorSize);

        for (UINT i = 0; i < FrameCount; i++)
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_descriptorHeap[i]->GetCPUDescriptorHandleForHeapStart());
            srvHandle.Offset(1, m_srvDescriptorSize);
            m_renderTexture[i] = new RenderTexture(DXGI_FORMAT_R8G8B8A8_UNORM);
            m_renderTexture[i]->SetClearColor({ 0.1f, 0.1f, 1.0f, 1.0f });
            m_renderTexture[i]->SetDevice(m_device.Get(), srvHandle, rtvHandle);
            m_renderTexture[i]->SetWindow(dimension);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC trianglePsoDesc = {};
    // Create pipeline states, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> triangleVertexShader;
        ComPtr<ID3DBlob> trianglePixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &triangleVertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &trianglePixelShader, nullptr));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        trianglePsoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        trianglePsoDesc.pRootSignature = m_rootSignature.Get();
        trianglePsoDesc.VS = CD3DX12_SHADER_BYTECODE(triangleVertexShader.Get());
        trianglePsoDesc.PS = CD3DX12_SHADER_BYTECODE(trianglePixelShader.Get());
        trianglePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        trianglePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        trianglePsoDesc.DepthStencilState.DepthEnable = FALSE;
        trianglePsoDesc.DepthStencilState.StencilEnable = FALSE;
        trianglePsoDesc.SampleMask = UINT_MAX;
        trianglePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        trianglePsoDesc.NumRenderTargets = 1;
        trianglePsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        trianglePsoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&trianglePsoDesc, IID_PPV_ARGS(&m_trianglePipelineState)));
    }

    {
        // Quad PSO
        ComPtr<ID3DBlob> quadVertexShader;
        ComPtr<ID3DBlob> quadPixelShader;
        ID3DBlob* errorBlob1 = nullptr;
        ID3DBlob* errorBlob2 = nullptr;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif


        HRESULT hr = D3DCompileFromFile(GetAssetFullPath(L"quad_shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &quadVertexShader, &errorBlob1);
        hr &= D3DCompileFromFile(GetAssetFullPath(L"quad_shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &quadPixelShader, &errorBlob2);

        if (FAILED(hr)) {
            if (errorBlob1) {
                OutputDebugStringA((char*)errorBlob1->GetBufferPointer());
                errorBlob1->Release();
            } 
            if (errorBlob2) {
                OutputDebugStringA((char*)errorBlob2->GetBufferPointer());
                errorBlob2->Release();
            }
            throw HrException(hr);
        }

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPsoDesc = trianglePsoDesc;
        quadPsoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        quadPsoDesc.VS = CD3DX12_SHADER_BYTECODE(quadVertexShader.Get());
        quadPsoDesc.PS = CD3DX12_SHADER_BYTECODE(quadPixelShader.Get());
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&quadPsoDesc, IID_PPV_ARGS(&m_quadPipelineState)));
    }

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_trianglePipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_commandList->Close());

    // Create the Triangle vertex buffer.
    {
        // Define the geometry for a triangle.
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        };

        const UINT vertexBufferSize = sizeof(triangleVertices);

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_triangleVertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_triangleVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        m_triangleVertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_triangleVertexBufferView.BufferLocation = m_triangleVertexBuffer->GetGPUVirtualAddress();
        m_triangleVertexBufferView.StrideInBytes = sizeof(Vertex);
        m_triangleVertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // Create the Quad vertex buffer.
    {
        // Define the geometry for a quad.
        TextureVertex quadVertices[] =
        {
            { { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f} },
            { { 1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f} },
            { { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f} },

            { { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } },
            { {  1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
            { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } }
        };

        const UINT vertexBufferSize = sizeof(quadVertices);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_quadVertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_quadVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, quadVertices, sizeof(quadVertices));
        m_quadVertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_quadVertexBufferView.BufferLocation = m_quadVertexBuffer->GetGPUVirtualAddress();
        m_quadVertexBufferView.StrideInBytes = sizeof(TextureVertex);
        m_quadVertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

// Update frame-based values.
void D3D12HelloTriangle::OnUpdate()
{
    // update app logic, such as moving the camera or figuring out what objects are in view
    static float rIncrement = 0.002f;
    static float gIncrement = 0.006f;
    static float bIncrement = 0.009f;

    m_shaderData.solidColor.x += rIncrement;
    m_shaderData.solidColor.y += gIncrement;
    m_shaderData.solidColor.z += bIncrement;

    if (m_shaderData.solidColor.x >= 1.0 || m_shaderData.solidColor.x <= 0.0)
    {
        m_shaderData.solidColor.x = m_shaderData.solidColor.x >= 1.0 ? 1.0 : 0.0;
        rIncrement = -rIncrement;
    }
    if (m_shaderData.solidColor.y >= 1.0 || m_shaderData.solidColor.y <= 0.0)
    {
        m_shaderData.solidColor.y = m_shaderData.solidColor.y >= 1.0 ? 1.0 : 0.0;
        gIncrement = -gIncrement;
    }
    if (m_shaderData.solidColor.z >= 1.0 || m_shaderData.solidColor.z <= 0.0)
    {
        m_shaderData.solidColor.z = m_shaderData.solidColor.z >= 1.0 ? 1.0 : 0.0;
        bIncrement = -bIncrement;
    }

    // copy our ConstantBuffer instance to the mapped constant buffer resource
    memcpy(m_writableAdresses[m_frameIndex], &m_shaderData, sizeof(ShaderData));

}

// Render the scene.
void D3D12HelloTriangle::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void D3D12HelloTriangle::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    for (int i = 0; i < FrameCount; ++i)
    {
        SAFE_RELEASE(m_descriptorHeap[i]);
        SAFE_RELEASE(m_ressourcesMemory[i]);
        m_renderTexture[i]->ReleaseDevice();
        delete m_renderTexture[i];
    };

    CloseHandle(m_fenceEvent);
}

void D3D12HelloTriangle::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_trianglePipelineState.Get()));

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Set descriptors Heaps
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_descriptorHeap[m_frameIndex].Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    m_commandList->SetGraphicsRootDescriptorTable(0, m_descriptorHeap[m_frameIndex]->GetGPUDescriptorHandleForHeapStart());

    // -------------------------------- Draw Triangle 
    m_renderTexture[m_frameIndex]->BeginScene(m_commandList.Get());
    CD3DX12_CPU_DESCRIPTOR_HANDLE offscreenHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex + 2, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &offscreenHandle, FALSE, nullptr);

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->SetPipelineState(m_trianglePipelineState.Get());
    m_commandList->IASetVertexBuffers(0, 1, &m_triangleVertexBufferView);
    m_commandList->DrawInstanced(3, 1, 0, 0);

    m_renderTexture[m_frameIndex]->EndScene(m_commandList.Get());

    // -------------------------------- Draw Quad 
    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->SetPipelineState(m_quadPipelineState.Get());
    m_commandList->IASetVertexBuffers(0, 1, &m_quadVertexBufferView);
    m_commandList->DrawInstanced(6, 1, 0, 0);

    // -------------------------------- Present frame
    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_commandList->Close());
}

void D3D12HelloTriangle::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
