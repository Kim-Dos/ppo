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

void Player::ChangeLowerState(PlayerState* nextState)
{
	// nextState가 현재 상태와 동일할 경우 상태를 변경하지 않는다.
	if (nextState->GetId() == mCurrentLowerState->GetId()) {
		if (nextState != nullptr)
			delete nextState;
		return;
	}

	// nextState의 id가 범위를 벗어날 경우 현재 상태를 유지한다.
	if ((UINT)nextState->GetId() < 0 || (UINT)nextState->GetId() >= (UINT)StateId::Count) {
		if (nextState != nullptr)
			delete nextState;
		return;
	}

	mCurrentLowerState->Exit(*this);
	delete mCurrentLowerState;
	mCurrentLowerState = nextState;
	mCurrentLowerState->Enter(*this);
}

void Player::ChangeUpperState(PlayerState* nextState)
{
	// nextState가 현재 상태와 동일할 경우 상태를 변경하지 않는다.
	if (nextState->GetId() == mCurrentUpperState->GetId()) {
		if (nextState != nullptr)
			delete nextState;
		return;
	}

	// nextState의 id가 범위를 벗어날 경우 현재 상태를 유지한다.
	if ((UINT)nextState->GetId() < 0 || (UINT)nextState->GetId() >= (UINT)StateId::Count) {
		if (nextState != nullptr)
			delete nextState;
		return;
	}

	mCurrentUpperState->Exit(*this);
	delete mCurrentUpperState;
	mCurrentUpperState = nextState;
	mCurrentUpperState->Enter(*this);
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
	if (mCamera) {
		delete mCamera;
		mCamera = nullptr;
	}

	if (mCurrentLowerState) {
		delete mCurrentLowerState;
		mCurrentLowerState = nullptr;
	}
		
	if (mCurrentUpperState) {
		delete mCurrentUpperState;
		mCurrentUpperState = nullptr;
	}
}

void Player::Update(const GameTimer& gt)
{
	float deltaTime = gt.DeltaTime();

	HandleInput();

	mCurrentLowerState->Update(*this, deltaTime);
	mCurrentUpperState->Update(*this, deltaTime);

	Move(deltaTime);

	// 카메라 이동
	UpdateCamera();

	if (mWeapon) {
		XMFLOAT4X4 rightHandMat = dynamic_cast<SkinnedMesh*>(GetMesh())->GetRightHandMatrix();
		XMFLOAT4X4 swordMat;
		XMStoreFloat4x4(&swordMat, XMLoadFloat4x4(&mWeaponOffsetMat) * XMLoadFloat4x4(&rightHandMat) * XMLoadFloat4x4(&GetWorld()));
		mWeapon->SetWorldMat(swordMat);
	}

	SetFrameDirty();
}

void Player::Move(const float deltaTime)
{
	// 추락
	mVelocity.y -= mGravity * deltaTime;
	float fallSpeed = sqrt(mVelocity.y * mVelocity.y);
	if ((mVelocity.y * mVelocity.y) > (mMaxVelocityFalling * mMaxVelocityFalling) && mVelocity.y < 0) {
		mVelocity.y = -mMaxVelocityFalling;
	}

	float maxVelocityXZ = (GetLowerStateId() == StateId::Run) ? mMaxVelocityRun : mMaxVelocityWalk;
	float groundSpeed = sqrt(mVelocity.x * mVelocity.x + mVelocity.z * mVelocity.z);
	if (groundSpeed > maxVelocityXZ) {
		mVelocity.x *= maxVelocityXZ / groundSpeed;
		mVelocity.z *= maxVelocityXZ / groundSpeed;
	}

	// 최대 속도 제한
	// 마찰
	XMFLOAT3 friction;
	XMStoreFloat3(&friction, -XMVector3Normalize(XMVectorSet(mVelocity.x, 0.0f, mVelocity.z, 0.0f)) * mFriction * deltaTime);
	mVelocity.x = (mVelocity.x >= 0.0f) ? max(0.0f, mVelocity.x + friction.x) : min(0.0f, mVelocity.x + friction.x);
	mVelocity.z = (mVelocity.z >= 0.0f) ? max(0.0f, mVelocity.z + friction.z) : min(0.0f, mVelocity.z + friction.z);

	// 위치 변환
	SetPosition(Vector3::Add(GetPosition(), Vector3::ScalarProduct(mVelocity, deltaTime, false)));
}

void Player::Jump()
{
	mIsFalling = true;
	mVelocity.y += mJumpForce;
}

