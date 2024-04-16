#include "Player.h"

XMFLOAT3 MultipleVelocity(const XMFLOAT3& dir, const XMFLOAT3& scalar)
{
	return XMFLOAT3(dir.x * scalar.x, dir.y * scalar.y, dir.z * scalar.z);
}

Player::Player() :
	GameObject()
{
	InitPlayer();
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
	// 최대 속도 제한
	float groundSpeed = sqrt(mVelocity.x * mVelocity.x + mVelocity.z * mVelocity.z);
	if (groundSpeed > mMaxWalkVelocityXZ) {
		mVelocity.x *= mMaxWalkVelocityXZ / groundSpeed;
		mVelocity.z *= mMaxWalkVelocityXZ / groundSpeed;
	}
	float fallSpeed = sqrt(mVelocity.y * mVelocity.y);
	if ((mVelocity.y * mVelocity.y) > (mMaxVelocityY * mMaxVelocityY) && mVelocity.y < 0) {
		mVelocity.y = -mMaxVelocityY;
	}

	// 마찰
	XMFLOAT3 friction;
	XMStoreFloat3(&friction, -XMVector3Normalize(XMVectorSet(mVelocity.x, 0.0f, mVelocity.z, 0.0f)) * mFriction * gt.DeltaTime());
	mVelocity.x = (mVelocity.x >= 0.0f) ? max(0.0f, mVelocity.x + friction.x) : min(0.0f, mVelocity.x + friction.x);
	mVelocity.z = (mVelocity.z >= 0.0f) ? max(0.0f, mVelocity.z + friction.z) : min(0.0f, mVelocity.z + friction.z);

	SetPosition(Vector3::Add(GetPosition(), mVelocity));

	// 카메라 이동
	UpdateCamera();
	UpdateState(gt);

	SetFrameDirty();
}

void Player::UpdateCamera()
{
	XMVECTOR cameraLook = XMVector3TransformNormal(XMLoadFloat3(&GetLook()), XMMatrixRotationAxis(XMLoadFloat3(&GetRight()), mPitch));

	XMVECTOR playerPosition = XMLoadFloat3(&GetPosition()) + XMLoadFloat3(&mCameraOffsetPosition);
	XMVECTOR cameraPosition = playerPosition - cameraLook * 500.f; // distance는 카메라와 플레이어 사이의 거리

	XMMATRIX viewMatrix = XMMatrixLookAtLH(cameraPosition, playerPosition, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	mCamera->LookAt(cameraPosition, playerPosition, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

	mCamera->UpdateViewMatrix();
}

void Player::UpdateState(const GameTimer& gt)
{
	/*
	if (GetPosition().y > 0)
		if (GetStateId() != (UINT)StateId::Jump)
			mState = (UINT)PlayerState::Fall;
	*/
	float groundSpeed = sqrt(mVelocity.x * mVelocity.x + mVelocity.z * mVelocity.z);
	if (groundSpeed <= 0.1f) {
		mFSM.ChangeState(PlayerIdleState(this));
	}
	else {
		mFSM.ChangeState(PlayerWalkState(this));
	}

	mFSM.UpdateState(gt);
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

void Player::MouseInput(float dx, float dy)
{
	Rotate(0.f, dx, 0.f);

	float maxPitchRaidan = XMConvertToRadians(MAX_PLAYER_CAMERA_PITCH);
	mPitch += dy;
	mPitch = (mPitch < -maxPitchRaidan) ? -maxPitchRaidan : (maxPitchRaidan < mPitch) ? maxPitchRaidan : mPitch;
}

void Player::InitPlayer()
{
	// Set Camera
	mCamera = new Camera();

	mCameraOffsetPosition = XMFLOAT3(0.0f, 100.0f, 0.0f);
	UpdateCamera();

	for (int i = 0; i < 6; i++)
		mAnimationIndex[i] = i - 1;

	mFSM = FSM(PlayerIdleState(this));
}

