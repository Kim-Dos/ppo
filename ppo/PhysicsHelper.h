#pragma once
#include "d3dUtil.h"

namespace PhysicsHelper
{
    bool CheckTransformedBoundingBoxCollision(const BoundingBox& box1, const DirectX::XMMATRIX& worldMatrix1,
        const BoundingBox& box2, const DirectX::XMMATRIX& worldMatrix2);

    XMFLOAT3 GetCollisionNormal(const DirectX::XMMATRIX& worldMatrix1, const DirectX::XMMATRIX& worldMatrix2);

    std::vector<DirectX::XMFLOAT3> GetTransformedCorners(const BoundingBox& box, const DirectX::XMMATRIX& worldMatrix);

    std::vector<DirectX::XMFLOAT3> GetLocalCorners(const std::vector<DirectX::XMFLOAT3>& corners, const DirectX::XMMATRIX& worldMatrix);

    bool IsPointInsideBoundingBox(const DirectX::XMFLOAT3& point, const BoundingBox& box);

    // ===== 지상 이동 충돌 검사 함수들 =====

     // AABB 침투 검사 (XZ 평면 기준 - 지상 이동용)
    bool GetPenetrationXZ(
        const BoundingBox& moving,
        const BoundingBox& obstacle,
        DirectX::XMFLOAT3& pushDir,
        float& pushDepth);

    // 오브젝트의 로컬BB + 월드 위치로 월드 공간 BB를 계산
    BoundingBox MakeWorldBB(
        const BoundingBox& localBox,
        const DirectX::XMFLOAT3& worldPos);

    // 캐릭터 vs 정적 충돌체 해소 (캐릭터만 밀림)
    DirectX::XMFLOAT3 ResolveStaticCollision(
        const BoundingBox& playerBox,
        const DirectX::XMFLOAT3& desiredPos,
        const std::vector<BoundingBox>& staticColliders);

    // 두 동적 오브젝트 간 충돌 해소
    // movingA/B: 이동 중 여부
    //   둘 다 이동 → 반반 밀림
    //   한쪽만 이동 → 이동 중인 쪽만 밀림
    //   둘 다 정지 → 반반 밀림 (겹침 해소)
    bool ResolveDynamicCollision(
        DirectX::XMFLOAT3& posA,
        DirectX::XMFLOAT3& posB,
        const BoundingBox& localBoxA,
        const BoundingBox& localBoxB,
        bool movingA = true,
        bool movingB = true);

    // 충돌 시 속도 보정 (밀린 축의 속도를 0으로)
    void AdjustVelocityAfterCollision(
        DirectX::XMFLOAT3& velocity,
        const DirectX::XMFLOAT3& originalPos,
        const DirectX::XMFLOAT3& resolvedPos);
};

