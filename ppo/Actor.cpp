#include "Actor.h"


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
