#include <VibeDX12App.h>
#include <VibeCamera.h>
#include <iostream>
#include <vector>
#include <DirectXMath.h>
#include <DirectXColors.h>

using namespace Vibe;
using namespace DirectX;

static XMFLOAT4X4 MatrixIdentity4x4() {
    XMFLOAT4X4 I;
    XMStoreFloat4x4(&I, XMMatrixIdentity());
    return I;
}

enum class RenderLayer : int {
    Opaque = 0,
    Mirrors,        // Mark stencil
    Reflected,      // Draw reflected objects
    Transparent,    // Draw mirror surface
    Count
};

struct RenderItem {
    XMFLOAT4X4 World = MatrixIdentity4x4();
    XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    XMFLOAT3 FresnelR0 = { 0.1f, 0.1f, 0.1f };
    float Roughness = 0.5f; // Shininess inverted

    // Mesh Info
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    INT BaseVertexLocation = 0;
};

// Constant Buffer structure matching shaders.hlsl
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

struct Vertex {
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
};

class StencilingApp : public VibeDX12App {
public:
    StencilingApp(HINSTANCE hInstance);
    virtual bool Initialize() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

private:
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
    void BuildRenderItems();
    void BuildPSOs();

    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
    
    void CreateBox(float width, float height, float depth, std::vector<Vertex>& vertices, std::vector<uint16_t>& indices);
    void CreateGrid(float width, float depth, uint32_t m, uint32_t n, std::vector<Vertex>& vertices, std::vector<uint16_t>& indices);
    void CreateSphere(float radius, uint32_t sliceCount, uint32_t stackCount, std::vector<Vertex>& vertices, std::vector<uint16_t>& indices);

    void OnKeyboardInput();
    void OnMouseMove(WPARAM btnState, int x, int y);

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3DBlob> m_vsByteCode;
    ComPtr<ID3DBlob> m_psByteCode;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

    // PSOs
    ComPtr<ID3D12PipelineState> m_psoOpaque;
    ComPtr<ID3D12PipelineState> m_psoMarkMirrors;
    ComPtr<ID3D12PipelineState> m_psoDrawReflections;
    ComPtr<ID3D12PipelineState> m_psoTransparent;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    ComPtr<ID3D12Resource> m_vertexBufferUpload;
    ComPtr<ID3D12Resource> m_indexBufferUpload;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    // Render items divided by layer
    std::vector<std::unique_ptr<RenderItem>> m_allRitems;
    std::vector<RenderItem*> m_ritemLayer[(int)RenderLayer::Count];

    VibeCamera m_camera;
    ObjectConstants m_constants;
    POINT mLastMousePos;

    // Cache geometry offsets
    struct SubmeshGeometry {
        UINT IndexCount = 0;
        UINT StartIndexLocation = 0;
        INT BaseVertexLocation = 0;
    };
    SubmeshGeometry m_mirrorGeo;
    SubmeshGeometry m_gridGeo;
    SubmeshGeometry m_sphereGeo;
};

StencilingApp::StencilingApp(HINSTANCE hInstance) : VibeDX12App(hInstance) {
    m_MainWndCaption = L"Lesson 09: Stenciling (Mirrors)";
    m_camera.SetPosition(0.0f, 3.0f, -6.0f);
    m_camera.Pitch(XMConvertToRadians(20.0f));
}

bool StencilingApp::Initialize() {
    if (!VibeDX12App::Initialize()) return false;
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));
    
    m_camera.SetLens(0.25f * XM_PI, static_cast<float>(m_ClientWidth) / m_ClientHeight, 1.0f, 1000.0f);

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
    BuildRenderItems();
    BuildPSOs();

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();

    return true;
}

