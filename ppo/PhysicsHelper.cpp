#include "PhysicsHelper.h"

bool PhysicsHelper::GetPenetrationXZ(
    const BoundingBox& moving,
    const BoundingBox& obstacle,
    DirectX::XMFLOAT3& pushDir,
    float& pushDepth)
{
    // 각 축의 겹침량 계산
    float overlapX = (moving.Extents.x + obstacle.Extents.x) - std::abs(moving.Center.x - obstacle.Center.x);
    float overlapY = (moving.Extents.y + obstacle.Extents.y) - std::abs(moving.Center.y - obstacle.Center.y);
    float overlapZ = (moving.Extents.z + obstacle.Extents.z) - std::abs(moving.Center.z - obstacle.Center.z);

    // 한 축이라도 겹치지 않으면 충돌 아님
    if (overlapX <= 0.0f || overlapY <= 0.0f || overlapZ <= 0.0f)
        return false;

    // XZ 중 겹침이 더 얕은(작은) 축 방향으로 밀어냄 (최소 침투 방향)
    // Y축은 지상 이동이므로 무시 (Terrain이 처리)
    if (overlapX <= overlapZ)
    {
        pushDepth = overlapX;
        float sign = (moving.Center.x > obstacle.Center.x) ? 1.0f : -1.0f;
        pushDir = DirectX::XMFLOAT3(sign, 0.0f, 0.0f);
    }
    else
    {
        pushDepth = overlapZ;
        float sign = (moving.Center.z > obstacle.Center.z) ? 1.0f : -1.0f;
        pushDir = DirectX::XMFLOAT3(0.0f, 0.0f, sign);
    }

    return true;
}


DirectX::XMFLOAT3 PhysicsHelper::ResolveGroundCollision(
    const BoundingBox& playerBox,
    const DirectX::XMFLOAT3& desiredPos,
    const std::vector<BoundingBox>& colliders)
{
    DirectX::XMFLOAT3 resolvedPos = desiredPos;

    // 플레이어 BB를 월드 위치로 이동시킨 테스트용 BB
    BoundingBox testBox;
    testBox.Extents = playerBox.Extents;

    // 반복 해소 (한 번에 여러 충돌체와 겹칠 수 있으므로 최대 4회 반복)
    const int MAX_ITERATIONS = 4;

    for (int iter = 0; iter < MAX_ITERATIONS; ++iter)
    {
        // testBox의 Center = 월드 위치 + 로컬 오프셋
        testBox.Center.x = resolvedPos.x + playerBox.Center.x;
        testBox.Center.y = resolvedPos.y + playerBox.Center.y;
        testBox.Center.z = resolvedPos.z + playerBox.Center.z;

        bool anyCollision = false;

        for (const auto& collider : colliders)
        {
            DirectX::XMFLOAT3 pushDir;
            float pushDepth;

            if (GetPenetrationXZ(testBox, collider, pushDir, pushDepth))
            {
                // 약간의 여유(epsilon)를 더해서 경계에 딱 붙는 떨림 방지
                //const float EPSILON = 0.001f;
                resolvedPos.x += pushDir.x * (pushDepth + EPSILON);
                resolvedPos.z += pushDir.z * (pushDepth + EPSILON);

                // testBox도 갱신
                testBox.Center.x = resolvedPos.x + playerBox.Center.x;
                testBox.Center.y = resolvedPos.y + playerBox.Center.y;
                testBox.Center.z = resolvedPos.z + playerBox.Center.z;

                anyCollision = true;
            }
        }

        // 이번 반복에서 충돌이 없었으면 해소 완료
        if (!anyCollision)
            break;
    }

    return resolvedPos;
}


void PhysicsHelper::AdjustVelocityAfterCollision(
    DirectX::XMFLOAT3& velocity,
    const DirectX::XMFLOAT3& originalPos,
    const DirectX::XMFLOAT3& resolvedPos)
{
    const float THRESHOLD = 0.001f;

    // X축으로 밀렸으면 X속도 제거
    if (std::abs(resolvedPos.x - originalPos.x) > THRESHOLD)
        velocity.x = 0.0f;

    // Z축으로 밀렸으면 Z속도 제거
    if (std::abs(resolvedPos.z - originalPos.z) > THRESHOLD)
        velocity.z = 0.0f;
}

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