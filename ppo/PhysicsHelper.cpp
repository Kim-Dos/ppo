#include "PhysicsHelper.h"

bool PhysicsHelper::CheckTransformedBoundingBoxCollision(const BoundingBox& box1, const DirectX::XMMATRIX& worldMatrix1,
    const BoundingBox& box2, const DirectX::XMMATRIX& worldMatrix2) 
{
    // 박스 1의 변환을 적용하여 꼭지점을 가져옵니다.
    std::vector<DirectX::XMFLOAT3> transformedCorners = GetTransformedCorners(box1, worldMatrix1);
    std::vector<DirectX::XMFLOAT3> localCorners = GetLocalCorners(transformedCorners, worldMatrix2);

    // 꼭지점들의 충돌을 확인합니다.
    for (const auto& corner : localCorners) {
        // 꼭지점이 다른 박스 내부에 있는지 확인합니다.
        if (IsPointInsideBoundingBox(corner, box2)) {
            return true; // 충돌이 감지됨
        }
    }

    // 모든 꼭지점에 대해 충돌이 없으면 충돌이 없음을 반환합니다.
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

    // BoundingBox의 각 꼭지점을 가져옵니다.
    box.GetCorners(corners.data());

    // 변환된 꼭지점 계산
    for (auto& corner : corners) {
        DirectX::XMVECTOR vec = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&corner), worldMatrix);
        DirectX::XMStoreFloat3(&corner, vec);
    }

    return corners;
}

std::vector<DirectX::XMFLOAT3> PhysicsHelper::GetLocalCorners(const std::vector<DirectX::XMFLOAT3>& corners, const DirectX::XMMATRIX& worldMatrix) {
    std::vector<DirectX::XMFLOAT3> localCorners(8);

    // 역 행렬을 사용하여 월드 좌표계에서 로컬 좌표계로 꼭지점을 변환합니다.
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
    // 꼭지점의 위치와 BoundingBox의 최소, 최대 좌표 사이에 있는지 확인합니다.
    return point.x >= box.Center.x - box.Extents.x && point.x <= box.Center.x + box.Extents.x &&
        point.y >= box.Center.y - box.Extents.y && point.y <= box.Center.y + box.Extents.y &&
        point.z >= box.Center.z - box.Extents.z && point.z <= box.Center.z + box.Extents.z;
}