void Player::HandleInput()
{
	mCurrentLowerState->HandleInput(*this, mKeyInput);
	mCurrentUpperState->HandleInput(*this, mKeyInput);
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

void Player::KeyboardInput(float dt)
{
	/*
	mIsRun = false;
	if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
		mIsRun = true;

	XMFLOAT3 velocity;

	if (mIsFalling)
		velocity = Vector3::ScalarProduct(mAcceleration, dt / 2, false);
	else if (mIsRun)
		velocity = Vector3::ScalarProduct(mAcceleration, dt * 2, false);
	else
		velocity = Vector3::ScalarProduct(mAcceleration, dt, false);
	

	if (GetAsyncKeyState('W') & 0x8000)
		mVelocity = Vector3::Add(mVelocity, MultipleVelocity(GetLook(), velocity));

	if (GetAsyncKeyState('S') & 0x8000)
		mVelocity = Vector3::Add(mVelocity, MultipleVelocity(Vector3::ScalarProduct(GetLook(), -1), velocity));

	if (GetAsyncKeyState('D') & 0x8000)
		mVelocity = Vector3::Add(mVelocity, MultipleVelocity(GetRight(), velocity));

	if (GetAsyncKeyState('A') & 0x8000)
		mVelocity = Vector3::Add(mVelocity, MultipleVelocity(Vector3::ScalarProduct(GetRight(), -1), velocity));
	*/
}

void Player::OnKeyboardMessage(UINT nMessageID, WPARAM wParam) 
{
	switch (nMessageID)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case 'W':
			mKeyInput.isPressedW = true;
			break;
		case 'A':
			mKeyInput.isPressedA = true;
			break;
		case 'S':
			mKeyInput.isPressedS = true;
			break;
		case 'D':
			mKeyInput.isPressedD = true;
			break;
		case 'F':
			mKeyInput.isPressedF = true;
			break;
		case VK_SHIFT:
			mKeyInput.isPressedShift = true;
			break;
		case VK_SPACE:
			mKeyInput.isPressedSpaceBar = true;
			break;
		}
		break;
	case WM_KEYUP:
		switch (wParam)
		{
		case 'W':
			mKeyInput.isPressedW = false;
			break;
		case 'A':
			mKeyInput.isPressedA = false;
			break;
		case 'S':
			mKeyInput.isPressedS = false;
			break;
		case 'D':
			mKeyInput.isPressedD = false;
			break;
		case 'F':
			mKeyInput.isPressedF = false;
			break;
		case VK_SHIFT:
			mKeyInput.isPressedShift = false;
			break;
		case VK_SPACE:
			mKeyInput.isPressedSpaceBar = false;
			break;
		}
		break;
	}
}

void Player::MouseInput(float dx, float dy)
{
	Rotate(0.f, dx, 0.f);

	float maxPitchRaidan = XMConvertToRadians(MAX_PLAYER_CAMERA_PITCH);
	mPitch += dy;
	mPitch = (mPitch < -maxPitchRaidan) ? -maxPitchRaidan : (maxPitchRaidan < mPitch) ? maxPitchRaidan : mPitch;
}

vector<string> Player::GetAnimationName()
{
	vector<string> lowerAnimationName = mCurrentLowerState->GetAnimationName();
	vector<string> upperAnimationName = mCurrentUpperState->GetAnimationName();

	if (upperAnimationName[0] == "Idle") {
		return lowerAnimationName;
	}
	else {
		return upperAnimationName;
	}

	return mCurrentLowerState->GetAnimationName();
}

void Player::InitPlayer()
{
	// Set Camera
	mCamera = new Camera();

	mCameraOffsetPosition = XMFLOAT3(0.0f, 100.0f, 20.0f);
	UpdateCamera();

	mCurrentLowerState = new IdlePlayerState;
	mCurrentLowerState->Enter(*this);
	mCurrentUpperState = new IdleAttackPlayerState;
	mCurrentUpperState->Enter(*this);

	XMStoreFloat4x4(&mWeaponOffsetMat, XMMatrixRotationRollPitchYaw(XMConvertToRadians(-10.0f), XMConvertToRadians(90.0f), XMConvertToRadians(0.0f)) * XMMatrixTranslation(-10.0f, 10.0f, -2.0f));
}

void OnGroundPlayerState::HandleInput(Player& player, KeyInput keyInput)
{
	moveX = 0, moveY = 0;
	// 이동 처리
	if (keyInput.isPressedW) {
		moveY++;
	}
	if (keyInput.isPressedS) {
		moveY--;
	}
	if (keyInput.isPressedA) {
		moveX--;
	}
	if (keyInput.isPressedD) {
		moveX++;
	}

	if (keyInput.isPressedSpaceBar) {
		if (!player.IsFalling() && player.GetLowerStateId() != StateId::Jump) {
			player.ChangeLowerState(new JumpPlayerState);
		}
	}
	else if (moveX != 0 || moveY != 0) {
		if (moveY == 1 && keyInput.isPressedShift) {
			if (player.GetLowerStateId() != StateId::Run) {
				animationTime = 0.0f;
				player.ChangeLowerState(new RunPlayerState);
			}
		}
		else {
			if (player.GetLowerStateId() != StateId::Walk) {
				animationTime = 0.0f;
				player.ChangeLowerState(new WalkPlayerState);
			}
		}
	}
	else {
		if (player.GetLowerStateId() != StateId::Idle && player.GetLowerStateId() != StateId::Land)
			player.ChangeLowerState(new IdlePlayerState);
	}
}

