#include <VibeDX12App.h>
#include <VibeCamera.h>
#include <iostream>
#include <vector>
#include <DirectXMath.h>

using namespace Vibe;
using namespace DirectX;

struct Vertex {
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
};

struct ObjectConstants {
    XMFLOAT4X4 World;
    XMFLOAT4X4 ViewProj;
    XMFLOAT3 EyePosW;
    float Pad0;
    XMFLOAT3 LightDir;
    float Pad1;
    XMFLOAT4 LightColor;
    XMFLOAT4 AmbientColor;
};

class LightingApp : public VibeDX12App {
public:
    LightingApp(HINSTANCE hInstance) : VibeDX12App(hInstance) {
        m_MainWndCaption = L"Lesson 07: Basic Lighting (Phong)";
        m_camera.SetPosition(0.0f, 2.0f, -5.0f);
    }

    virtual bool Initialize() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildGeometry(); // Generate Sphere
    void BuildPSO();
    
    void OnKeyboardInput();
    void OnMouseMove(WPARAM btnState, int x, int y);

    ComPtr<ID3D12RootSignature> m_rootSignature;
    
    ComPtr<ID3DBlob> m_vsByteCode;
    ComPtr<ID3DBlob> m_psByteCode;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    ComPtr<ID3D12PipelineState> m_pso;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    UINT m_indexCount = 0;

    VibeCamera m_camera;
    ObjectConstants m_constants;
    
    POINT mLastMousePos;
};

bool LightingApp::Initialize() {
    if (!VibeDX12App::Initialize()) return false;

    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    m_camera.SetLens(0.25f * XM_PI, static_cast<float>(m_ClientWidth) / m_ClientHeight, 1.0f, 1000.0f);

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildGeometry();
    BuildPSO();

    ThrowIfFailed(m_commandList->Close());
    
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    
    FlushCommandQueue();

    return true;
}LRESULT LightingApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
    }
    return VibeDX12App::MsgProc(hwnd, msg, wParam, lParam);
}

void LightingApp::OnKeyboardInput() {
    float dt = 0.016f; 
    float speed = 5.0f; 

    if (GetAsyncKeyState('W') & 0x8000) m_camera.Walk(speed * dt);
    if (GetAsyncKeyState('S') & 0x8000) m_camera.Walk(-speed * dt);
    if (GetAsyncKeyState('A') & 0x8000) m_camera.Strafe(-speed * dt);
    if (GetAsyncKeyState('D') & 0x8000) m_camera.Strafe(speed * dt);
}

void LightingApp::OnMouseMove(WPARAM btnState, int x, int y) {
    if ((btnState & MK_LBUTTON) != 0) {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
        m_camera.Pitch(dy);
        m_camera.RotateY(dx);
    }
    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void LightingApp::OnUpdate() {
    OnKeyboardInput();
    m_camera.UpdateViewMatrix();

    // Setup Constants
    XMMATRIX world = XMMatrixIdentity();
    XMStoreFloat4x4(&m_constants.World, XMMatrixTranspose(world));
    
    XMMATRIX viewProj = m_camera.GetViewProj();
    XMStoreFloat4x4(&m_constants.ViewProj, XMMatrixTranspose(viewProj));
    
    m_constants.EyePosW = m_camera.GetPosition3f();
    
    // Light Params
    m_constants.LightDir = { 0.57735f, -0.57735f, 0.57735f }; // Diagonal
    m_constants.LightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_constants.AmbientColor = { 0.1f, 0.1f, 0.1f, 1.0f };
}

void LightingApp::BuildRootSignature() {
    // Just one root parameter for the massive constant buffer
    // Note: 48 floats = 48 32-bit values.
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];
    slotRootParameter[0].InitAsConstants(48, 0); // b0

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob));

    ThrowIfFailed(m_d3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)));
}

void LightingApp::BuildShadersAndInputLayout() {
    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0, &m_vsByteCode, &errors);
    if(errors != nullptr) {
        OutputDebugStringA((char*)errors->GetBufferPointer());
        MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "VS Compile Error", MB_OK);
    }
    ThrowIfFailed(hr);

    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0, &m_psByteCode, &errors);
    if(errors != nullptr) {
        OutputDebugStringA((char*)errors->GetBufferPointer());
        MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "PS Compile Error", MB_OK);
    }
    ThrowIfFailed(hr);

    m_inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void LightingApp::BuildGeometry() {
    // Generate Sphere Mesh
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    float radius = 1.0f;
    UINT stackCount = 20;
    UINT sliceCount = 20;

    // TopVertex
    Vertex topVertex;
    topVertex.Pos = { 0.0f, radius, 0.0f };
    topVertex.Normal = { 0.0f, 1.0f, 0.0f };
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

            vertices.push_back(v);
        }
    }

    // BottomVertex
    Vertex bottomVertex;
    bottomVertex.Pos = { 0.0f, -radius, 0.0f };
    bottomVertex.Normal = { 0.0f, -1.0f, 0.0f };
    vertices.push_back(bottomVertex);

    UINT ringVertexCount = sliceCount + 1;

    // Top Ring
    for(UINT i = 0; i < sliceCount; ++i) {
        indices.push_back(0);
        indices.push_back(i + 2);
        indices.push_back(i + 1);
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
        indices.push_back(southPoleIndex);
        indices.push_back(baseIndex + i);
        indices.push_back(baseIndex + i + 1);
    }

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

void LightingApp::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    
    // Cull back faces
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    // Wireframe for debugging?
    // psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

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

void LightingApp::OnRender() {
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
    
    // Push Constants
    m_commandList->SetGraphicsRoot32BitConstants(0, 48, &m_constants, 0);

    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);
    
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
    LightingApp app(hInstance);
    if(!app.Initialize()) return 0;
    return app.Run();
}
