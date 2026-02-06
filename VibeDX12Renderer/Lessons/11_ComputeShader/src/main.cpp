#include <VibeDX12App.h>
#include <vector>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <filesystem>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace Vibe;
using namespace DirectX;

struct Vertex {
    XMFLOAT3 Pos;
    XMFLOAT2 TexC;
};

class ComputeShaderApp : public VibeDX12App {
public:
    ComputeShaderApp(HINSTANCE hInstance);
    virtual bool Initialize() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;

private:
    void BuildRootSignatures();
    void BuildShadersAndInputLayout();
    void BuildGeometry();
    void BuildTextureAndUAV(); 
    void BuildDescriptorHeaps();
    void BuildPSOs();

    // Root Signatures
    ComPtr<ID3D12RootSignature> m_rootSignatureGraphics;
    ComPtr<ID3D12RootSignature> m_rootSignatureCompute;

    // Heaps
    ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;
    
    // Shaders
    ComPtr<ID3DBlob> m_vsByteCode;
    ComPtr<ID3DBlob> m_psByteCode;
    ComPtr<ID3DBlob> m_csByteCode;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    // PSOs
    ComPtr<ID3D12PipelineState> m_psoGraphics;
    ComPtr<ID3D12PipelineState> m_psoCompute;

    // Resources
    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    
    ComPtr<ID3D12Resource> m_textureInput;      // Source (Read-Only for CS)
    ComPtr<ID3D12Resource> m_textureInputUpload;
    
    ComPtr<ID3D12Resource> m_textureOutput;     // Destination (RW for CS, SRV for PS)

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    UINT m_indexCount = 0;
    UINT m_texWidth = 0;
    UINT m_texHeight = 0;

    XMFLOAT4X4 m_worldViewProj;
};

ComputeShaderApp::ComputeShaderApp(HINSTANCE hInstance) : VibeDX12App(hInstance) {
    m_MainWndCaption = L"Lesson 11: Compute Shader (Image Blur)";
}

bool ComputeShaderApp::Initialize() {
    if (!VibeDX12App::Initialize()) return false;

    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    BuildTextureAndUAV(); // Load tex first to get dims
    BuildRootSignatures();
    BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildGeometry();
    BuildPSOs();

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmdsLists);
    FlushCommandQueue();

    return true;
}

void ComputeShaderApp::BuildRootSignatures() {
    // 1. Graphics Root Signature
    {
        CD3DX12_DESCRIPTOR_RANGE texTable;
        texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

        CD3DX12_ROOT_PARAMETER slotRootParameter[2];
        slotRootParameter[0].InitAsConstants(16, 0); // b0
        slotRootParameter[1].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ShaderRegister = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        
        ComPtr<ID3DBlob> serializedRootSig = nullptr;
        ComPtr<ID3DBlob> errorBlob = nullptr;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob));
        ThrowIfFailed(m_d3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignatureGraphics)));
    }

    // 2. Compute Root Signature
    {
        // Parameter 0: SRV Table (Input) t0
        // Parameter 1: UAV Table (Output) u0
        CD3DX12_DESCRIPTOR_RANGE srvTable;
        srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
        
        CD3DX12_DESCRIPTOR_RANGE uavTable;
        uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

        CD3DX12_ROOT_PARAMETER slotRootParameter[2];
        slotRootParameter[0].InitAsDescriptorTable(1, &srvTable);
        slotRootParameter[1].InitAsDescriptorTable(1, &uavTable);

        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

        ComPtr<ID3DBlob> serializedRootSig = nullptr;
        ComPtr<ID3DBlob> errorBlob = nullptr;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob));
        ThrowIfFailed(m_d3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignatureCompute)));
    }
}

