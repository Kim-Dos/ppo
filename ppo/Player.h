#pragma once

#include "Camera.h"
#include "GameObject.h"

class Player : public GameObject
{
public:
	Player() {}
	Player(const string name, XMFLOAT4X4 world, XMFLOAT4X4 texTransform);
	Player(const string name, XMMATRIX world, XMMATRIX texTransform);
	~Player() {}

	virtual void Update(const GameTimer& gt);

	void KeyInput(float dt);

private:
	XMFLOAT3					m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3     				m_xmf3Gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float						m_fPitch = 0.0f;
	float           			m_fYaw = 0.0f;
	float           			m_fRoll = 0.0f;

	float           			m_fMaxVelocityXZ = 0.0f;
	float           			m_fMaxVelocityY = 0.0f;
	float           			m_fFriction = 0.0f;

	Camera* mCamera = nullptr;
};

class Bullet
{
    XMFLOAT3 mPos;
    XMFLOAT3 mDir;
};
