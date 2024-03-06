#include "Player.h"

Player::Player(const string name, XMFLOAT4X4 world, XMFLOAT4X4 texTransform) : 
    GameObject(name, world, texTransform)
{
}

Player::Player(const string name, XMMATRIX world, XMMATRIX texTransform) :
    GameObject(name, world, texTransform)
{
}

void Player::Update(const GameTimer& gt)
{
}

void Player::KeyInput(float dt)
{
	if (GetAsyncKeyState(VK_UP) & 0x8000)
		MoveFront(20.0f * dt);

	if (GetAsyncKeyState(VK_DOWN) & 0x8000)
		MoveFront(-20.0f * dt);

	if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
		MoveStrafe(20.0f * dt);

	if (GetAsyncKeyState(VK_LEFT) & 0x8000)
		MoveStrafe(-20.0f * dt);

	SetFrameDirty();
}