void ComputeShaderApp::BuildShadersAndInputLayout() {
    UINT compileFlags = 0; // D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    ComPtr<ID3DBlob> errors;
    
    HRESULT hr;
    
    // Graphics Shaders
    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0, &m_vsByteCode, &errors);
    if(FAILED(hr)) { 
        if(errors) OutputDebugStringA((char*)errors->GetBufferPointer()); 
        throw std::runtime_error("VS Compile Failed");
    }

    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0, &m_psByteCode, &errors);
    if(FAILED(hr)) { 
        if(errors) OutputDebugStringA((char*)errors->GetBufferPointer()); 
        throw std::runtime_error("PS Compile Failed");
    }

    // Compute Shader
    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "CS", "cs_5_0", compileFlags, 0, &m_csByteCode, &errors);
    if(FAILED(hr)) { 
        if(errors) OutputDebugStringA((char*)errors->GetBufferPointer()); 
        throw std::runtime_error("CS Compile Failed");
    }

    m_inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void ComputeShaderApp::BuildTextureAndUAV() {
    // 1. Load Input Texture
    int width, height, channels;
    const char* filename = "Assets/512x512_Texel_Density_Texture_1.png";
    unsigned char* img = stbi_load(filename, &width, &height, &channels, 4);
    if(!img) img = stbi_load("../Assets/512x512_Texel_Density_Texture_1.png", &width, &height, &channels, 4);
    if(!img) img = stbi_load("../../Assets/512x512_Texel_Density_Texture_1.png", &width, &height, &channels, 4);
    if(!img) throw std::runtime_error("Failed to load texture");

    m_texWidth = width;
    m_texHeight = height;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto heapDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapDefault,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_textureInput)));

    UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_textureInput.Get(), 0, 1);
    auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapUpload,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_textureInputUpload)));

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = img;
    textureData.RowPitch = width * 4;
    textureData.SlicePitch = textureData.RowPitch * height;

    UpdateSubresources(m_commandList.Get(), m_textureInput.Get(), m_textureInputUpload.Get(), 0, 0, 1, &textureData);
    
    // Transition Input to SRV
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_textureInput.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ); // Generic Read includes SRV
    m_commandList->ResourceBarrier(1, &barrier);

    stbi_image_free(img);

    // 2. Create Output Texture (UAV + SRV)
    // IMPORTANT: Format R8G8B8A8_UNORM supports UAV Typed Load/Store in DX12 if hardware supports it.
    // Most do. If not we might need R32G32B32A32_FLOAT. Let's try R8G8B8A8_UNORM.
    
    D3D12_RESOURCE_DESC uavDesc = texDesc;
    uavDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapDefault,
        D3D12_HEAP_FLAG_NONE,
        &uavDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, // Start as UAV
        nullptr,
        IID_PPV_ARGS(&m_textureOutput)));
}

void ComputeShaderApp::BuildDescriptorHeaps() {
    // 3 Descriptors:
    // [0] Input Texture SRV
    // [1] Output Texture UAV
    // [2] Output Texture SRV (for Graphics)
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 3;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvUavHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_srvUavHeap->GetCPUDescriptorHandleForHeapStart());
    UINT descriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // 0: Input SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_textureInput->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    m_d3dDevice->CreateShaderResourceView(m_textureInput.Get(), &srvDesc, hDescriptor);

    // 1: Output UAV
    hDescriptor.Offset(1, descriptorSize);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = m_textureOutput->GetDesc().Format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;
    m_d3dDevice->CreateUnorderedAccessView(m_textureOutput.Get(), nullptr, &uavDesc, hDescriptor);

    // 2: Output SRV
    hDescriptor.Offset(1, descriptorSize);
    srvDesc.Format = m_textureOutput->GetDesc().Format;
    m_d3dDevice->CreateShaderResourceView(m_textureOutput.Get(), &srvDesc, hDescriptor);
}

void ComputeShaderApp::BuildPSOs() {
    // Graphics PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignatureGraphics.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE; // No depth needed for quad
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_psoGraphics)));

    // Compute PSO
    D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
    computePsoDesc.pRootSignature = m_rootSignatureCompute.Get();
    computePsoDesc.CS = { reinterpret_cast<BYTE*>(m_csByteCode->GetBufferPointer()), m_csByteCode->GetBufferSize() };
    computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    ThrowIfFailed(m_d3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_psoCompute)));
}

