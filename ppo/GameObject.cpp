#include "GameObject.h"



GameObject::GameObject() : Actor()
{
}

GameObject::GameObject(const string name, ObjectsType type, XMFLOAT4X4 world, XMFLOAT4X4 texTransform) : Actor(name, world, type)
{

    mTexTransform = texTransform;
}

GameObject::GameObject(const string name, ObjectsType type, XMMATRIX world, XMMATRIX texTransform) : Actor(name, world, type)
{

    XMStoreFloat4x4(&mTexTransform, texTransform);
}

GameObject::~GameObject()
{
}

void GameObject::Update(const GameTimer& gt)
{
}

void GameObject::SetMaterial(Material* material)
{
    mMaterials[0] = material;
}

void GameObject::SetMaterials(UINT numMaterials, const vector<Material*>& materials)
{
    for (int i = 0; i < numMaterials; i++)
    {
        mMaterials[i] = materials[i];
    }
}

void GameObject::SetCBIndex(int& objCBIndex)
{
    mObjCBIndex[0] = objCBIndex;
    mSkinnedCBIndex = -1;
    objCBIndex++;
}

void GameObject::SetCBIndex(int& objCBIndex, int& skinnedCBIndex)
{
    mObjCBIndex[0] = objCBIndex;
    mSkinnedCBIndex = skinnedCBIndex;
    objCBIndex++;
    skinnedCBIndex++;
}

void GameObject::SetCBIndex(int numObjCBs, int& objCBIndices, int& skinnedCBIndex)
{
    for (int i = 0; i < numObjCBs; i++)
    {
        mObjCBIndex[i] = objCBIndices;
        objCBIndices++;
    }

    mSkinnedCBIndex = skinnedCBIndex;
    skinnedCBIndex++;
}

void GameObject::AddSubmesh(const Submesh& submesh)
{
    mDrawIndex[mNumSubmeshes].mNumIndices = submesh.numIndices;
    mDrawIndex[mNumSubmeshes].mBaseVertex = submesh.baseVertex;
    mDrawIndex[mNumSubmeshes].mBaseIndex = submesh.baseIndex;

    ++mNumSubmeshes;
}

void GameObject::SetScale(float x, float y, float z)
{
    XMMATRIX mtxScale = XMMatrixScaling(x, y, z);
    mWorld = Matrix4x4::Multiply(mtxScale, mWorld);
}

void GameObject::SetScale(XMFLOAT3 scale)
{
    XMMATRIX mtxScale = XMMatrixScaling(scale.x, scale.y, scale.z);
    mWorld = Matrix4x4::Multiply(mtxScale, mWorld);
}

void GameObject::SetTextureScale(float x, float y, float z)
{
    XMMATRIX mtxScale = XMMatrixScaling(x, y, z);
    mTexTransform = Matrix4x4::Multiply(mtxScale, mTexTransform);
}

void GameObject::SetTextureScale(XMFLOAT3 scale)
{
    XMMATRIX mtxScale = XMMatrixScaling(scale.x, scale.y, scale.z);
    mTexTransform = Matrix4x4::Multiply(mtxScale, mTexTransform);
}


void GameObject::CreateBoundingBox(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* commandList)
{
    // 바운딩 박스 생성
    XMFLOAT3 corners[8];
    GetBoundingBox().GetCorners(corners);
    std::vector<ColorVertex> vertices(8);
    for (int i = 0; i < 8; i++)
    {
        vertices[i].Pos = corners[i];
        vertices[i].Color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
    }
    std::vector<UINT> indices = {
        0, 1, 1, 2, 2, 3, 3, 0, // 앞면
        4, 5, 5, 6, 6, 7, 7, 4, // 뒷면
        0, 4, 1, 5, 2, 6, 3, 7  // 모서리
    };

    const UINT vbByteSize = vertices.size() * sizeof(ColorVertex);
    const UINT ibByteSize = indices.size() * sizeof(UINT);

    mBoundVertexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice, commandList,
        vertices.data(), vbByteSize, mBoundVertexBufferUploader);

    mBoundIndexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice, commandList,
        indices.data(), ibByteSize, mBoundIndexBufferUploader);
}

