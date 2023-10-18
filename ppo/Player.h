#pragma once

#include "Camera.h"

class Player
{
	XMFLOAT3					m_xmf3Position	= XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3					m_xmf3Right		= XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3					m_xmf3Up		= XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMFLOAT3					m_xmf3Look		= XMFLOAT3(0.0f, 0.0f, 1.0f);

	XMFLOAT3					m_xmf3Velocity	= XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3     				m_xmf3Gravity	= XMFLOAT3(0.0f, 0.0f, 0.0f);

	float						m_fPitch = 0.0f;
	float           			m_fYaw		= 0.0f;
	float           			m_fRoll		= 0.0f;

	float           			m_fMaxVelocityXZ = 0.0f;
	float           			m_fMaxVelocityY = 0.0f;
	float           			m_fFriction = 0.0f;

	LPVOID						m_pPlayerUpdatedContext = NULL;
	LPVOID						m_pCameraUpdatedContext = NULL;

	Camera* mCamera = nullptr;
};

class Bullet
{
    XMFLOAT3 mPos;
    XMFLOAT3 mDir;
};