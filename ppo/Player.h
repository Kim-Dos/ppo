#pragma once

#include "Camera.h"
#include "GameObject.h"

class Player : public GameObject
{
public:
	Player() {}
	Player(const string name, XMFLOAT4X4 world, XMFLOAT4X4 texTransform);
	Player(const string name, XMMATRIX world, XMMATRIX texTransform);
	~Player();

	virtual void Update(const GameTimer& gt);
	void UpdateCamera();

	void KeyInput(float dt);

	Camera* GetCamera() { return mCamera; }

private:
	void InitPlayer();

	XMFLOAT3 mVelocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 mAcceleration = XMFLOAT3(10.0f, 10.0f, 10.0f);

	float mMaxVelocityXZ = 10.0f;
	float mMaxVelocityY = 10.0f;
	float mFriction = 0.01f;

	Camera* mCamera = nullptr;
};

class Bullet
{
    XMFLOAT3 mPos;
    XMFLOAT3 mDir;
};
