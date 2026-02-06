#include <VibeDX12App.h>
#include <VibeCamera.h>
#include <vector>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <filesystem>
#include <chrono>  // For simple timer
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace Vibe;
using namespace DirectX;

struct Vertex {
    XMFLOAT3 Pos;
    XMFLOAT2 TexC;
};

class TessellationTerrainApp : public VibeDX12App {
public:
    TessellationTerrainApp(HINSTANCE hInstance);
    virtual bool Initialize() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildGeometry();
    void BuildTexture();
    void BuildDescriptorHeaps();
    void BuildPSO();
    void OnKeyboardInput(float dt);

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    
    // Camera
    VibeCamera m_camera;
    POINT m_lastMousePos;
    std::chrono::high_resolution_clock::time_point m_lastTime;
    
    // Shaders
    ComPtr<ID3DBlob> m_vsByteCode;
    ComPtr<ID3DBlob> m_hsByteCode;
    ComPtr<ID3DBlob> m_dsByteCode;
    ComPtr<ID3DBlob> m_psByteCode;
    
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
    ComPtr<ID3D12PipelineState> m_pso;

    // Geometry
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    UINT m_vertexCount = 0;

    // Texture
    ComPtr<ID3D12Resource> m_texture;
    ComPtr<ID3D12Resource> m_textureUpload;

    XMFLOAT4X4 m_worldViewProj;
};

TessellationTerrainApp::TessellationTerrainApp(HINSTANCE hInstance) : VibeDX12App(hInstance) {
    m_MainWndCaption = L"Lesson 12-1: Tessellation with Texture";
    // Position camera just above the center of the terrain, looking slightly down.
    m_camera.SetPosition(0.0f, 15.0f, -20.0f);
    m_camera.Pitch(XMConvertToRadians(30.0f));
}

bool TessellationTerrainApp::Initialize() {
    if (!VibeDX12App::Initialize()) return false;
    
    // Init Camera Lens
    float aspect = static_cast<float>(m_ClientWidth) / m_ClientHeight;
    m_camera.SetLens(0.25f * XM_PI, aspect, 1.0f, 1000.0f);
    m_lastTime = std::chrono::high_resolution_clock::now();

    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    BuildTexture(); // Load texture first
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildGeometry();
    BuildPSO();

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmdsLists);
    FlushCommandQueue();

    return true;
}

LRESULT TessellationTerrainApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            m_lastMousePos.x = LOWORD(lParam);
            m_lastMousePos.y = HIWORD(lParam);
            SetCapture(hwnd);
            return 0;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            ReleaseCapture();
            return 0;
        case WM_MOUSEMOVE:
            if(wParam & MK_LBUTTON) {
                float dx = XMConvertToRadians(0.15f * static_cast<float>(LOWORD(lParam) - m_lastMousePos.x));
                float dy = XMConvertToRadians(0.15f * static_cast<float>(HIWORD(lParam) - m_lastMousePos.y));

                m_camera.Pitch(dy);
                m_camera.RotateY(dx);
            }
            m_lastMousePos.x = LOWORD(lParam);
            m_lastMousePos.y = HIWORD(lParam);
            return 0;
    }
    return VibeDX12App::MsgProc(hwnd, msg, wParam, lParam);
}

void TessellationTerrainApp::BuildRootSignature() {
    // 1. Constant Buffer (MVP)
    // 2. Texture SRV Table
    D3D12_DESCRIPTOR_RANGE texTable;
    texTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texTable.NumDescriptors = 1;
    texTable.BaseShaderRegister = 0;
    texTable.RegisterSpace = 0;
    texTable.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER slotRootParameter[2];
    
    // Param 0: Constants
    slotRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    slotRootParameter[0].Constants.ShaderRegister = 0;
    slotRootParameter[0].Constants.RegisterSpace = 0;
    slotRootParameter[0].Constants.Num32BitValues = 16;
    slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // DS needs it

    // Param 1: Descriptor Table (Texture)
    slotRootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    slotRootParameter[1].DescriptorTable.NumDescriptorRanges = 1;
    slotRootParameter[1].DescriptorTable.pDescriptorRanges = &texTable;
    slotRootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Sampler
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 2;
    rootSigDesc.pParameters = slotRootParameter;
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = &sampler;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob));
    ThrowIfFailed(m_d3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

void TessellationTerrainApp::BuildTexture() {
    int width, height, channels;
    const char* filename = "Assets/512x512_Texel_Density_Texture_1.png";
    unsigned char* img = stbi_load(filename, &width, &height, &channels, 4);
    if(!img) img = stbi_load("../Assets/512x512_Texel_Density_Texture_1.png", &width, &height, &channels, 4);
    if(!img) img = stbi_load("../../Assets/512x512_Texel_Density_Texture_1.png", &width, &height, &channels, 4);
    if(!img) throw std::runtime_error("Failed to load texture");

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
        IID_PPV_ARGS(&m_texture)));

    UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
    auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapUpload,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_textureUpload)));

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = img;
    textureData.RowPitch = width * 4;
    textureData.SlicePitch = textureData.RowPitch * height;

    UpdateSubresources(m_commandList.Get(), m_texture.Get(), m_textureUpload.Get(), 0, 0, 1, &textureData);
    
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_commandList->ResourceBarrier(1, &barrier);

    stbi_image_free(img);
}