void StencilingApp::BuildRootSignature() {
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];
    slotRootParameter[0].InitAsConstants(sizeof(ObjectConstants)/4, 0); 
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);
    if(errorBlob != nullptr) {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);
    ThrowIfFailed(m_d3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

void StencilingApp::BuildShadersAndInputLayout() {
    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VS", "vs_5_0", compileFlags, 0, &m_vsByteCode, &errors);
    if(errors) OutputDebugStringA((char*)errors->GetBufferPointer());
    if(FAILED(hr)) {
        std::string err = "VS Compile Error: ";
        if(errors) err += (char*)errors->GetBufferPointer();
        else err += "Unknown (File not found?)";
        throw std::runtime_error(err);
    }

    hr = D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PS", "ps_5_0", compileFlags, 0, &m_psByteCode, &errors);
    if(errors) OutputDebugStringA((char*)errors->GetBufferPointer());
    if(FAILED(hr)) {
        std::string err = "PS Compile Error: ";
        if(errors) err += (char*)errors->GetBufferPointer();
        else err += "Unknown (File not found?)";
        throw std::runtime_error(err);
    }

    m_inputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void StencilingApp::CreateSphere(float radius, uint32_t sliceCount, uint32_t stackCount, std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {
    // Generate Vertex Stack
    Vertex topVertex; topVertex.Pos = { 0.0f, radius, 0.0f }; topVertex.Normal = { 0.0f, 1.0f, 0.0f };
    vertices.push_back(topVertex);
    float phiStep = XM_PI / stackCount; float thetaStep = 2.0f * XM_PI / sliceCount;
    for(uint32_t i = 1; i <= stackCount - 1; ++i) {
        float phi = i * phiStep;
        for(uint32_t j = 0; j <= sliceCount; ++j) {
            float theta = j * thetaStep;
            Vertex v;
            v.Pos = { radius * sinf(phi) * cosf(theta), radius * cosf(phi), radius * sinf(phi) * sinf(theta) };
            XMVECTOR p = XMLoadFloat3(&v.Pos);
            XMStoreFloat3(&v.Normal, XMVector3Normalize(p));
            vertices.push_back(v);
        }
    }
    Vertex bottomVertex; bottomVertex.Pos = { 0.0f, -radius, 0.0f }; bottomVertex.Normal = { 0.0f, -1.0f, 0.0f };
    vertices.push_back(bottomVertex);
    // Indices
    for(uint32_t i = 0; i < sliceCount; ++i) { indices.push_back(0); indices.push_back(i+2); indices.push_back(i+1); }
    uint32_t baseIndex = 1; uint32_t ringVertexCount = sliceCount + 1;
    for(uint32_t i = 0; i < stackCount - 2; ++i) {
        for(uint32_t j = 0; j < sliceCount; ++j) {
            indices.push_back(baseIndex + i*ringVertexCount + j);
            indices.push_back(baseIndex + i*ringVertexCount + j + 1);
            indices.push_back(baseIndex + (i+1)*ringVertexCount + j);
            indices.push_back(baseIndex + (i+1)*ringVertexCount + j);
            indices.push_back(baseIndex + i*ringVertexCount + j + 1);
            indices.push_back(baseIndex + (i+1)*ringVertexCount + j + 1);
        }
    }
    uint32_t southPoleIndex = (uint32_t)vertices.size() - 1; baseIndex = southPoleIndex - ringVertexCount;
    for(uint32_t i = 0; i < sliceCount; ++i) { indices.push_back(southPoleIndex); indices.push_back(baseIndex + i); indices.push_back(baseIndex + i + 1); }
}

void StencilingApp::CreateBox(float width, float height, float depth, std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {
    float w2 = 0.5f * width;
    float h2 = 0.5f * height;
    float d2 = 0.5f * depth;

    // Create 8 Vertices
    Vertex v[24];

    // Front Face (Normal -Z) - Mirror Logic reused here for one face essentially
    v[0].Pos = { -w2, -h2, -d2 }; v[0].Normal = { 0.0f, 0.0f, -1.0f };
    v[1].Pos = { -w2, +h2, -d2 }; v[1].Normal = { 0.0f, 0.0f, -1.0f };
    v[2].Pos = { +w2, +h2, -d2 }; v[2].Normal = { 0.0f, 0.0f, -1.0f };
    v[3].Pos = { +w2, -h2, -d2 }; v[3].Normal = { 0.0f, 0.0f, -1.0f };
    
    // Back Face (Normal +Z)
    v[4].Pos = { -w2, -h2, +d2 }; v[4].Normal = { 0.0f, 0.0f, 1.0f };
    v[5].Pos = { +w2, -h2, +d2 }; v[5].Normal = { 0.0f, 0.0f, 1.0f };
    v[6].Pos = { +w2, +h2, +d2 }; v[6].Normal = { 0.0f, 0.0f, 1.0f };
    v[7].Pos = { -w2, +h2, +d2 }; v[7].Normal = { 0.0f, 0.0f, 1.0f };

    // Top Face (Normal +Y)
    v[8].Pos = { -w2, +h2, -d2 }; v[8].Normal = { 0.0f, 1.0f, 0.0f };
    v[9].Pos = { -w2, +h2, +d2 }; v[9].Normal = { 0.0f, 1.0f, 0.0f };
    v[10].Pos = { +w2, +h2, +d2 }; v[10].Normal = { 0.0f, 1.0f, 0.0f };
    v[11].Pos = { +w2, +h2, -d2 }; v[11].Normal = { 0.0f, 1.0f, 0.0f };

    // Bottom Face (Normal -Y)
    v[12].Pos = { -w2, -h2, -d2 }; v[12].Normal = { 0.0f, -1.0f, 0.0f };
    v[13].Pos = { +w2, -h2, -d2 }; v[13].Normal = { 0.0f, -1.0f, 0.0f };
    v[14].Pos = { +w2, -h2, +d2 }; v[14].Normal = { 0.0f, -1.0f, 0.0f };
    v[15].Pos = { -w2, -h2, +d2 }; v[15].Normal = { 0.0f, -1.0f, 0.0f };

    // Left Face (Normal -X)
    v[16].Pos = { -w2, -h2, +d2 }; v[16].Normal = { -1.0f, 0.0f, 0.0f };
    v[17].Pos = { -w2, +h2, +d2 }; v[17].Normal = { -1.0f, 0.0f, 0.0f };
    v[18].Pos = { -w2, +h2, -d2 }; v[18].Normal = { -1.0f, 0.0f, 0.0f };
    v[19].Pos = { -w2, -h2, -d2 }; v[19].Normal = { -1.0f, 0.0f, 0.0f };

    // Right Face (Normal +X)
    v[20].Pos = { +w2, -h2, -d2 }; v[20].Normal = { 1.0f, 0.0f, 0.0f };
    v[21].Pos = { +w2, +h2, -d2 }; v[21].Normal = { 1.0f, 0.0f, 0.0f };
    v[22].Pos = { +w2, +h2, +d2 }; v[22].Normal = { 1.0f, 0.0f, 0.0f };
    v[23].Pos = { +w2, -h2, +d2 }; v[23].Normal = { 1.0f, 0.0f, 0.0f };

    vertices.insert(vertices.end(), std::begin(v), std::end(v));

    uint16_t i[36] = {
        // Front
        0, 1, 2, 0, 2, 3,
        // Back
        4, 5, 6, 4, 6, 7,
        // Top
        8, 9, 10, 8, 10, 11,
        // Bottom
        12, 13, 14, 12, 14, 15,
        // Left
        16, 17, 18, 16, 18, 19,
        // Right
        20, 21, 22, 20, 22, 23
    };

    indices.insert(indices.end(), std::begin(i), std::end(i));
}

void StencilingApp::CreateGrid(float width, float depth, uint32_t m, uint32_t n, std::vector<Vertex>& vertices, std::vector<uint16_t>& indices) {
    uint32_t vertexCount = m * n;
    float dx = width / (n - 1); float dz = depth / (m - 1);
    float du = 1.0f / (n - 1); float dv = 1.0f / (m - 1);
    float w2 = 0.5f * width; float d2 = 0.5f * depth;
    for(uint32_t i = 0; i < m; ++i) {
        float z = d2 - i * dz;
        for(uint32_t j = 0; j < n; ++j) {
            float x = -w2 + j * dx;
            Vertex v; v.Pos = { x, 0.0f, z }; v.Normal = { 0.0f, 1.0f, 0.0f };
            vertices.push_back(v);
        }
    }
    for(uint32_t i = 0; i < m - 1; ++i) {
        for(uint32_t j = 0; j < n - 1; ++j) {
            indices.push_back(i * n + j); indices.push_back(i * n + j + 1); indices.push_back((i + 1) * n + j);
            indices.push_back((i + 1) * n + j); indices.push_back(i * n + j + 1); indices.push_back((i + 1) * n + j + 1);
        }
    }
}

void StencilingApp::BuildShapeGeometry() {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    // 1. Grid (Floor/Mirror)
    m_gridGeo.StartIndexLocation = (UINT)indices.size();
    m_gridGeo.BaseVertexLocation = (INT)vertices.size();
    CreateGrid(20.0f, 20.0f, 40, 40, vertices, indices);
    m_gridGeo.IndexCount = (UINT)indices.size() - m_gridGeo.StartIndexLocation;

    // 2. Sphere
    m_sphereGeo.StartIndexLocation = (UINT)indices.size();
    m_sphereGeo.BaseVertexLocation = (INT)vertices.size();
    CreateSphere(0.5f, 20, 20, vertices, indices);
    m_sphereGeo.IndexCount = (UINT)indices.size() - m_sphereGeo.StartIndexLocation;

    // 3. Mirror Quad (Using CreateBox but flattened or just Front Face effectively)
    // Actually, to be pedagogical, let's use CreateBox but we only care about the front face if we wanted to be efficient,
    // but here we just create a thin box or use a helper. 
    // Let's implement CreateMirrorQuad properly as a helper for this specific lesson or JUST reuse CreateGrid rotated.
    // Reverting to the "CreateGrid and Rotate" approach is most standard for these lessons, BUT geometry generation is easier with a manual quad.
    // I will rename CreateBox back to CreateMirrorQuad in definition? No, the user wants standard.
    // I will use CreateBox for a thin box acting as a mirror.
    m_mirrorGeo.StartIndexLocation = (UINT)indices.size();
    m_mirrorGeo.BaseVertexLocation = (INT)vertices.size();
    
    // Create a thin box to act as the mirror
    CreateBox(6.0f, 4.0f, 0.1f, vertices, indices);
    
    m_mirrorGeo.IndexCount = (UINT)indices.size() - m_mirrorGeo.StartIndexLocation;

    UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

    ThrowIfFailed(CreateDefaultBuffer(m_d3dDevice.Get(), m_commandList.Get(), vertices.data(), vbByteSize, m_vertexBufferUpload, m_vertexBuffer));
    ThrowIfFailed(CreateDefaultBuffer(m_d3dDevice.Get(), m_commandList.Get(), indices.data(), ibByteSize, m_indexBufferUpload, m_indexBuffer));

    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vbByteSize;

    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_indexBufferView.SizeInBytes = ibByteSize;
}

void StencilingApp::BuildRenderItems() {
    // 1. Opaque Floor
    auto floor = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&floor->World, XMMatrixTranslation(0.0f, -1.0f, 0.0f));
    floor->DiffuseAlbedo = { 0.4f, 0.4f, 0.4f, 1.0f };
    floor->Roughness = 0.5f;
    floor->IndexCount = m_gridGeo.IndexCount;
    floor->StartIndexLocation = m_gridGeo.StartIndexLocation;
    floor->BaseVertexLocation = m_gridGeo.BaseVertexLocation;
    m_ritemLayer[(int)RenderLayer::Opaque].push_back(floor.get());
    m_allRitems.push_back(std::move(floor));

    // 2. Opaque Skull (Sphere)
    auto skull = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&skull->World, XMMatrixTranslation(0.0f, 0.5f, -2.0f));
    skull->DiffuseAlbedo = { 0.8f, 0.2f, 0.2f, 1.0f };
    skull->Roughness = 0.1f;
    skull->IndexCount = m_sphereGeo.IndexCount;
    skull->StartIndexLocation = m_sphereGeo.StartIndexLocation;
    skull->BaseVertexLocation = m_sphereGeo.BaseVertexLocation;
    m_ritemLayer[(int)RenderLayer::Opaque].push_back(skull.get());

    // 3. Mirror (Mark Stencil)
    auto mirror = std::make_unique<RenderItem>();
    
    // The mirror Quad is defined in XY plane with Normal pointing to -Z.
    // It is created with width 6 and height 4.
    // Translate to (0, 1.0, 2.0) so it sits on the floor
    XMMATRIX mirrorWorld = XMMatrixTranslation(0.0f, 1.0f, 2.0f);
    XMStoreFloat4x4(&mirror->World, mirrorWorld);
    
    mirror->DiffuseAlbedo = { 0.1f, 0.1f, 0.3f, 0.3f }; 
    mirror->Roughness = 0.1f;
    mirror->IndexCount = m_mirrorGeo.IndexCount;
    mirror->StartIndexLocation = m_mirrorGeo.StartIndexLocation;
    mirror->BaseVertexLocation = m_mirrorGeo.BaseVertexLocation;
    m_ritemLayer[(int)RenderLayer::Mirrors].push_back(mirror.get());
    m_ritemLayer[(int)RenderLayer::Transparent].push_back(mirror.get());
    
    // 4. Reflected Skull
    auto reflectedSkull = std::make_unique<RenderItem>();
    
    // Mirror Plane: Z=2. Normal points to -Z (0, 0, -1).
    // Plane from Point(0, 1, 2) and Normal(0, 0, -1).
    XMVECTOR mirrorPlane = XMPlaneFromPointNormal(XMVectorSet(0.0f, 1.0f, 2.0f, 1.0f), XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f));
    XMMATRIX R = XMMatrixReflect(mirrorPlane);

    XMMATRIX skullWorld = XMLoadFloat4x4(&skull->World);
    XMStoreFloat4x4(&reflectedSkull->World, skullWorld * R);
    
    // Reflected object winding order is flipped, but we use Cull Front state, so this matches without scaling hack.
    
    reflectedSkull->DiffuseAlbedo = { 0.2f, 0.8f, 0.2f, 1.0f };
    
    reflectedSkull->Roughness = 0.1f;
    reflectedSkull->IndexCount = m_sphereGeo.IndexCount;
    reflectedSkull->StartIndexLocation = m_sphereGeo.StartIndexLocation;
    reflectedSkull->BaseVertexLocation = m_sphereGeo.BaseVertexLocation;
    m_ritemLayer[(int)RenderLayer::Reflected].push_back(reflectedSkull.get());
    m_allRitems.push_back(std::move(reflectedSkull));
    
    m_allRitems.push_back(std::move(skull));
    m_allRitems.push_back(std::move(mirror));
}

void StencilingApp::BuildPSOs() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(m_vsByteCode->GetBufferPointer()), m_vsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(m_psByteCode->GetBufferPointer()), m_psByteCode->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = m_depthStencilFormat;

    // 1. Opaque PSO
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_psoOpaque)));

    // 2. Mark Mirrors PSO
    // - Disable Color Write (don't draw yet)
    // - Write 1 to Stencil
    // - Depth Write Disabled (don't occlude)
    // - Cull Mode None (Ensure we mark the mirror regardless of orientation)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC mirrorPsoDesc = psoDesc;
    mirrorPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; 
    CD3DX12_BLEND_DESC mirrorBlendState(D3D12_DEFAULT);
    mirrorBlendState.RenderTarget[0].RenderTargetWriteMask = 0;
    mirrorPsoDesc.BlendState = mirrorBlendState;
    
    CD3DX12_DEPTH_STENCIL_DESC mirrorDS(D3D12_DEFAULT);
    mirrorDS.DepthEnable = true;
    mirrorDS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write depth, we want to see behind it? 
    // Wait, if we don't write depth, the reflection won't be depth tested against the mirror surface properly?
    // Actually, the mirror surface itself is usually drawn transparently later. 
    // The Reflected objects are drawn "inside" the mirror.
    // We want the Stencil to be marked where the Mirror IS.
    mirrorDS.StencilEnable = true;
    mirrorDS.StencilWriteMask = 0xff;
    mirrorDS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    mirrorDS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    mirrorDS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
    mirrorDS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    mirrorPsoDesc.DepthStencilState = mirrorDS;
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&mirrorPsoDesc, IID_PPV_ARGS(&m_psoMarkMirrors)));

    // 3. Draw Reflection PSO
    // - Visible only where Stencil == 1
    // - Cull Mode = Front (Winding flipped)
    // - Standard Blending (Opaque reflection)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC reflectPsoDesc = psoDesc;
    CD3DX12_DEPTH_STENCIL_DESC reflectDS(D3D12_DEFAULT);
    reflectDS.StencilEnable = true;
    reflectDS.StencilReadMask = 0xff;
    reflectDS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    reflectDS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    reflectDS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    reflectDS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL; // Draw where stencil == Ref
    
    // IMPORTANT: Since we flip winding order with reflection, the hardware might consider these Back Faces 
    // depending on CullMode state and Winding. Even if we Cull Front, the rasterizer still classifies them.
    // To be safe, set BackFace settings to match TopFace (or just set them to be sure).
    reflectDS.BackFace = reflectDS.FrontFace;

    // Allow reflected pixels to pass where depth is equal as well and avoid writing depth to reduce z-fighting
    reflectDS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    reflectDS.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    reflectPsoDesc.DepthStencilState = reflectDS;

    CD3DX12_RASTERIZER_DESC reflectRaster(D3D12_DEFAULT);
    // Cull front faces because reflection flips winding order; keep front cull to avoid drawing the 'back' of the reflected mesh
    reflectRaster.CullMode = D3D12_CULL_MODE_FRONT;
    reflectPsoDesc.RasterizerState = reflectRaster;
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&reflectPsoDesc, IID_PPV_ARGS(&m_psoDrawReflections)));

    // 4. Transparent PSO (for Mirror surface)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC transPsoDesc = psoDesc;
    CD3DX12_BLEND_DESC transBlend(D3D12_DEFAULT); 
    transBlend.RenderTarget[0].BlendEnable = true;
    transBlend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    transBlend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    transPsoDesc.BlendState = transBlend;
    // Make mirror surface double-sided so we can see it from either side
    CD3DX12_RASTERIZER_DESC transRaster(D3D12_DEFAULT);
    transRaster.CullMode = D3D12_CULL_MODE_NONE;
    transPsoDesc.RasterizerState = transRaster;
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&transPsoDesc, IID_PPV_ARGS(&m_psoTransparent)));
}

