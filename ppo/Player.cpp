#include "Player.h"

XMFLOAT3 MultipleVelocity(const XMFLOAT3& dir, const XMFLOAT3& scalar)
{
	return XMFLOAT3(dir.x * scalar.x, dir.y * scalar.y, dir.z * scalar.z);
}

Player::Player(const string name, XMFLOAT4X4 world, XMFLOAT4X4 texTransform) : 
    GameObject(name, world, texTransform)
{
	InitPlayer();
}

Player::Player(const string name, XMMATRIX world, XMMATRIX texTransform) :
    GameObject(name, world, texTransform)
{
	InitPlayer();
}

Player::~Player()
{
	if (mCamera)
		delete mCamera;
}

void Player::Update(const GameTimer& gt)
{
	// 이동
	SetPosition(Vector3::Add(GetPosition(), mVelocity));
	// 마찰
	mVelocity = Vector3::ScalarProduct(mVelocity, pow(mFriction,gt.DeltaTime()), false);

	// 카메라 이동
	UpdateCamera();

	mCamera->GetLook3f();

	SetFrameDirty();
}

void Player::UpdateCamera()
{
	mCamera->SetPosition(GetPosition());
	mCamera->UpdateViewMatrix();
}

void Player::KeyInput(float dt)
{
	XMFLOAT3 velocity = Vector3::ScalarProduct(mAcceleration, dt, false);

	if (GetAsyncKeyState('W') & 0x8000)
		mVelocity = Vector3::Add(mVelocity, MultipleVelocity(GetLook(), velocity));

	if (GetAsyncKeyState('S') & 0x8000)
		mVelocity = Vector3::Add(mVelocity, MultipleVelocity(Vector3::ScalarProduct(GetLook(), -1), velocity));

	if (GetAsyncKeyState('D') & 0x8000)
		mVelocity = Vector3::Add(mVelocity, MultipleVelocity(GetRight(), velocity));

	if (GetAsyncKeyState('A') & 0x8000)
		mVelocity = Vector3::Add(mVelocity, MultipleVelocity(Vector3::ScalarProduct(GetRight(), -1), velocity));

	if (GetAsyncKeyState(VK_SPACE) & 0x8000)
		;

}

void Player::InitPlayer()
{
	// Set Camera
	mCamera = new Camera();
	mCamera->SetOffset(XMFLOAT3(0.0f, 30.0f, -100.0f), 0.0f, 0.0f, 0.0f);

	mCamera->SetPosition(GetPosition());
}

