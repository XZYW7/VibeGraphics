#include <VibeDX12App.h>
#include <iostream>
#include <vector>
#include <DirectXMath.h>

using namespace Vibe;
using namespace DirectX;

struct Vertex {
    XMFLOAT3 Pos;
    XMFLOAT2 TexC;
};

class TexturesApp : public VibeDX12App {
public:
    TexturesApp(HINSTANCE hInstance) : VibeDX12App(hInstance) {
        m_MainWndCaption = L"Lesson 05: Textures";
    }

    virtual bool Initialize() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;

private:
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildGeometry();
    void BuildTexture(); // Create checkerboard texture
    void BuildDescriptorHeaps();
    void BuildPSO();

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    
    ComPtr<ID3DBlob> m_vsByteCode;
    ComPtr<ID3DBlob> m_psByteCode;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    ComPtr<ID3D12PipelineState> m_pso;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    ComPtr<ID3D12Resource> m_texture;
    ComPtr<ID3D12Resource> m_textureUploadHeap;
    
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    UINT m_indexCount = 0;
    struct MathHelper {
        static XMFLOAT4X4 Identity4x4() {
            static XMFLOAT4X4 I(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f);
            return I;
        }
    };
    XMFLOAT4X4 m_worldViewProj = MathHelper::Identity4x4();
    

};

bool TexturesApp::Initialize() {
    if (!VibeDX12App::Initialize()) return false;

    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildGeometry();
    BuildTexture();
    BuildDescriptorHeaps();
    BuildPSO();

    ThrowIfFailed(m_commandList->Close());
    
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();

    return true;
}

void TexturesApp::BuildRootSignature() {
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

    CD3DX12_ROOT_PARAMETER slotRootParameter[2];
    
    // Param 0: Matrix Constants (b0)
    slotRootParameter[0].InitAsConstants(16, 0); 

    // Param 1: Texture Descriptor Table
    slotRootParameter[1].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

    // Static Sampler
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob));

    ThrowIfFailed(m_d3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)));
}

void TexturesApp::BuildShadersAndInputLayout() {
    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0, &m_vsByteCode, &errors);
    if(errors != nullptr) OutputDebugStringA((char*)errors->GetBufferPointer());
    ThrowIfFailed(hr);

    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0, &m_psByteCode, &errors);
    if(errors != nullptr) OutputDebugStringA((char*)errors->GetBufferPointer());
    ThrowIfFailed(hr);

    m_inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void TexturesApp::BuildGeometry() {
    // Cube with Texture Coordinates
    std::vector<Vertex> vertices = {
        // Front face
        { { -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f } },
        { { -1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f } },
        { {  1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f } },
        { {  1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f } },
        
        // Back face
        { { -1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f } },
        { {  1.0f, -1.0f, 1.0f }, { 0.0f, 1.0f } },
        { {  1.0f,  1.0f, 1.0f }, { 0.0f, 0.0f } },
        { { -1.0f,  1.0f, 1.0f }, { 1.0f, 0.0f } },
        
        // Top face
        { { -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f } },
        { { -1.0f, 1.0f,  1.0f }, { 0.0f, 0.0f } },
        { {  1.0f, 1.0f,  1.0f }, { 1.0f, 0.0f } },
        { {  1.0f, 1.0f, -1.0f }, { 1.0f, 1.0f } },
        
        // Bottom face
        { { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f } },
        { {  1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f } },
        { {  1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f } },
        { { -1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f } },
        
        // Left face
        { { -1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f } },
        { { -1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f } },
        { { -1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f } },
        { { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f } },
        
        // Right face
        { { 1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f } },
        { { 1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f } },
        { { 1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f } },
        { { 1.0f, -1.0f,  1.0f }, { 1.0f, 1.0f } },
    };

    std::vector<uint16_t> indices = {
        0, 1, 2, 0, 2, 3,        // Front
        4, 5, 6, 4, 6, 7,        // Back
        8, 9, 10, 8, 10, 11,     // Top
        12, 13, 14, 12, 14, 15,  // Bottom
        16, 17, 18, 16, 18, 19,  // Left
        20, 21, 22, 20, 22, 23   // Right
    };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);
    m_indexCount = (UINT)indices.size();

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    
    // Vertex Buffer
    auto vBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &vBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)));

    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0); 
    ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, vertices.data(), vbByteSize);
    m_vertexBuffer->Unmap(0, nullptr);

    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vbByteSize;

    // Index Buffer
    auto iBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &iBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_indexBuffer)));

    UINT8* pIndexDataBegin;
    ThrowIfFailed(m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
    memcpy(pIndexDataBegin, indices.data(), ibByteSize);
    m_indexBuffer->Unmap(0, nullptr);

    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_indexBufferView.SizeInBytes = ibByteSize;
}

