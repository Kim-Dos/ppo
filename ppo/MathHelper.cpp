//***************************************************************************************
// MathHelper.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "MathHelper.h"
#include <float.h>
#include <cmath>

using namespace DirectX;

const float MathHelper::Infinity = FLT_MAX;
const float MathHelper::Pi       = 3.1415926535f;

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


