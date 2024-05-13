#include "PhysicsHelper.h"

bool PhysicsHelper::CheckTransformedBoundingBoxCollision(const BoundingBox& box1, const DirectX::XMMATRIX& worldMatrix1,
    const BoundingBox& box2, const DirectX::XMMATRIX& worldMatrix2) 
{
    // �ڽ� 1�� ��ȯ�� �����Ͽ� �������� �����ɴϴ�.
    std::vector<DirectX::XMFLOAT3> transformedCorners = GetTransformedCorners(box1, worldMatrix1);
    std::vector<DirectX::XMFLOAT3> localCorners = GetLocalCorners(transformedCorners, worldMatrix2);

    // ���������� �浹�� Ȯ���մϴ�.
    for (const auto& corner : localCorners) {
        // �������� �ٸ� �ڽ� ���ο� �ִ��� Ȯ���մϴ�.
        if (IsPointInsideBoundingBox(corner, box2)) {
            return true; // �浹�� ������
        }
    }

    // ��� �������� ���� �浹�� ������ �浹�� ������ ��ȯ�մϴ�.
    return false;
}

XMFLOAT3 PhysicsHelper::GetCollisionNormal(const DirectX::XMMATRIX& worldMatrix1, const DirectX::XMMATRIX& worldMatrix2)
{
    XMFLOAT3 normal;
    XMVECTOR normalVec = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

    normalVec = XMVector3TransformNormal(normalVec, worldMatrix1);
    normalVec = XMVector3TransformNormal(normalVec, XMMatrixTranspose(worldMatrix2));

    XMStoreFloat3(&normal, normalVec);
    return normal;
}

std::vector<DirectX::XMFLOAT3> PhysicsHelper::GetTransformedCorners(const BoundingBox& box, const DirectX::XMMATRIX& worldMatrix) 
{
    std::vector<DirectX::XMFLOAT3> corners(8);

    // BoundingBox�� �� �������� �����ɴϴ�.
    box.GetCorners(corners.data());

    // ��ȯ�� ������ ���
    for (auto& corner : corners) {
        DirectX::XMVECTOR vec = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&corner), worldMatrix);
        DirectX::XMStoreFloat3(&corner, vec);
    }

    return corners;
}

std::vector<DirectX::XMFLOAT3> PhysicsHelper::GetLocalCorners(const std::vector<DirectX::XMFLOAT3>& corners, const DirectX::XMMATRIX& worldMatrix) {
    std::vector<DirectX::XMFLOAT3> localCorners(8);

    // �� ����� ����Ͽ� ���� ��ǥ�迡�� ���� ��ǥ��� �������� ��ȯ�մϴ�.
    DirectX::XMMATRIX inverseWorldMatrix = DirectX::XMMatrixInverse(nullptr, worldMatrix);
    for (int i = 0; i < 8; i++)
    {
        DirectX::XMVECTOR vec = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&corners[i]), inverseWorldMatrix);
        DirectX::XMStoreFloat3(&localCorners[i], vec);
    }

    return localCorners;
}

bool PhysicsHelper::IsPointInsideBoundingBox(const DirectX::XMFLOAT3& point, const BoundingBox& box) 
{
    // �������� ��ġ�� BoundingBox�� �ּ�, �ִ� ��ǥ ���̿� �ִ��� Ȯ���մϴ�.
    return point.x >= box.Center.x - box.Extents.x && point.x <= box.Center.x + box.Extents.x &&
        point.y >= box.Center.y - box.Extents.y && point.y <= box.Center.y + box.Extents.y &&
        point.z >= box.Center.z - box.Extents.z && point.z <= box.Center.z + box.Extents.z;
}