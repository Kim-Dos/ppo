//***************************************************************************************
// MathHelper.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "MathHelper.h"
#include <float.h>
#include <cmath>

using namespace DirectX;

const float MathHelper::Infinity = FLT_MAX;
const float MathHelper::Pi       = 3.1415926535f;

bool MathHelper::IntersectRayAABB( XMVECTOR rayOrigin,  XMVECTOR rayDirection, XMFLOAT3 boxCenter, XMFLOAT3 boxExtents, float& distance)
{
	XMVECTOR center = XMLoadFloat3(&boxCenter);
	XMVECTOR extents = XMLoadFloat3(&boxExtents);

	// 레이를 박스 로컬 공간으로 변환
	XMVECTOR localOrigin = rayOrigin - center;

	// 각 축별로 슬랩 교차 검사
	float tmin = -FLT_MAX;
	float tmax = FLT_MAX;

	// X축 검사
	float e = XMVectorGetX(localOrigin);
	float f = XMVectorGetX(rayDirection);

	if (fabs(f) > 0.001f)
	{
		float t1 = (e + XMVectorGetX(extents)) / -f;
		float t2 = (e - XMVectorGetX(extents)) / -f;

		tmin = max(tmin, min(t1, t2));
		tmax = min(tmax, max(t1, t2));
	}
	else if (abs(e) > XMVectorGetX(extents))
	{
		return false;
	}

	// Y축 검사
	e = XMVectorGetY(localOrigin);
	f = XMVectorGetY(rayDirection);

	if (fabs(f) > 0.001f)
	{
		float t1 = (e + XMVectorGetY(extents)) / -f;
		float t2 = (e - XMVectorGetY(extents)) / -f;

		tmin = max(tmin, min(t1, t2));
		tmax = min(tmax, max(t1, t2));
	}
	else if (abs(e) > XMVectorGetY(extents))
	{
		return false;
	}

	// Z축 검사
	e = XMVectorGetZ(localOrigin);
	f = XMVectorGetZ(rayDirection);

	if (fabs(f) > 0.001f)
	{
		float t1 = (e + XMVectorGetZ(extents)) / -f;
		float t2 = (e - XMVectorGetZ(extents)) / -f;

		tmin = max(tmin, min(t1, t2));
		tmax = min(tmax, max(t1, t2));
	}
	else if (abs(e) > XMVectorGetZ(extents))
	{
		return false;
	}

	// 교차점이 레이의 양의 방향에 있는지 확인
	if (tmax < 0 || tmin > tmax)
	{
		return false;
	}

	distance = tmin < 0 ? tmax : tmin;

	return true;
}

XMVECTOR MathHelper::ScreenToWorld(int screenX, int screenY, int screenWidth, int screenHeight, const XMMATRIX& viewMatrix, const XMMATRIX& projMatrix)
{
	float ndcX = (2.0f * screenX) / screenWidth - 1.0f;
	float ndcY = 1.0f - (2.0f * screenY) / screenHeight;

	// near plane과 살짝 더 먼 거리만 사용
	XMVECTOR rayOrigin = XMVectorSet(ndcX, ndcY, 0.1f, 1.0f);    // near plane
	XMVECTOR rayTarget = XMVectorSet(ndcX, ndcY, 20000.f, 1.0f);    // 1.0f에서 0.1f로 변경

	XMMATRIX invViewProj = XMMatrixInverse(nullptr, XMMatrixMultiply(viewMatrix, projMatrix));
	XMVECTOR worldRayOrigin = XMVector3TransformCoord(rayOrigin, invViewProj);
	XMVECTOR worldRayTarget = XMVector3TransformCoord(rayTarget, invViewProj);

	XMVECTOR rayDirection = XMVector3Normalize(XMVectorSubtract(worldRayTarget, worldRayOrigin));

	float dirY = XMVectorGetY(rayDirection);
	if (abs(dirY) < 0.0001f)
	{
		return XMVectorSet(
			XMVectorGetX(worldRayOrigin),
			0.0f,
			XMVectorGetZ(worldRayOrigin),
			1.0f
		);
	}

	float t = -XMVectorGetY(worldRayOrigin) / dirY;
	XMVECTOR intersectionPoint = XMVectorAdd(
		worldRayOrigin,
		XMVectorScale(rayDirection, t)
	);

	return intersectionPoint;
}

float MathHelper::AngleFromXY(float x, float y)
{
	float theta = 0.0f;
 
	// Quadrant I or IV
	if(x >= 0.0f) 
	{
		// If x = 0, then atanf(y/x) = +pi/2 if y > 0
		//                atanf(y/x) = -pi/2 if y < 0
		theta = atanf(y / x); // in [-pi/2, +pi/2]

		if(theta < 0.0f)
			theta += 2.0f*Pi; // in [0, 2*pi).
	}

	// Quadrant II or III
	else      
		theta = atanf(y/x) + Pi; // in [0, 2*pi).

	return theta;
}

XMVECTOR MathHelper::RandUnitVec3()
{
	XMVECTOR One  = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR Zero = XMVectorZero();

	// Keep trying until we get a point on/in the hemisphere.
	while(true)
	{
		// Generate random point in the cube [-1,1]^3.
		XMVECTOR v = XMVectorSet(MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), 0.0f);

		// Ignore points outside the unit sphere in order to get an even distribution 
		// over the unit sphere.  Otherwise points will clump more on the sphere near 
		// the corners of the cube.

		if( XMVector3Greater( XMVector3LengthSq(v), One) )
			continue;

		return XMVector3Normalize(v);
	}
}

XMVECTOR MathHelper::RandHemisphereUnitVec3(XMVECTOR n)
{
	XMVECTOR One  = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR Zero = XMVectorZero();

	// Keep trying until we get a point on/in the hemisphere.
	while(true)
	{
		// Generate random point in the cube [-1,1]^3.
		XMVECTOR v = XMVectorSet(MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), 0.0f);

		// Ignore points outside the unit sphere in order to get an even distribution 
		// over the unit sphere.  Otherwise points will clump more on the sphere near 
		// the corners of the cube.
		
		if( XMVector3Greater( XMVector3LengthSq(v), One) )
			continue;

		// Ignore points in the bottom hemisphere.
		if( XMVector3Less( XMVector3Dot(n, v), Zero ) )
			continue;

		return XMVector3Normalize(v);
	}
}