void ComputeShaderApp::BuildGeometry() {
    // Quad
    std::vector<Vertex> vertices = {
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
        { { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } },
        { {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f } },
        { {  1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } }
    };
    std::vector<uint16_t> indices = { 0, 1, 2, 0, 2, 3 };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);
    m_indexCount = (UINT)indices.size();

    // Create Buffers (Standard)
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)));
        
    // Mapping & Copying data...
    UINT8* pVertexDataBegin;
    m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pVertexDataBegin));
    memcpy(pVertexDataBegin, vertices.data(), vbByteSize);
    m_vertexBuffer->Unmap(0, nullptr);
    
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vbByteSize;

    auto ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &ibDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_indexBuffer)));

    UINT8* pIndexDataBegin;
    m_indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pIndexDataBegin));
    memcpy(pIndexDataBegin, indices.data(), ibByteSize);
    m_indexBuffer->Unmap(0, nullptr);

    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_indexBufferView.SizeInBytes = ibByteSize;

    // Use Identity for Quad
    m_worldViewProj = { 
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 
    };
}

void ComputeShaderApp::OnUpdate() {
}

void ComputeShaderApp::OnRender() {
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_psoCompute.Get()));

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvUavHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, descriptorHeaps);
    UINT descriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // --------------------------------------------------------------------------
    // 1. Compute Pass
    // --------------------------------------------------------------------------
    m_commandList->SetPipelineState(m_psoCompute.Get());
    m_commandList->SetComputeRootSignature(m_rootSignatureCompute.Get());
    
    auto hGPU = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE hGPU_UAV = hGPU;
    hGPU_UAV.ptr += descriptorSize;
    
    // Param 0: Input SRV (Index 0 in heap)
    m_commandList->SetComputeRootDescriptorTable(0, hGPU);
    
    // Param 1: Output UAV (Index 1 in heap)
    m_commandList->SetComputeRootDescriptorTable(1, hGPU_UAV);

    // Dispatch
    // ThreadGroupSize (16, 16, 1)
    // Needs Ceil(Width/16), Ceil(Height/16)
    UINT dpX = (m_texWidth + 15) / 16;
    UINT dpY = (m_texHeight + 15) / 16;
    m_commandList->Dispatch(dpX, dpY, 1);

    // --------------------------------------------------------------------------
    // 2. Resource Transition (UAV -> SRV)
    // --------------------------------------------------------------------------
    auto barrierToSRV = CD3DX12_RESOURCE_BARRIER::Transition(
        m_textureOutput.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_commandList->ResourceBarrier(1, &barrierToSRV);

    // --------------------------------------------------------------------------
    // 3. Graphics Pass (Display Result)
    // --------------------------------------------------------------------------
    m_commandList->SetPipelineState(m_psoGraphics.Get());
    m_commandList->SetGraphicsRootSignature(m_rootSignatureGraphics.Get());
    
    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
    
    auto barrierRTV = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrierRTV);

    auto rtv = CurrentBackBufferView();
    m_commandList->ClearRenderTargetView(rtv, DirectX::Colors::LightGray, 0, nullptr);
    m_commandList->OMSetRenderTargets(1, &rtv, true, nullptr);

    // Bind Output Texture as SRV (Index 2 in Heap)
    D3D12_GPU_DESCRIPTOR_HANDLE hSRV = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
    hSRV.ptr += 2 * descriptorSize;
    
    m_commandList->SetGraphicsRootDescriptorTable(1, hSRV); // Param 1 is Table
    m_commandList->SetGraphicsRoot32BitConstants(0, 16, &m_worldViewProj, 0);

    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    // Transition BackBuffer
    auto barrierPresent = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrierPresent);
    
    // Transition Output Texture back to UAV for next frame
    auto barrierToUAV = CD3DX12_RESOURCE_BARRIER::Transition(
        m_textureOutput.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    m_commandList->ResourceBarrier(1, &barrierToUAV);

    ThrowIfFailed(m_commandList->Close());
    
    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmdLists);
    
    ThrowIfFailed(m_swapChain->Present(1, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;
    FlushCommandQueue();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
    ComputeShaderApp theApp(hInstance);
    if (!theApp.Initialize())
        return 0;
    return theApp.Run();
}
