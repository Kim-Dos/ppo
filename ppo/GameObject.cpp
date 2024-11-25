#include "GameObject.h"

Actor::Actor()
{
}

Actor::Actor(const string name, XMFLOAT4X4 world)
{
    mName = name;
    mWorld = world;
}

Actor::Actor(const string name, XMMATRIX world)
{
    mName = name;
    XMStoreFloat4x4(&mWorld, world);
}

Actor::~Actor()
{
}

void Actor::Update(const GameTimer& gt)
{
}

void Actor::SetWorldMat(XMFLOAT4X4 world)
{
    mWorld = world;
    SetFrameDirty();
}

void Actor::SetPosition(float x, float y, float z)
{
    mWorld._41 = x;
    mWorld._42 = y;
    mWorld._43 = z;
}

void Actor::SetPosition(XMFLOAT3 position)
{
    mWorld._41 = position.x;
    mWorld._42 = position.y;
    mWorld._43 = position.z;
}

XMFLOAT4X4 Actor::GetWorld()
{
    return mWorld;
}

XMFLOAT3 Actor::GetPosition()
{
    return XMFLOAT3(mWorld._41, mWorld._42, mWorld._43);
}

XMFLOAT3 Actor::GetLook()
{
    return Vector3::Normalize(XMFLOAT3(mWorld._31, mWorld._32, mWorld._33));
}

XMFLOAT3 Actor::GetUp()
{
    return Vector3::Normalize(XMFLOAT3(mWorld._21, mWorld._22, mWorld._23));
}

XMFLOAT3 Actor::GetRight()
{
    return Vector3::Normalize(XMFLOAT3(mWorld._11, mWorld._12, mWorld._13));
}

void Actor::MoveStrafe(float distance)
{
    SetPosition(Vector3::Add(GetPosition(), GetRight(), distance));
}

void Actor::MoveFront(float distance)
{
    XMFLOAT3 frontVector = GetLook();
    frontVector.y = 0.0f;
    SetPosition(Vector3::Add(GetPosition(), Vector3::Normalize(frontVector), distance));
}

void Actor::MoveUp(float distance)
{
    SetPosition(Vector3::Add(GetPosition(), GetUp(), distance));
}

void Actor::MoveForward(float distance)
{
    SetPosition(Vector3::Add(GetPosition(), GetLook(), distance));
}

void Actor::Rotate(float pitch, float yaw, float roll)
{
    XMVECTOR rotation = XMQuaternionRotationMatrix(XMLoadFloat4x4(&mWorld));

    XMMATRIX pitchRotation = XMMatrixRotationAxis(XMLoadFloat3(&GetRight()), pitch);
    XMMATRIX yawRotation = XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), yaw);
    XMMATRIX rollRotation = XMMatrixRotationAxis(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), roll);

    XMMATRIX rotationMatrix = rollRotation * pitchRotation * yawRotation;

    XMStoreFloat4x4(&mWorld, rotationMatrix * XMLoadFloat4x4(&mWorld));
}

void Actor::Rotate(XMFLOAT3* axis, float angle)
{
    XMMATRIX rotateMat = XMMatrixRotationAxis(XMLoadFloat3(axis), XMConvertToRadians(angle));
    mWorld = Matrix4x4::Multiply(rotateMat, mWorld);
}

void Actor::Rotate(XMFLOAT4* quaternion)
{
    XMMATRIX rotateMat = XMMatrixRotationQuaternion(XMLoadFloat4(quaternion));
    mWorld = Matrix4x4::Multiply(rotateMat, mWorld);
}


GameObject::GameObject() : Actor()
{
}

GameObject::GameObject(const string name, XMFLOAT4X4 world, XMFLOAT4X4 texTransform) : Actor(name, world)
{

    mTexTransform = texTransform;
}

GameObject::GameObject(const string name, XMMATRIX world, XMMATRIX texTransform) : Actor(name, world)
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

<<<<<<< Updated upstream
XMFLOAT4X4 GameObject::GetWorld()
{
    /*if (mWorldMatDirty)
        UpdateWorldMatrix();*/

    return mWorld;
}

XMFLOAT3 GameObject::GetPosition()
{
    return XMFLOAT3(mWorld._41, mWorld._42, mWorld._43);
}

XMFLOAT3 GameObject::GetLook()
{
    return Vector3::Normalize(XMFLOAT3(mWorld._31, mWorld._32, mWorld._33));
}

XMFLOAT3 GameObject::GetUp()
{
    return Vector3::Normalize(XMFLOAT3(mWorld._21, mWorld._22, mWorld._23));
}

XMFLOAT3 GameObject::GetRight()
{
    return Vector3::Normalize(XMFLOAT3(mWorld._11, mWorld._12, mWorld._13));
}

void GameObject::MoveStrafe(float distance)
{
    SetPosition(Vector3::Add(GetPosition(), GetRight(), distance));
}

void GameObject::MoveFront(float distance)
{
    XMFLOAT3 frontVector = GetLook();
    frontVector.y = 0.0f;
    SetPosition(Vector3::Add(GetPosition(), Vector3::Normalize(frontVector), distance));
}

void GameObject::MoveUp(float distance)
{
    SetPosition(Vector3::Add(GetPosition(), GetUp(), distance));
}

void GameObject::MoveForward(float distance)
{
    SetPosition(Vector3::Add(GetPosition(), GetLook(), distance));
}

void GameObject::Rotate(float pitch, float yaw, float roll)
{
    XMVECTOR rotation = XMQuaternionRotationMatrix(XMLoadFloat4x4(&mWorld));

    XMMATRIX pitchRotation = XMMatrixRotationAxis(XMLoadFloat3(&GetRight()), pitch);
    XMMATRIX yawRotation = XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), yaw);
    XMMATRIX rollRotation = XMMatrixRotationAxis(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), roll);

    XMMATRIX rotationMatrix = rollRotation * pitchRotation * yawRotation;

    XMStoreFloat4x4(&mWorld, rotationMatrix * XMLoadFloat4x4(&mWorld));
}

void GameObject::Rotate(XMFLOAT3* axis, float angle)
{
    XMMATRIX rotateMat = XMMatrixRotationAxis(XMLoadFloat3(axis), XMConvertToRadians(angle));
    mWorld = Matrix4x4::Multiply(rotateMat, mWorld);
}

void GameObject::Rotate(XMFLOAT4* quaternion)
{
    XMMATRIX rotateMat = XMMatrixRotationQuaternion(XMLoadFloat4(quaternion));
    mWorld = Matrix4x4::Multiply(rotateMat, mWorld);
}
=======

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

D3D12_VERTEX_BUFFER_VIEW GameObject::BoundingBoxVertexBufferView() const
{
    if (mBoundVertexBufferGPU != nullptr) {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = mBoundVertexBufferGPU->GetGPUVirtualAddress();
        vbv.StrideInBytes = sizeof(ColorVertex);
        vbv.SizeInBytes = 8 * sizeof(ColorVertex);

        return vbv;
    }
}

D3D12_INDEX_BUFFER_VIEW GameObject::BoundingBoxIndexBufferView() const
{
    if (mBoundIndexBufferGPU) {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = mBoundIndexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = DXGI_FORMAT_R32_UINT;
        ibv.SizeInBytes = 24 * sizeof(UINT);

        return ibv;
    }
}

>>>>>>> Stashed changes
