#include <VibeDX12App.h>
#include <VibeCamera.h>
#include <iostream>
#include <vector>
#include <DirectXMath.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace Vibe;
using namespace DirectX;

static XMFLOAT4X4 MatrixIdentity4x4() {
    XMFLOAT4X4 I;
    XMStoreFloat4x4(&I, XMMatrixIdentity());
    return I;
}

struct Vertex {
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
    XMFLOAT2 TexC;
};

// 56 floats
struct ObjectConstants {
    XMFLOAT4X4 World;
    XMFLOAT4X4 ViewProj;
    XMFLOAT3 EyePosW;
    float Pad0;

    XMFLOAT4 DiffuseAlbedo;
    XMFLOAT3 FresnelR0;
    float Roughness;

    XMFLOAT3 LightDir;
    float Pad1;
    XMFLOAT4 LightColor;
    XMFLOAT4 AmbientLight;
};

struct RenderItem {
    XMFLOAT4X4 World = MatrixIdentity4x4();
    XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    XMFLOAT3 FresnelR0 = { 0.1f, 0.1f, 0.1f };
    float Roughness = 0.5f;
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    INT BaseVertexLocation = 0;
};

class TexturedLightingApp : public VibeDX12App {
public:
    TexturedLightingApp(HINSTANCE hInstance) : VibeDX12App(hInstance) {
        m_MainWndCaption = L"Lesson 07-2: Textured Lighting";
        m_camera.SetPosition(0.0f, 2.0f, -6.0f);
    }

    virtual bool Initialize() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildGeometry();
    void LoadTextures();
    void BuildDescriptorHeaps();
    void BuildPSO();
    
    void BuildRenderItems();

    void OnKeyboardInput();
    void OnMouseMove(WPARAM btnState, int x, int y);

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

    std::vector<RenderItem> m_allRitems;

    VibeCamera m_camera;
    ObjectConstants m_constants;
    
    POINT mLastMousePos;
};

bool TexturedLightingApp::Initialize() {
    if (!VibeDX12App::Initialize()) return false;

    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    m_camera.SetLens(0.25f * XM_PI, static_cast<float>(m_ClientWidth) / m_ClientHeight, 1.0f, 1000.0f);

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildGeometry();
    LoadTextures();
    BuildDescriptorHeaps(); 
    BuildRenderItems();
    BuildPSO();

    ThrowIfFailed(m_commandList->Close());
    
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    
    FlushCommandQueue();

    return true;
}

LRESULT TexturedLightingApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            mLastMousePos.x = LOWORD(lParam);
            mLastMousePos.y = HIWORD(lParam);
            SetCapture(hwnd);
            return 0;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            ReleaseCapture();
            return 0;
        case WM_MOUSEMOVE:
            OnMouseMove(wParam, LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_SIZE:
             m_ClientWidth = LOWORD(lParam);
             m_ClientHeight = HIWORD(lParam);
             if(m_d3dDevice && m_ClientHeight > 0) {
                 m_camera.SetLens(0.25f * XM_PI, static_cast<float>(m_ClientWidth)/m_ClientHeight, 1.0f, 1000.0f);
             }
             return 0;
    }
    return VibeDX12App::MsgProc(hwnd, msg, wParam, lParam);
}

void TexturedLightingApp::OnKeyboardInput() {
    float dt = 0.016f; 
    float speed = 5.0f; 
    if (GetAsyncKeyState('W') & 0x8000) m_camera.Walk(speed * dt);
    if (GetAsyncKeyState('S') & 0x8000) m_camera.Walk(-speed * dt);
    if (GetAsyncKeyState('A') & 0x8000) m_camera.Strafe(-speed * dt);
    if (GetAsyncKeyState('D') & 0x8000) m_camera.Strafe(speed * dt);
}

