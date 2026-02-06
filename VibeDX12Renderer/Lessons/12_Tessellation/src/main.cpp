#include <VibeDX12App.h>
#include <VibeCamera.h>
#include <vector>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <filesystem>
#include <chrono>

using namespace Vibe;
using namespace DirectX;

struct Vertex {
    XMFLOAT3 Pos;
};

class TessellationApp : public VibeDX12App {
public:
    TessellationApp(HINSTANCE hInstance);
    virtual bool Initialize() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildGeometry();
    void BuildPSO();
    void OnKeyboardInput(float dt);

    ComPtr<ID3D12RootSignature> m_rootSignature;
    
    // Camera
    VibeCamera m_camera;
    POINT m_lastMousePos;
    std::chrono::high_resolution_clock::time_point m_lastTime;

    ComPtr<ID3DBlob> m_vsByteCode;
    ComPtr<ID3DBlob> m_hsByteCode;
    ComPtr<ID3DBlob> m_dsByteCode;
    ComPtr<ID3DBlob> m_psByteCode;
    
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
    ComPtr<ID3D12PipelineState> m_pso;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    UINT m_vertexCount = 0;

    XMFLOAT4X4 m_worldViewProj;
};

TessellationApp::TessellationApp(HINSTANCE hInstance) : VibeDX12App(hInstance) {
    m_MainWndCaption = L"Lesson 12: Tessellation (WASD+QE Move)";
    m_camera.SetPosition(0.0f, 15.0f, -25.0f);
    m_camera.Pitch(XMConvertToRadians(20.0f));
}

bool TessellationApp::Initialize() {
    if (!VibeDX12App::Initialize()) return false;
    
    // Init Camera Lens
    float aspect = static_cast<float>(m_ClientWidth) / m_ClientHeight;
    m_camera.SetLens(0.25f * XM_PI, aspect, 1.0f, 1000.0f);
    m_lastTime = std::chrono::high_resolution_clock::now();

    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildGeometry();
    BuildPSO();

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmdsLists);
    FlushCommandQueue();

    return true;
}

void TessellationApp::BuildRootSignature() {
    // CD3DX12_ROOT_PARAMETER slotRootParameter[1];
    // slotRootParameter[0].InitAsConstants(16, 0); 
    D3D12_ROOT_PARAMETER slotRootParameter[1];
    slotRootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    slotRootParameter[0].Constants.ShaderRegister = 0;
    slotRootParameter[0].Constants.RegisterSpace = 0;
    slotRootParameter[0].Constants.Num32BitValues = 16;
    slotRootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 1;
    rootSigDesc.pParameters = slotRootParameter;
    rootSigDesc.NumStaticSamplers = 0;
    rootSigDesc.pStaticSamplers = nullptr;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob));
    ThrowIfFailed(m_d3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

void TessellationApp::BuildShadersAndInputLayout() {
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
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void TessellationApp::BuildGeometry() {
    std::vector<Vertex> vertices = {
        { { -10.0f, 0.0f,  10.0f } }, // TL
        { {  10.0f, 0.0f,  10.0f } }, // TR
        { { -10.0f, 0.0f, -10.0f } }, // BL
        { {  10.0f, 0.0f, -10.0f } }  // BR
    };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    m_vertexCount = (UINT)vertices.size();

    // CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Alignment = 0;
    bufferDesc.Width = vbByteSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.SampleDesc.Quality = 0;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
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

void TessellationApp::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.HS = { reinterpret_cast<BYTE*>(m_hsByteCode->GetBufferPointer()), m_hsByteCode->GetBufferSize() };
    psoDesc.DS = { reinterpret_cast<BYTE*>(m_dsByteCode->GetBufferPointer()), m_dsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    
    // Rasterizer Defaults
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // Blend Defaults
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL
    };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        psoDesc.BlendState.RenderTarget[i] = defaultRenderTargetBlendDesc;

    // Depth Stencil Defaults
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    psoDesc.DepthStencilState.FrontFace = defaultStencilOp;
    psoDesc.DepthStencilState.BackFace = defaultStencilOp;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH; 
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
}

void TessellationApp::OnUpdate() {
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

void TessellationApp::OnKeyboardInput(float dt) {
    float speed = 20.0f; // Units per second

    if(GetAsyncKeyState('W') & 0x8000)
        m_camera.Walk(speed * dt);
    if(GetAsyncKeyState('S') & 0x8000)
        m_camera.Walk(-speed * dt);
    
    if(GetAsyncKeyState('A') & 0x8000)
        m_camera.Strafe(-speed * dt);
    if(GetAsyncKeyState('D') & 0x8000)
        m_camera.Strafe(speed * dt);

    // E for Up, Q for Down (User Request)
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

void TessellationApp::OnRender() {
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pso.Get()));

    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Barrier Present -> RT
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = CurrentBackBuffer();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    auto rtv = CurrentBackBufferView();
    m_commandList->ClearRenderTargetView(rtv, Colors::LightGray, 0, nullptr);
    m_commandList->OMSetRenderTargets(1, &rtv, true, nullptr);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->SetGraphicsRoot32BitConstants(0, 16, &m_worldViewProj, 0);

    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

    m_commandList->DrawInstanced(m_vertexCount, 1, 0, 0);

    // Barrier RT -> Present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);

    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* cmds[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmds);

    ThrowIfFailed(m_swapChain->Present(1, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;
    FlushCommandQueue();
}

LRESULT TessellationApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
    TessellationApp theApp(hInstance);
    if (!theApp.Initialize())
        return 0;
    return theApp.Run();
}