void StencilingApp::OnUpdate() {
    OnKeyboardInput();
    m_camera.UpdateViewMatrix();
    
    // Update Constants that are per-frame but here stuck in per-object for simplicity
    m_constants.LightDir = { 0.57735f, -0.57735f, 0.57735f };
    m_constants.LightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    m_constants.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    m_constants.EyePosW = m_camera.GetPosition3f();
}

void StencilingApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
    for(size_t i = 0; i < ritems.size(); ++i) {
        auto ri = ritems[i];
        
        m_constants.World = ri->World;
        XMMATRIX world = XMLoadFloat4x4(&ri->World);
        XMMATRIX viewProj = m_camera.GetViewProj();
        
        // FIX: The shader computes posW = mul(posL, World), then posH = mul(posW, ViewProj).
        // So m_constants.ViewProj must be ONLY View * Proj. 
        // DO NOT include World matrix here.
        XMStoreFloat4x4(&m_constants.ViewProj, XMMatrixTranspose(viewProj));

        XMStoreFloat4x4(&m_constants.World, XMMatrixTranspose(world));
        
        m_constants.DiffuseAlbedo = ri->DiffuseAlbedo;
        m_constants.FresnelR0 = ri->FresnelR0;
        m_constants.Roughness = ri->Roughness;

        cmdList->SetGraphicsRoot32BitConstants(0, sizeof(ObjectConstants)/4, &m_constants, 0);
        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void StencilingApp::OnRender() {
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_psoOpaque.Get()));
    
    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrier);

    auto rtv = CurrentBackBufferView();
    auto dsv = DepthStencilView();
    m_commandList->ClearRenderTargetView(rtv, Colors::LightSteelBlue, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    m_commandList->OMSetRenderTargets(1, &rtv, true, &dsv);
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 1. Draw Opaque
    m_commandList->SetPipelineState(m_psoOpaque.Get());
    DrawRenderItems(m_commandList.Get(), m_ritemLayer[(int)RenderLayer::Opaque]);

    // 2. Mark Mirror (Stencil Ref = 1)
    m_commandList->OMSetStencilRef(1);
    m_commandList->SetPipelineState(m_psoMarkMirrors.Get());
    DrawRenderItems(m_commandList.Get(), m_ritemLayer[(int)RenderLayer::Mirrors]);

    // 3. Draw Reflection (Stencil Ref == 1)
    m_commandList->SetPipelineState(m_psoDrawReflections.Get());
    DrawRenderItems(m_commandList.Get(), m_ritemLayer[(int)RenderLayer::Reflected]);

    // 4. Draw Transparent Mirror Surface
    // Reset Stencil Ref not needed as opaque things don't check it?
    // Wait, Transparent psoDesc didn't enable Stencil. So it will ignore stencil. Correct.
    // So we just draw on top.
    m_commandList->SetPipelineState(m_psoTransparent.Get());
    DrawRenderItems(m_commandList.Get(), m_ritemLayer[(int)RenderLayer::Transparent]);

    auto barrierPresent = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrierPresent);
    
    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(m_swapChain->Present(0, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;
    FlushCommandQueue();
}

void StencilingApp::OnKeyboardInput() {
    float dt = 0.016f; float speed = 10.0f;
    if (GetAsyncKeyState('W') & 0x8000) m_camera.Walk(speed * dt);
    if (GetAsyncKeyState('S') & 0x8000) m_camera.Walk(-speed * dt);
    if (GetAsyncKeyState('A') & 0x8000) m_camera.Strafe(-speed * dt);
    if (GetAsyncKeyState('D') & 0x8000) m_camera.Strafe(speed * dt);
}
void StencilingApp::OnMouseMove(WPARAM btnState, int x, int y) {
    if ((btnState & MK_LBUTTON) != 0) {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
        m_camera.Pitch(dy);
        m_camera.RotateY(dx);
    }
    mLastMousePos.x = x; mLastMousePos.y = y;
}
LRESULT StencilingApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
     switch(msg) {
        case WM_LBUTTONDOWN: m_camera.SetPosition(0,2,-7); mLastMousePos.x = LOWORD(lParam); mLastMousePos.y = HIWORD(lParam); SetCapture(hwnd); return 0;
        case WM_LBUTTONUP: ReleaseCapture(); return 0;
        case WM_MOUSEMOVE: OnMouseMove(wParam, LOWORD(lParam), HIWORD(lParam)); return 0;
    }
    return VibeDX12App::MsgProc(hwnd, msg, wParam, lParam);
}

#include <stdexcept>

// ...

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
#if defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    try {
        StencilingApp theApp(hInstance);
        if(!theApp.Initialize()) return 0;
        return theApp.Run();
    }
    catch(const std::exception& e) {
        std::string msg = "Uncaught Exception: ";
        msg += e.what();
        MessageBoxA(nullptr, msg.c_str(), "Context Error", MB_OK);
        return 0;
    }
    catch(char* e) { MessageBoxA(nullptr, e, "Error", MB_OK); return 0; }
}
