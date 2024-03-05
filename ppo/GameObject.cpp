#include "GameObject.h"

GameObject::GameObject()
{
}

GameObject::GameObject(const string name, UINT objCBIndex, XMFLOAT4X4 world, XMFLOAT4X4 texTransform)
{
    mName = name;
    mObjCBIndex = objCBIndex;
    mWorld = world;
    mTexTransform = texTransform;
}

GameObject::~GameObject()
{
}

void GameObject::SetPosition(float x, float y, float z)
{
    mWorld._41 = x;
    mWorld._42 = y;
    mWorld._43 = z;
}

void GameObject::SetPosition(XMFLOAT3 position)
{
    mWorld._41 = position.x;
    mWorld._42 = position.y;
    mWorld._43 = position.z;
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
    XMMATRIX rotateMat = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(pitch), XMConvertToRadians(yaw), XMConvertToRadians(roll));
    mWorld = Matrix4x4::Multiply(rotateMat, mWorld);
}

void GameObject::Rotate(XMFLOAT3* axis, float angle)
{
    XMMATRIX rotateMat = XMMatrixRotationAxis(
        XMLoadFloat3(axis), XMConvertToRadians(angle));
    mWorld = Matrix4x4::Multiply(rotateMat, mWorld);
}

void GameObject::Rotate(XMFLOAT4* quaternion)
{
    XMMATRIX rotateMat = XMMatrixRotationQuaternion(XMLoadFloat4(quaternion));
    mWorld = Matrix4x4::Multiply(rotateMat, mWorld);
}