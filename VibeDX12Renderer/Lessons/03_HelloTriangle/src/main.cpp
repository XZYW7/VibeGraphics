#include <VibeDX12App.h>
#include <iostream>
#include <vector>

using namespace Vibe;

struct Vertex {
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};

class HelloTriangleApp : public VibeDX12App {
public:
    HelloTriangleApp(HINSTANCE hInstance) : VibeDX12App(hInstance) {
        m_MainWndCaption = L"Lesson 03: Hello Triangle";
    }

    virtual bool Initialize() override;
    virtual void OnUpdate() override {}
    virtual void OnRender() override;

private:
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildGeometry();
    void BuildPSO();

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap; // (预留，本课可能不用)
    
    ComPtr<ID3DBlob> m_vsByteCode;
    ComPtr<ID3DBlob> m_psByteCode;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    ComPtr<ID3D12PipelineState> m_pso;

    // 几何体数据
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
};

bool HelloTriangleApp::Initialize() {
    if (!VibeDX12App::Initialize()) return false;

    // 初始化渲染管线必要的资源
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildGeometry(); // 上传三角形顶点
    BuildPSO();      // 编译管线状态

    ThrowIfFailed(m_commandList->Close());
    
    // 执行初始化命令（例如顶点上传）
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // 等待初始化完成
    FlushCommandQueue();

    return true;
}

void HelloTriangleApp::BuildRootSignature() {
    // 简单的空 Root Signature
    // Root Signature 定义了 Shader 需要什么参数（常量、纹理、采样器等）
    // 即便不需要参数，也必须有一个空的 Root Signature
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
    rootSigDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);
    if(errorBlob != nullptr) {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(m_d3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)));
}

void HelloTriangleApp::BuildShadersAndInputLayout() {
    // 编译 Shader (运行时编译)
    // 实际项目中通常预编译为 .cso 文件
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

    // 定义输入布局 (对应 HLSL 中的 struct Vertex)
    m_inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 } // 偏移 12 字节 (3 float)
    };
}

void HelloTriangleApp::BuildGeometry() {
    // 定义一个彩色三角形
    std::vector<Vertex> vertices = {
        { { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } }, // 上 (红)
        { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } }, // 右下 (绿)
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }  // 左下 (蓝)
    };

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    // 创建顶点缓冲区资源 (Upload Heap - CPU 可写，GPU 可读)
    // 注意：对于静态几何体，最佳实践是创建 Default Heap 并通过 Upload Heap 复制过去。
    // 这里为了简化 Lesson 03，直接使用 Upload Heap。
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)));

    // 将顶点数据复制到 Buffer
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0); // 我们不打算从 CPU 读取它
    ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, vertices.data(), vbByteSize);
    m_vertexBuffer->Unmap(0, nullptr);

    // 初始化 Vertex Buffer View
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vbByteSize;
}

void HelloTriangleApp::BuildPSO() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    
    psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE; // 暂时关闭深度测试
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
}

void HelloTriangleApp::OnRender() {
    // 1. 命令记录准备
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pso.Get()));

    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // 2. 资源状态切换 (Present -> RenderTarget)
    auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrierToRT);

    // 3. 设置渲染目标并清除
    auto rtvHandle = CurrentBackBufferView();
    m_commandList->OMSetRenderTargets(1, &rtvHandle, true, nullptr);

    const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f }; // 深灰色
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // 4. 设置图形管线状态
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    // 5. 绘制！
    m_commandList->DrawInstanced(3, 1, 0, 0);

    // 6. 资源状态切换 (RenderTarget -> Present)
    auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrierToPresent);

    // 7. 提交
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // 8. 呈现
    ThrowIfFailed(m_swapChain->Present(1, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

    // 9. 等待 (简单同步)
    FlushCommandQueue();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev, PSTR cmd, int show) {
    try {
        HelloTriangleApp theApp(hInstance);
        if(!theApp.Initialize()) return 0;
        return theApp.Run();
    }
    catch(std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK);
        return 0;
    }
}
