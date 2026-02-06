#include <VibeDX12App.h>
#include <VibeCamera.h>
#include <DirectXColors.h>
#include <windowsx.h>
#include <vector>

using namespace Vibe;
using namespace DirectX;

struct Vertex {
    XMFLOAT3 Pos;
    XMFLOAT2 Size; // World size of billboard
};

struct ObjectConstants {
    XMFLOAT4X4 ViewProj;
    XMFLOAT3 EyePosW;
    float Pad;
};

class GeometryShaderApp : public VibeDX12App {
public:
    GeometryShaderApp(HINSTANCE hInstance);
    virtual bool Initialize() override;
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override; 

protected:
    virtual void OnUpdate() override;
    virtual void OnRender() override;
    
    void OnKeyboardInput();
    void OnMouseMove(WPARAM btnState, int x, int y);
    void OnMouseDown(WPARAM btnState, int x, int y);
    void OnMouseUp(WPARAM btnState, int x, int y);  

private:
    float AspectRatio() const { return static_cast<float>(m_ClientWidth)/m_ClientHeight; }
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildTreeGeometry(); // We'll render trees!
    void BuildPSO();

    VibeCamera m_camera;

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3DBlob> m_vsByteCode;
    ComPtr<ID3DBlob> m_gsByteCode;
    ComPtr<ID3DBlob> m_psByteCode;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
    
    ComPtr<ID3D12PipelineState> m_pso;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_vertexBufferUpload;
    D3D12_VERTEX_BUFFER_VIEW m_vbView;
    UINT m_treeCount = 0;

    ObjectConstants m_objConstants;
    POINT mLastMousePos;
};

GeometryShaderApp::GeometryShaderApp(HINSTANCE hInstance) : VibeDX12App(hInstance) {
    m_MainWndCaption = L"Lesson 10: Geometry Shader (Billboards)";
    m_camera.SetPosition(0.0f, 2.0f, -10.0f);
}

bool GeometryShaderApp::Initialize() {
    if(!VibeDX12App::Initialize()) return false;
    
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    m_camera.SetLens(0.25f * XM_PI, (float)m_ClientWidth/m_ClientHeight, 1.0f, 1000.0f);

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildTreeGeometry();
    BuildPSO();

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmdLists);
    FlushCommandQueue();

    return true;
}

LRESULT GeometryShaderApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_SIZE:
            m_ClientWidth = LOWORD(lParam);
            m_ClientHeight = HIWORD(lParam);
            if(m_d3dDevice) {
                 // For this tutorial, we won't fully handle buffer resizing to keep it simple, 
                 // but we will update camera aspect ratio.
                 if(m_ClientHeight > 0)
                    m_camera.SetLens(0.25f * XM_PI, AspectRatio(), 1.0f, 1000.0f);
            }
            return 0;
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_MOUSEMOVE:
            OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
    }
    return VibeDX12App::MsgProc(hwnd, msg, wParam, lParam);
}