void GameObject::CreateCylinderBoundingBox(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* commandList, int segments)
{
    // BoundingBox에서 원기둥 파라미터 추출
    // Center = (0, 85, 0), Extents = (40, 85, 40)
    XMFLOAT3 center = mBoundingBox.Center;
    XMFLOAT3 extents = mBoundingBox.Extents;

    float radius = max(extents.x, extents.z); // XZ 중 큰 값 = 반지름
    float halfHeight = extents.y;                    // Y Extent = 반높이

    float bottomY = center.y - halfHeight;
    float topY = center.y + halfHeight;

    // 정점: 상단 원 + 하단 원 = segments * 2
    std::vector<ColorVertex> vertices;
    vertices.reserve(segments * 2);

    for (int i = 0; i < segments; ++i)
    {
        float angle = (2.0f * XM_PI * i) / segments;
        float x = center.x + radius * cosf(angle);
        float z = center.z + radius * sinf(angle);

        // 하단 원
        ColorVertex bottom;
        bottom.Pos = XMFLOAT3(x, bottomY, z);
        bottom.Color = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f); // 초록색 (캐릭터 구분)
        vertices.push_back(bottom);

        // 상단 원
        ColorVertex top;
        top.Pos = XMFLOAT3(x, topY, z);
        top.Color = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
        vertices.push_back(top);
    }

    // 인덱스: 하단 원 + 상단 원 + 수직선
    std::vector<UINT> indices;
    indices.reserve(segments * 6);

    for (int i = 0; i < segments; ++i)
    {
        int next = (i + 1) % segments;

        int bottomCurr = i * 2;
        int topCurr = i * 2 + 1;
        int bottomNext = next * 2;
        int topNext = next * 2 + 1;

        // 하단 원 선분
        indices.push_back(bottomCurr);
        indices.push_back(bottomNext);

        // 상단 원 선분
        indices.push_back(topCurr);
        indices.push_back(topNext);

        // 수직선 (4개만 - 90도 간격)
        if (i % (segments / 4) == 0)
        {
            indices.push_back(bottomCurr);
            indices.push_back(topCurr);
        }
    }

    mBoundIndexCount = (UINT)indices.size();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(ColorVertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(UINT);

    mBoundVertexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice, commandList,
        vertices.data(), vbByteSize, mBoundVertexBufferUploader);

    mBoundIndexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice, commandList,
        indices.data(), ibByteSize, mBoundIndexBufferUploader);
}

D3D12_VERTEX_BUFFER_VIEW GameObject::BoundingBoxVertexBufferView() const
{
    if (mBoundVertexBufferGPU != nullptr) {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = mBoundVertexBufferGPU->GetGPUVirtualAddress();
        vbv.StrideInBytes = sizeof(ColorVertex);
        vbv.SizeInBytes = (UINT)mBoundVertexBufferGPU->GetDesc().Width;
        return vbv;
    }
    return {};
}

D3D12_INDEX_BUFFER_VIEW GameObject::BoundingBoxIndexBufferView() const
{
    if (mBoundIndexBufferGPU) {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = mBoundIndexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = DXGI_FORMAT_R32_UINT;
        ibv.SizeInBytes = mBoundIndexCount * sizeof(UINT);
        return ibv;
    }
    return {};
}

void GameObject::SetMaxHP(int maxHP)
{
   if (maxHP  < 0 ) {
        mMaxHP = 0;
    } else {
        mMaxHP = maxHP;
    }
    // 현재 HP가 최대 HP를 초과하지 않도록 조정
    if (mCurrentHP > mMaxHP) {
        mCurrentHP = mMaxHP;
	}
}

void GameObject::SetCurrentHP(int currentHP)
{
    if (currentHP < 0) {
        mCurrentHP = 0;
    } else if (currentHP > mMaxHP) {
        mCurrentHP = mMaxHP;
    } else {
        mCurrentHP = currentHP;
    }
}