void TexturedLightingApp::OnMouseMove(WPARAM btnState, int x, int y) {
    if ((btnState & MK_LBUTTON) != 0) {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
        m_camera.Pitch(dy);
        m_camera.RotateY(dx);
    }
    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void TexturedLightingApp::OnUpdate() {
    OnKeyboardInput();
    m_camera.UpdateViewMatrix();

    XMMATRIX viewProj = m_camera.GetViewProj();
    XMStoreFloat4x4(&m_constants.ViewProj, XMMatrixTranspose(viewProj));
    m_constants.EyePosW = m_camera.GetPosition3f();
    
    m_constants.LightDir = { 0.57735f, -0.57735f, 0.57735f };
    m_constants.LightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_constants.AmbientLight = { 0.2f, 0.2f, 0.2f, 1.0f };
}

void TexturedLightingApp::BuildRenderItems() {
    // 1. Center Sphere
    RenderItem item1;
    XMStoreFloat4x4(&item1.World, XMMatrixTranspose(XMMatrixTranslation(0.0f, 0.5f, 0.0f)));
    item1.DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    item1.FresnelR0 = { 0.1f, 0.1f, 0.1f };
    item1.Roughness = 0.25f;
    item1.IndexCount = 20 * 20 * 6;
    item1.StartIndexLocation = 0;
    item1.BaseVertexLocation = 0;
    m_allRitems.push_back(item1);

    // 2. Grid
    RenderItem item2;
    XMStoreFloat4x4(&item2.World, XMMatrixTranspose(XMMatrixTranslation(0.0f, -0.5f, 0.0f)));
    item2.DiffuseAlbedo = { 0.8f, 0.8f, 0.8f, 1.0f };
    item2.FresnelR0 = { 0.02f, 0.02f, 0.02f };
    item2.Roughness = 0.8f;
    item2.IndexCount = 6;
    item2.StartIndexLocation = 20 * 20 * 6;
    item2.BaseVertexLocation = (20 + 1) * (20 + 1);
    m_allRitems.push_back(item2);
}

void TexturedLightingApp::LoadTextures() {
    int width, height, channels;
    unsigned char* data = stbi_load("Assets/512x512_Texel_Density_Texture_1.png", &width, &height, &channels, 4);
    if (!data) {
        MessageBoxA(nullptr, "Failed to load texture", "Error", MB_OK);
        return;
    }

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Alignment = 0;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_texture)));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

    auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_textureUploadHeap)));

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = data;
    textureData.RowPitch = width * 4;
    textureData.SlicePitch = textureData.RowPitch * height;

    UpdateSubresources(m_commandList.Get(), m_texture.Get(), m_textureUploadHeap.Get(), 0, 0, 1, &textureData);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_commandList->ResourceBarrier(1, &barrier);

    // Keep data alive until flush in Initialize() -- executed immediately after this call returns by caller
    // stbi_image_free(data) technically should wait, but stbi buffer is copied to upload heap immediately.
    // The upload heap must be kept alive until GPU executes command list. We store m_textureUploadHeap as member.
    
    // safe to free cpu memory now? yes, UpdateSubresources copies to upload heap cpu mapping.
    stbi_image_free(data);
}

void TexturedLightingApp::BuildDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

    // Create SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Should match texture
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    m_d3dDevice->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
}

void TexturedLightingApp::BuildRootSignature() {
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

    CD3DX12_ROOT_PARAMETER slotRootParameter[2];
    
    // b0
    slotRootParameter[0].InitAsConstants(56, 0); 
    // t0
    slotRootParameter[1].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

    // Samplers
    CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR); // s0 = Linear

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

