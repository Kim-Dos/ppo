#include "PhysicsHelper.h"

// ----------------------------------------------------------
// AABB 침투 검사 (XZ 평면 기준)
// ----------------------------------------------------------
bool PhysicsHelper::GetPenetrationXZ(
    const BoundingBox& moving,
    const BoundingBox& obstacle,
    DirectX::XMFLOAT3& pushDir,
    float& pushDepth)
{
    float overlapX = (moving.Extents.x + obstacle.Extents.x) - std::abs(moving.Center.x - obstacle.Center.x);
    float overlapY = (moving.Extents.y + obstacle.Extents.y) - std::abs(moving.Center.y - obstacle.Center.y);
    float overlapZ = (moving.Extents.z + obstacle.Extents.z) - std::abs(moving.Center.z - obstacle.Center.z);

    // 한 축이라도 안 겹치면 충돌 아님
    if (overlapX <= 0.0f || overlapY <= 0.0f || overlapZ <= 0.0f)
        return false;

    // XZ 중 얕은 축으로 밀어냄 (Y는 terrain이 처리하므로 무시)
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


// ----------------------------------------------------------
// 로컬BB + 월드위치 → 월드 BB
// ----------------------------------------------------------
BoundingBox PhysicsHelper::MakeWorldBB(
    const BoundingBox& localBox,
    const DirectX::XMFLOAT3& worldPos)
{
    BoundingBox worldBB;
    worldBB.Center.x = worldPos.x + localBox.Center.x;
    worldBB.Center.y = worldPos.y + localBox.Center.y;
    worldBB.Center.z = worldPos.z + localBox.Center.z;
    worldBB.Extents = localBox.Extents;
    return worldBB;
}


// ----------------------------------------------------------
// 캐릭터 vs 정적 충돌체 해소
// ----------------------------------------------------------
DirectX::XMFLOAT3 PhysicsHelper::ResolveStaticCollision(
    const BoundingBox& playerBox,
    const DirectX::XMFLOAT3& desiredPos,
    const std::vector<BoundingBox>& staticColliders)
{
    DirectX::XMFLOAT3 resolvedPos = desiredPos;
    const int MAX_ITERATIONS = 4;

    for (int iter = 0; iter < MAX_ITERATIONS; ++iter)
    {
        BoundingBox testBox = MakeWorldBB(playerBox, resolvedPos);
        bool anyCollision = false;

        for (const auto& collider : staticColliders)
        {
            DirectX::XMFLOAT3 pushDir;
            float pushDepth;

            if (GetPenetrationXZ(testBox, collider, pushDir, pushDepth))
            {
                resolvedPos.x += pushDir.x * (pushDepth + EPSILON);
                resolvedPos.z += pushDir.z * (pushDepth + EPSILON);
                anyCollision = true;
            }
        }

        if (!anyCollision)
            break;
    }

    return resolvedPos;
}


// ----------------------------------------------------------
// 두 동적 오브젝트 간 충돌 해소 (이동 상태에 따라 밀리는 비율 결정)
// ----------------------------------------------------------
bool PhysicsHelper::ResolveDynamicCollision(
    DirectX::XMFLOAT3& posA,
    DirectX::XMFLOAT3& posB,
    const BoundingBox& localBoxA,
    const BoundingBox& localBoxB,
    bool movingA,
    bool movingB)
{
    BoundingBox worldA = MakeWorldBB(localBoxA, posA);
    BoundingBox worldB = MakeWorldBB(localBoxB, posB);

    DirectX::XMFLOAT3 pushDir;
    float pushDepth;

    if (!GetPenetrationXZ(worldA, worldB, pushDir, pushDepth))
        return false;

    float totalPush = pushDepth + EPSILON;

    // 밀리는 비율 결정
    float ratioA, ratioB;

    if (movingA && movingB)
    {
        // 둘 다 이동 중 → 반반
        ratioA = 0.5f;
        ratioB = 0.5f;
    }
    else if (movingA && !movingB)
    {
        // A만 이동 중 → A만 밀림
        ratioA = 1.0f;
        ratioB = 0.0f;
    }
    else if (!movingA && movingB)
    {
        // B만 이동 중 → B만 밀림
        ratioA = 0.0f;
        ratioB = 1.0f;
    }
    else
    {
        // 둘 다 정지 → 반반 (겹침 해소)
        ratioA = 0.5f;
        ratioB = 0.5f;
    }

    // A는 pushDir 방향, B는 반대 방향
    posA.x += pushDir.x * totalPush * ratioA;
    posA.z += pushDir.z * totalPush * ratioA;
    posB.x -= pushDir.x * totalPush * ratioB;
    posB.z -= pushDir.z * totalPush * ratioB;

    return true;
}


// ----------------------------------------------------------
// 충돌 후 속도 보정
// ----------------------------------------------------------
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