void GeometryShaderApp::BuildRootSignature() {
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];
    slotRootParameter[0].InitAsConstants(sizeof(ObjectConstants)/4, 0); 

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);
    ThrowIfFailed(hr);
    ThrowIfFailed(m_d3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

void GeometryShaderApp::BuildShadersAndInputLayout() {
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    ComPtr<ID3DBlob> errors;
    HRESULT hr;
    
    // VS
    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0, &m_vsByteCode, &errors);
    if(FAILED(hr)) { OutputDebugStringA((char*)errors->GetBufferPointer()); throw std::runtime_error("VS Compile Failed"); }

    // GS - This is new!
    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "GS", "gs_5_0", compileFlags, 0, &m_gsByteCode, &errors);
    if(FAILED(hr)) { OutputDebugStringA((char*)errors->GetBufferPointer()); throw std::runtime_error("GS Compile Failed"); }

    // PS
    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0, &m_psByteCode, &errors);
    if(FAILED(hr)) { OutputDebugStringA((char*)errors->GetBufferPointer()); throw std::runtime_error("PS Compile Failed"); }

    m_inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "SIZE",     0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void GeometryShaderApp::BuildTreeGeometry() {
    std::vector<Vertex> vertices;
    // Create random points
    for(int i=-5; i<=5; ++i) {
        for(int j=-5; j<=5; ++j) {
            Vertex v;
            v.Pos = XMFLOAT3(i*2.0f, 0.0f, j*2.0f);
            v.Size = XMFLOAT2(1.5f, 1.5f);
            vertices.push_back(v);
        }
    }
    m_treeCount = (UINT)vertices.size();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    ThrowIfFailed(CreateDefaultBuffer(m_d3dDevice.Get(), m_commandList.Get(), vertices.data(), vbByteSize, m_vertexBufferUpload, m_vertexBuffer));
    
    m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbView.StrideInBytes = sizeof(Vertex);
    m_vbView.SizeInBytes = vbByteSize;
}

void GeometryShaderApp::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.GS = { reinterpret_cast<BYTE*>(m_gsByteCode->GetBufferPointer()), m_gsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // Billboards are 2D
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; // IMPORTANT: Input is POINTS
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.DSVFormat = m_depthStencilFormat;

    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
}

void GeometryShaderApp::OnUpdate() {
    OnKeyboardInput();
    m_camera.UpdateViewMatrix();
    
    XMMATRIX view = m_camera.GetView();
    XMMATRIX proj = m_camera.GetProj();
    XMMATRIX viewProj = view * proj;
    
    XMStoreFloat4x4(&m_objConstants.ViewProj, XMMatrixTranspose(viewProj));
    m_objConstants.EyePosW = m_camera.GetPosition3f();
}

void GeometryShaderApp::OnRender() {
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pso.Get()));

    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrier);

    auto rtv = CurrentBackBufferView();
    auto dsv = DepthStencilView();
    m_commandList->ClearRenderTargetView(rtv, Colors::CornflowerBlue, 0, nullptr); // Blue background to see green trees
    m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    m_commandList->OMSetRenderTargets(1, &rtv, true, &dsv);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->SetGraphicsRoot32BitConstants(0, sizeof(ObjectConstants)/4, &m_objConstants, 0);

    m_commandList->IASetVertexBuffers(0, 1, &m_vbView);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST); // Input Points

    m_commandList->DrawInstanced(m_treeCount, 1, 0, 0);

    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrier2);

    ThrowIfFailed(m_commandList->Close());
    
    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmdLists);
    
    ThrowIfFailed(m_swapChain->Present(0, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;
    FlushCommandQueue();
}

void GeometryShaderApp::OnKeyboardInput() {
    float dt = 0.01f;
    if(GetAsyncKeyState('W') & 0x8000) m_camera.Walk(10.0f*dt);
    if(GetAsyncKeyState('S') & 0x8000) m_camera.Walk(-10.0f*dt);
    if(GetAsyncKeyState('A') & 0x8000) m_camera.Strafe(-10.0f*dt);
    if(GetAsyncKeyState('D') & 0x8000) m_camera.Strafe(10.0f*dt);
}

void GeometryShaderApp::OnMouseDown(WPARAM btnState, int x, int y) {
    mLastMousePos.x = x; mLastMousePos.y = y;
    SetCapture(m_hMainWnd);
}
void GeometryShaderApp::OnMouseUp(WPARAM btnState, int x, int y) {
    ReleaseCapture();
}
void GeometryShaderApp::OnMouseMove(WPARAM btnState, int x, int y) {
    if((btnState & MK_LBUTTON) != 0) {
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));
        m_camera.Pitch(dy);
        m_camera.RotateY(dx);
    }
    mLastMousePos.x = x; mLastMousePos.y = y;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
    GeometryShaderApp theApp(hInstance);
    if(!theApp.Initialize()) return 0;
    return theApp.Run();
}