void TexturesApp::BuildTexture() {
    // Generate Checkerboard Data
    const UINT TextureWidth = 256;
    const UINT TextureHeight = 256;
    const UINT PixelSize = 4; // R8G8B8A8
    std::vector<UINT8> data(TextureWidth * TextureHeight * PixelSize);

    for (UINT i = 0; i < TextureHeight; ++i) {
        for (UINT j = 0; j < TextureWidth; ++j) {
            UINT8 color = ((i / 32) % 2) == ((j / 32) % 2) ? 0xFF : 0x00; // Black or White
            UINT index = (i * TextureWidth + j) * PixelSize;
            data[index + 0] = color;
            data[index + 1] = color;
            data[index + 2] = color;
            data[index + 3] = 0xFF; // Alpha
        }
    }

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Width = TextureWidth;
    textureDesc.Height = TextureHeight;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    auto heapPropsDefault = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapPropsDefault,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_texture)));

    // GetRequiredIntermediateSize logic
    D3D12_RESOURCE_DESC desc = m_texture->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT64 rowSizeInBytes;
    UINT64 totalBytes;
    m_d3dDevice->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, &rowSizeInBytes, &totalBytes);
  
    UINT64 uploadBufferSize = totalBytes;
    
    auto heapPropsUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapPropsUpload,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_textureUploadHeap)));

    // Manual copy data to upload heap then to default heap
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = data.data();
    textureData.RowPitch = TextureWidth * PixelSize;
    textureData.SlicePitch = textureData.RowPitch * TextureHeight;

    BYTE* pMappedData = nullptr;
    ThrowIfFailed(m_textureUploadHeap->Map(0, nullptr, reinterpret_cast<void**>(&pMappedData)));

    BYTE* pDestSlice = pMappedData + footprint.Offset;
    const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(textureData.pData);
    for (UINT y = 0; y < TextureHeight; ++y)
    {
        memcpy(pDestSlice + y * footprint.Footprint.RowPitch,
               pSrcSlice + y * textureData.RowPitch,
               TextureWidth * PixelSize);
    }
    m_textureUploadHeap->Unmap(0, nullptr);

    // Command to copy from upload heap to default heap
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = m_texture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = m_textureUploadHeap.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;
    
    m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_commandList->ResourceBarrier(1, &barrier);
}

void TexturesApp::BuildDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    m_d3dDevice->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
}

void TexturesApp::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    
    psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    psoDesc.DSVFormat = m_depthStencilFormat;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
}

void TexturesApp::OnUpdate() {
    static float t = 0.0f;
    t += 0.005f;

    XMVECTOR pos = XMVectorSet(0.0f, 2.0f, -4.0f, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);

    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, (float)m_ClientWidth / m_ClientHeight, 1.0f, 1000.0f);

    XMMATRIX world = XMMatrixRotationY(t) * XMMatrixRotationX(XM_PIDIV4 * 0.5f);
    XMMATRIX worldViewProj = world * view * proj;

    XMStoreFloat4x4(&m_worldViewProj, XMMatrixTranspose(worldViewProj));
}

void TexturesApp::OnRender() {
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pso.Get()));

    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrierToRT);

    auto rtvHandle = CurrentBackBufferView();
    auto dsvHandle = DepthStencilView();
    m_commandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

    const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f }; 
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    
    // Bind Descriptor Heap
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    
    // Set Descriptor Table (Texture)
    m_commandList->SetGraphicsRootDescriptorTable(1, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    m_commandList->SetGraphicsRoot32BitConstants(0, 16, &m_worldViewProj, 0);

    m_commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrierToPresent);

    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmdsLists);

    ThrowIfFailed(m_swapChain->Present(1, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

    FlushCommandQueue();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
    TexturesApp app(hInstance);
    if(!app.Initialize()) return 0;
    return app.Run();
}
