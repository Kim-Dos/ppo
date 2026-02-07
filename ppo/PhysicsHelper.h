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

    bool GetPenetrationXZ(
        const BoundingBox& moving,
        const BoundingBox& obstacle,
        DirectX::XMFLOAT3& pushDir,
        float& pushDepth);


    DirectX::XMFLOAT3 ResolveGroundCollision(
        const BoundingBox& playerBox,
        const DirectX::XMFLOAT3& desiredPos,
        const std::vector<BoundingBox>& colliders);


    void AdjustVelocityAfterCollision(
        DirectX::XMFLOAT3& velocity,
        const DirectX::XMFLOAT3& originalPos,
        const DirectX::XMFLOAT3& resolvedPos);
};