void OnGroundPlayerState::Update(Player& player, const float deltaTime)
{
	PlayerState::Update(player, deltaTime);

	float velocity;
	if (player.GetLowerStateId() == StateId::Run) {
		velocity = 2 * player.GetAcc() * deltaTime;
	}
	else {
		velocity = player.GetAcc() * deltaTime;
	}

	XMFLOAT3 movementDir;
	XMStoreFloat3(&movementDir, XMVector3Normalize((XMLoadFloat3(&player.GetLook()) * moveY) + (XMLoadFloat3(&player.GetRight()) * moveX)));

	XMFLOAT3 newVelocity = Vector3::Add(player.GetVelocity(), Vector3::ScalarProduct(movementDir, velocity, false));
	player.SetVelocity(newVelocity);
}

void OnAirPlayerState::HandleInput(Player& player, KeyInput keyInput)
{
	moveX = 0, moveY = 0;
	if (keyInput.isPressedW) {
		moveY++;
	}
	if (keyInput.isPressedS) {
		moveY--;
	}
	if (keyInput.isPressedA) {
		moveX--;
	}
	if (keyInput.isPressedD) {
		moveX++;
	}
}

void OnAirPlayerState::Update(Player& player, const float deltaTime)
{
	PlayerState::Update(player, deltaTime);

	float velocity = player.GetAcc() * deltaTime;

	XMFLOAT3 movementDir;
	XMStoreFloat3(&movementDir, XMVector3Normalize((XMLoadFloat3(&player.GetLook()) * moveY) + (XMLoadFloat3(&player.GetRight()) * moveX)));

	XMFLOAT3 newVelocity = Vector3::Add(player.GetVelocity(), Vector3::ScalarProduct(movementDir, velocity, false));
	player.SetVelocity(newVelocity);

	if (!player.IsFalling()) {
		player.ChangeLowerState(new LandingPlayerState);
	}
}

void JumpPlayerState::Update(Player& player, const float deltaTime)
{
	OnAirPlayerState::Update(player, deltaTime);

	float animationDuration;
	Mesh* mesh = dynamic_cast<SkinnedMesh*>(player.GetMesh());
	if (mesh) {
		animationDuration = mesh->GetAnimationDuration("Jump");
	}
	else {
		animationDuration = 0.0f;
	}

	if (animationTime > animationDuration) {
		animationTime = 0.0f;
		player.ChangeLowerState(new FallingPlayerState);
	}
}

void JumpPlayerState::Enter(Player& player)
{
	animationTime = 0.0f;
	player.Jump();
}

void LandingPlayerState::Update(Player& player, const float deltaTime)
{
	OnGroundPlayerState::Update(player, deltaTime);

	float animationDuration;
	Mesh* mesh = dynamic_cast<SkinnedMesh*>(player.GetMesh());
	if (mesh) {
		animationDuration = mesh->GetAnimationDuration("Landing");
	}
	else {
		animationDuration = 0.0f;
	}

	if (animationTime > animationDuration - 0.1f) {
		animationTime = 0.0f;
		player.ChangeLowerState(new IdlePlayerState);
	}
}

void AttackPlayerState::HandleInput(Player& player, KeyInput keyInput)
{
	if (keyInput.isPressedF) {
		if (player.GetUpperStateId() != StateId::MeleeAttack) {
			animationTime = 0.0f;
			player.SetAttacking(true);
			player.ChangeUpperState(new MeleeAttackPlayerState);
		}
	}
}

void AttackPlayerState::Update(Player& player, const float deltaTime)
{
	PlayerState::Update(player, deltaTime);
}

void MeleeAttackPlayerState::Update(Player& player, const float deltaTime)
{
	AttackPlayerState::Update(player, deltaTime);

	float animationDuration;
	Mesh* mesh = dynamic_cast<SkinnedMesh*>(player.GetMesh());
	if (mesh) {
		animationDuration = mesh->GetAnimationDuration("MeleeAttack1");
	}
	else {
		animationDuration = 0.0f;
	}

	if (animationTime > animationDuration - 0.1f) {
		animationTime = 0.f;
		player.SetAttacking(false);
		player.ChangeUpperState(new IdleAttackPlayerState);
	}
}