void TexturedLightingApp::BuildShadersAndInputLayout() {
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
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void TexturedLightingApp::BuildGeometry() {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    // --- Sphere Geometry ---
    float radius = 1.0f;
    UINT stackCount = 20;
    UINT sliceCount = 20;

    // TopVertex
    Vertex topVertex; 
    topVertex.Pos = { 0.0f, radius, 0.0f }; 
    topVertex.Normal = { 0.0f, 1.0f, 0.0f };
    topVertex.TexC = { 0.0f, 0.0f };
    vertices.push_back(topVertex);

    float phiStep = XM_PI / stackCount;
    float thetaStep = 2.0f * XM_PI / sliceCount;

    for(UINT i = 1; i <= stackCount - 1; ++i) {
        float phi = i * phiStep;
        for(UINT j = 0; j <= sliceCount; ++j) {
            float theta = j * thetaStep;
            Vertex v;
            v.Pos.x = radius * sinf(phi) * cosf(theta);
            v.Pos.y = radius * cosf(phi);
            v.Pos.z = radius * sinf(phi) * sinf(theta);
            XMVECTOR p = XMLoadFloat3(&v.Pos);
            XMStoreFloat3(&v.Normal, XMVector3Normalize(p));
            
            // UVs
            v.TexC.x = theta / XM_2PI;
            v.TexC.y = phi / XM_PI;
            
            vertices.push_back(v);
        }
    }
    // BottomVertex
    Vertex bottomVertex; 
    bottomVertex.Pos = { 0.0f, -radius, 0.0f }; 
    bottomVertex.Normal = { 0.0f, -1.0f, 0.0f };
    bottomVertex.TexC = { 0.0f, 1.0f };
    vertices.push_back(bottomVertex);

    UINT ringVertexCount = sliceCount + 1;
    // Top Ring
    for(UINT i = 0; i < sliceCount; ++i) {
        indices.push_back(0); indices.push_back(i + 2); indices.push_back(i + 1);
    }
    // Inner Rings
    UINT baseIndex = 1;
    for(UINT i = 0; i < stackCount - 2; ++i) {
        for(UINT j = 0; j < sliceCount; ++j) {
            indices.push_back(baseIndex + i * ringVertexCount + j);
            indices.push_back(baseIndex + i * ringVertexCount + j + 1);
            indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
            indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
            indices.push_back(baseIndex + i * ringVertexCount + j + 1);
            indices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
        }
    }
    // Bottom Ring
    UINT southPoleIndex = (UINT)vertices.size() - 1;
    baseIndex = southPoleIndex - ringVertexCount;
    for(UINT i = 0; i < sliceCount; ++i) {
        indices.push_back(southPoleIndex); indices.push_back(baseIndex + i); indices.push_back(baseIndex + i + 1);
    }

    // --- Grid Geometry --- (4 vertices, 2 triangles)
    float width = 20.0f; 
    float depth = 20.0f;
    
    Vertex v[4];
    v[0] = { {-width*0.5f, 0.0f, -depth*0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 10.0f} }; // Tile texture 10x
    v[1] = { {-width*0.5f, 0.0f, +depth*0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f} };
    v[2] = { {+width*0.5f, 0.0f, +depth*0.5f}, {0.0f, 1.0f, 0.0f}, {10.0f, 0.0f} };
    v[3] = { {+width*0.5f, 0.0f, -depth*0.5f}, {0.0f, 1.0f, 0.0f}, {10.0f, 10.0f} };
    
    vertices.push_back(v[0]); vertices.push_back(v[1]);
    vertices.push_back(v[2]); vertices.push_back(v[3]);
    
    indices.push_back(0); indices.push_back(1); indices.push_back(2);
    indices.push_back(0); indices.push_back(2); indices.push_back(3);

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    
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

void TexturedLightingApp::BuildPSO() {
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
    psoDesc.DSVFormat = m_depthStencilFormat;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
}

void TexturedLightingApp::OnRender() {
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pso.Get()));
    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
    
    auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
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
    
    // Bind Texture Table (T0)
    m_commandList->SetGraphicsRootDescriptorTable(1, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw Render Items
    for(auto& ri : m_allRitems) {
        // Update per-object constants
        m_constants.World = ri.World;
        m_constants.DiffuseAlbedo = ri.DiffuseAlbedo;
        m_constants.FresnelR0 = ri.FresnelR0;
        m_constants.Roughness = ri.Roughness;

        // Bind Constants (B0)
        m_commandList->SetGraphicsRoot32BitConstants(0, 56, &m_constants, 0);
        m_commandList->DrawIndexedInstanced(ri.IndexCount, 1, ri.StartIndexLocation, ri.BaseVertexLocation, 0);
    }
    
    auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrierToPresent);
    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmdsLists);
    ThrowIfFailed(m_swapChain->Present(1, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;
    FlushCommandQueue();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
    try {
        TexturedLightingApp app(hInstance);
        if(!app.Initialize()) return 0;
        return app.Run();
    }
    catch(std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Exception", MB_OK);
        return 0;
    }
}
