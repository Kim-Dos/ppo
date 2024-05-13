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
};

