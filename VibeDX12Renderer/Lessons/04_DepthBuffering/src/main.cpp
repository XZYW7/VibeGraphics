#include <VibeDX12App.h>
#include <iostream>
#include <vector>
#include <DirectXMath.h>

using namespace Vibe;
using namespace DirectX;

struct Vertex {
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

class DepthBufferingApp : public VibeDX12App {
public:
    DepthBufferingApp(HINSTANCE hInstance) : VibeDX12App(hInstance) {
        m_MainWndCaption = L"Lesson 04: Depth Buffering & Index Buffer";
    }

    virtual bool Initialize() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;

private:
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildGeometry();
    void BuildPSO();

    ComPtr<ID3D12RootSignature> m_rootSignature;
    
    ComPtr<ID3DBlob> m_vsByteCode;
    ComPtr<ID3DBlob> m_psByteCode;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    ComPtr<ID3D12PipelineState> m_pso;

    // 几何体数据
    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    UINT m_indexCount = 0;

    // 变换矩阵
    XMFLOAT4X4 m_worldViewProj = MathHelper::Identity4x4();
    float m_theta = 0.0f;
    float m_phi = XM_PIDIV4;
    float m_radius = 5.0f;

    // 简单的 MathHelper
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
};

bool DepthBufferingApp::Initialize() {
    if (!VibeDX12App::Initialize()) return false;

    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildGeometry();
    BuildPSO();

    ThrowIfFailed(m_commandList->Close());
    
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();

    return true;
}

void DepthBufferingApp::BuildRootSignature() {
    // 创建一个 Root Parameter，类型为 32位常量 (Root Constants)
    // 这样我们就不用创建 Descriptor Heap 也能传矩阵了
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];
    
    // 参数 0: 16个 float (4x4 矩阵), 对应 shader register(b0)
    slotRootParameter[0].InitAsConstants(16, 0); 

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

void DepthBufferingApp::BuildShadersAndInputLayout() {
    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> errors;
    // 使用宽字符路径
    HRESULT hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0, &m_vsByteCode, &errors);
    if(errors != nullptr) OutputDebugStringA((char*)errors->GetBufferPointer());
    ThrowIfFailed(hr);

    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0, &m_psByteCode, &errors);
    if(errors != nullptr) OutputDebugStringA((char*)errors->GetBufferPointer());
    ThrowIfFailed(hr);

    m_inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void DepthBufferingApp::BuildGeometry() {
    // 构造一个四棱锥 (Pyramid)
    std::vector<Vertex> vertices = {
        { { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },  // 0. 顶点 (红)
        { { -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } }, // 1. 左后 (绿)
        { { -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },  // 2. 左前 (蓝)
        { { 1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } },   // 3. 右前 (黄)
        { { 1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f } }   // 4. 右后 (青)
    };

    // 索引数据 (Index Buffer)
    // 使用 16位索引 (UINT16)
    std::vector<uint16_t> indices = {
        // 前面
        0, 2, 3,
        // 右面
        0, 3, 4,
        // 后面
        0, 4, 1,
        // 左面
        0, 1, 2,
        // 底面 (由两个三角形组成)
        1, 4, 3,
        1, 3, 2
    };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);
    m_indexCount = (UINT)indices.size();

    // 创建 Vertex Buffer
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto vBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &vBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)));

    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0); 
    ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, vertices.data(), vbByteSize);
    m_vertexBuffer->Unmap(0, nullptr);

    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vbByteSize;

    // 创建 Index Buffer
    auto iBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapProps, // 复用 upload heap props
        D3D12_HEAP_FLAG_NONE,
        &iBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_indexBuffer)));

    UINT8* pIndexDataBegin;
    ThrowIfFailed(m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
    memcpy(pIndexDataBegin, indices.data(), ibByteSize);
    m_indexBuffer->Unmap(0, nullptr);

    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT; // 注意是 16位
    m_indexBufferView.SizeInBytes = ibByteSize;
}

void DepthBufferingApp::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    
    psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    
    // !! 开启深度测试 !!
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    // !! 配置 DSV 格式 !!
    psoDesc.DSVFormat = m_depthStencilFormat;

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
}

void DepthBufferingApp::OnUpdate() {
    // 简单的旋转动画
    static float t = 0.0f;
    t += 0.005f;

    // 构建 View 矩阵 (摄像机)
    XMVECTOR pos = XMVectorSet(0.0f, 2.0f, -3.0f, 1.0f); // 摄像机位置
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);

    // 构建 Projection 矩阵 (透视投影)
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, (float)m_ClientWidth / m_ClientHeight, 1.0f, 1000.0f);

    // 构建 World 矩阵 (物体本身旋转)
    XMMATRIX world = XMMatrixRotationY(t);

    // 组合 MVP 矩阵
    XMMATRIX worldViewProj = world * view * proj;

    // 转置矩阵 (HLSL 是列主序，DirectXMath 是行主序)
    XMStoreFloat4x4(&m_worldViewProj, XMMatrixTranspose(worldViewProj));
}

void DepthBufferingApp::OnRender() {
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pso.Get()));

    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Transition SwapChain to Render Target
    auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrierToRT);

    // 清除 RTV 和 DSV
    auto rtvHandle = CurrentBackBufferView();
    auto dsvHandle = DepthStencilView();
    
    // !! 注意这里要传入 DSV 的句柄 !!
    m_commandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

    const float clearColor[] = { 0.6f, 0.7f, 0.9f, 1.0f }; // 浅蓝色背景
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    
    // !! 清除深度缓冲区 !!
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    
    // !! 设置 Index Buffer !!
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    // !! 传递 Root Constants (矩阵) !!
    m_commandList->SetGraphicsRoot32BitConstants(0, 16, &m_worldViewProj, 0);

    // 绘制 (DrawIndexed)
    m_commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrierToPresent);

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(m_swapChain->Present(1, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

    FlushCommandQueue();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev, PSTR cmd, int show) {
    try {
        DepthBufferingApp theApp(hInstance);
        if(!theApp.Initialize()) return 0;
        return theApp.Run();
    }
    catch(std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK);
        return 0;
    }
}