void TessellationTerrainApp::BuildDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_texture->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
    m_d3dDevice->CreateShaderResourceView(m_texture.Get(), &srvDesc, hDescriptor);
}

void TessellationTerrainApp::BuildShadersAndInputLayout() {
    UINT compileFlags = 0; 
    ComPtr<ID3DBlob> errors;
    HRESULT hr;

    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0, &m_vsByteCode, &errors);
    if(FAILED(hr)) { if(errors) OutputDebugStringA((char*)errors->GetBufferPointer()); throw std::runtime_error("VS Compile Failed"); }

    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "HS", "hs_5_0", compileFlags, 0, &m_hsByteCode, &errors);
    if(FAILED(hr)) { if(errors) OutputDebugStringA((char*)errors->GetBufferPointer()); throw std::runtime_error("HS Compile Failed"); }

    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "DS", "ds_5_0", compileFlags, 0, &m_dsByteCode, &errors);
    if(FAILED(hr)) { if(errors) OutputDebugStringA((char*)errors->GetBufferPointer()); throw std::runtime_error("DS Compile Failed"); }

    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0, &m_psByteCode, &errors);
    if(FAILED(hr)) { if(errors) OutputDebugStringA((char*)errors->GetBufferPointer()); throw std::runtime_error("PS Compile Failed"); }

    m_inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void TessellationTerrainApp::BuildGeometry() {
    float size = 15.0f;
    // 4 Control Points for a Quad with UVs
    std::vector<Vertex> vertices = {
        { { -size, 0.0f,  size }, { 0.0f, 0.0f } }, // TL
        { {  size, 0.0f,  size }, { 1.0f, 0.0f } }, // TR
        { { -size, 0.0f, -size }, { 0.0f, 1.0f } }, // BL
        { {  size, 0.0f, -size }, { 1.0f, 1.0f } }  // BR
    };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    m_vertexCount = (UINT)vertices.size();

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);
    
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)));

    UINT8* pVertexDataBegin;
    m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pVertexDataBegin));
    memcpy(pVertexDataBegin, vertices.data(), vbByteSize);
    m_vertexBuffer->Unmap(0, nullptr);

    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vbByteSize;
}

void TessellationTerrainApp::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.HS = { reinterpret_cast<BYTE*>(m_hsByteCode->GetBufferPointer()), m_hsByteCode->GetBufferSize() };
    psoDesc.DS = { reinterpret_cast<BYTE*>(m_dsByteCode->GetBufferPointer()), m_dsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    // Solid fill mode to see the texture!
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; 
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH; 
    
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    // Enable Depth Stencil
    psoDesc.DSVFormat = m_depthStencilFormat;
    
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
}

void TessellationTerrainApp::OnUpdate() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(currentTime - m_lastTime).count();
    m_lastTime = currentTime;

    OnKeyboardInput(dt);

    XMMATRIX view = m_camera.GetView();
    XMMATRIX proj = m_camera.GetProj();
    XMMATRIX world = XMMatrixIdentity();
    
    // Update Camera View Matrix
    m_camera.UpdateViewMatrix();
    view = m_camera.GetView();

    XMMATRIX mvp = world * view * proj;
    XMStoreFloat4x4(&m_worldViewProj, XMMatrixTranspose(mvp));
}

void TessellationTerrainApp::OnKeyboardInput(float dt) {
    float speed = 20.0f; // Units per second

    if(GetAsyncKeyState('W') & 0x8000)
        m_camera.Walk(speed * dt);
    if(GetAsyncKeyState('S') & 0x8000)
        m_camera.Walk(-speed * dt);
    
    if(GetAsyncKeyState('A') & 0x8000)
        m_camera.Strafe(-speed * dt);
    if(GetAsyncKeyState('D') & 0x8000)
        m_camera.Strafe(speed * dt);

    if(GetAsyncKeyState('E') & 0x8000) {
        XMFLOAT3 pos = m_camera.GetPosition3f();
        pos.y += speed * dt;
        m_camera.SetPosition(pos);
    }
    if(GetAsyncKeyState('Q') & 0x8000) {
        XMFLOAT3 pos = m_camera.GetPosition3f();
        pos.y -= speed * dt;
        m_camera.SetPosition(pos);
    }

    m_camera.UpdateViewMatrix();
}

void TessellationTerrainApp::OnRender() {
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pso.Get()));

    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrier);

    auto rtv = CurrentBackBufferView();
    auto dsv = DepthStencilView();
    m_commandList->ClearRenderTargetView(rtv, Colors::LightGray, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    m_commandList->OMSetRenderTargets(1, &rtv, true, &dsv);

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, descriptorHeaps);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->SetGraphicsRoot32BitConstants(0, 16, &m_worldViewProj, 0);
    // Bind Texture Table (Param 1)
    m_commandList->SetGraphicsRootDescriptorTable(1, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

    m_commandList->DrawInstanced(m_vertexCount, 1, 0, 0);

    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrier2);

    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* cmds[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmds);

    ThrowIfFailed(m_swapChain->Present(1, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;
    FlushCommandQueue();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
    TessellationTerrainApp theApp(hInstance);
    if (!theApp.Initialize())
        return 0;
    return theApp.Run();
}
