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
	// ½ºÅ©¸° ÁÂÇ¥¸¦ ºäÆ÷Æ® ÁÂÇ¥·Î º¯È¯
	float vx = (2.0f * screenX / screenWidth) - 1.0f;
	float vy = 1.0f - (2.0f * screenY / screenHeight);

	// ºäÆ÷Æ® ÁÂÇ¥¸¦ Å¬¸³ ÁÂÇ¥·Î º¯È¯
	XMVECTOR clipCoords = XMVectorSet(vx, vy, 1.0f, 1.0f);

	// Å¬¸³ ÁÂÇ¥¸¦ ºä ÁÂÇ¥·Î º¯È¯
	XMMATRIX invProj = XMMatrixInverse(nullptr, projMatrix);
	XMVECTOR viewCoords = XMVector4Transform(clipCoords, invProj);
	viewCoords = XMVectorSetZ(viewCoords, 1.0f);  // Å¬¸³ ÁÂÇ¥¿¡¼­ Z¸¦ 1.0À¸·Î ¼³Á¤

	// ºä ÁÂÇ¥¸¦ ¿ùµå ÁÂÇ¥·Î º¯È¯
	XMMATRIX invView = XMMatrixInverse(nullptr, viewMatrix);
	XMVECTOR worldCoords = XMVector3TransformCoord(viewCoords, invView);

	return worldCoords;
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


