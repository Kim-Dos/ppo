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
	//printf("하이요\n");
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

	// 최대 속도 제한
	float maxVelocityXZ = (GetLowerStateId() == StateId::Run) ? mMaxVelocityRun : mMaxVelocityWalk;
	float groundSpeed = sqrt(mVelocity.x * mVelocity.x + mVelocity.z * mVelocity.z);
	if (groundSpeed > maxVelocityXZ) {
		mVelocity.x *= maxVelocityXZ / groundSpeed;
		mVelocity.z *= maxVelocityXZ / groundSpeed;
	}

	
	// 마찰
	XMFLOAT3 friction;
	XMStoreFloat3(&friction, -XMVector3Normalize(XMVectorSet(mVelocity.x, 0.0f, mVelocity.z, 0.0f)) * mFriction * deltaTime);
	mVelocity.x = (mVelocity.x >= 0.0f) ? max(0.0f, mVelocity.x + friction.x) : min(0.0f, mVelocity.x + friction.x);
	//mVelocity.y = (mVelocity.y >= 0.0f) ? max(0.0f, mVelocity.y + friction.z) : min(0.0f, mVelocity.y + friction.z);
	mVelocity.z = (mVelocity.z >= 0.0f) ? max(0.0f, mVelocity.z + friction.z) : min(0.0f, mVelocity.z + friction.z);

	// 위치 변환
	SetPosition(Vector3::Add(GetPosition(), Vector3::ScalarProduct(mVelocity, deltaTime, false)));

	if (GetPosition().y < 0) {
		SetPosition(GetPosition().x, 0.0f, GetPosition().z);
		mVelocity.y = 0.0f;
		mIsFalling = false;
	}
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
	XMVECTOR cameraPosition = playerPosition - cameraLook * cam_dist; // distance는 카메라와 플레이어 사이의 거리

	//XMMATRIX viewMatrix = XMMatrixLookAtLH(cameraPosition, playerPosition, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	mCamera->LookAt(cameraPosition, playerPosition, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

	mCamera->UpdateViewMatrix();
}

void Player::OnKeyboardMessage(UINT nMessageID, WPARAM wParam) 
{
	switch (nMessageID)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case 'W':
		case 'w':
			mKeyInput.isPressedW = true;
			break;
		case 'A':
		case 'a':
			mKeyInput.isPressedA = true;
			break;
		case 'S':
		case 's':
			mKeyInput.isPressedS = true;
			break;
		case 'D':
		case 'd':
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
		case 'Q':
		case 'q':
			this->MoveUp(-500);
			break;
		case 'E':
		case 'e':
			this->MoveUp(500);
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
	float yaw = dx;
	Rotate(0.f, dx, 0.f);

	float maxPitchRaidan = XMConvertToRadians(MAX_PLAYER_CAMERA_PITCH);
	mPitch += dy;
	mPitch = (mPitch < -maxPitchRaidan) ? -maxPitchRaidan : (maxPitchRaidan < mPitch) ? maxPitchRaidan : mPitch;
}

void Player::WheelInput(WPARAM wParam)
{
	int delta = GET_WHEEL_DELTA_WPARAM(wParam); 
	if (delta < 0) { 
		if(cam_dist < 1000)
		cam_dist += 50.f; }
	else { 
		if (cam_dist > 50)
		cam_dist -= 50.f; }
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
		player.ChangeUpperState(new IdleAttackPlayerState);
	}
}




void TPPlayer::InitPlayer()
{

	mCamera = new Camera();

	mCameraOffsetPosition = XMFLOAT3(0.0f, 1000.0f, 20.0f);
	UpdateCamera();

}

TPPlayer::TPPlayer() :
	GameObject()
{
	InitPlayer();
}

TPPlayer::TPPlayer(const string name, XMMATRIX world) : GameObject()
{
	this->SetName(name);
	XMFLOAT4X4 temp;
	XMStoreFloat4x4(&temp, world);
	this->SetWorldMat(temp);
	InitPlayer();
}

TPPlayer::TPPlayer(const string name, XMFLOAT4X4 world, XMFLOAT4X4 texTransform) :
	GameObject(name, world, texTransform)
{
	InitPlayer();
}

TPPlayer::TPPlayer(const string name, XMMATRIX world, XMMATRIX texTransform) :
	GameObject(name, world, texTransform)
{
	InitPlayer();
}

TPPlayer::~TPPlayer()
{
	if (mCamera) {
		delete mCamera;
		mCamera = nullptr;
	}
	
}

void TPPlayer::Update(const GameTimer& gt)
{
	float deltaTime = gt.DeltaTime();

	Move(deltaTime);
	printf("Se\n");
	UpdateCamera();
	SetFrameDirty();
}

void TPPlayer::Move(const float deltaTime)
{

	float velocity = this->GetAcc()* deltaTime;

	XMFLOAT3 movementDir;
	int x = 0, y = 0, z = 0;
	if (mKeyInput.isPressedW) x = 1;
	if (mKeyInput.isPressedS) x = -1;
	if (mKeyInput.isPressedA) z = 1;
	if (mKeyInput.isPressedD) z = -1;
	if (mKeyInput.isPressedQ) y = 1;
	if (mKeyInput.isPressedE) y = -1;


	XMStoreFloat3(&movementDir, XMVector3Normalize((XMLoadFloat3(&this->GetLook()) * x ) + (XMLoadFloat3(&this->GetRight()) * y ) + (XMLoadFloat3(&this->GetUp())* z )));

	XMFLOAT3 newVelocity = Vector3::Add(this->GetVelocity(), Vector3::ScalarProduct(movementDir, velocity, false));
	this->SetVelocity(newVelocity);

	// 최대 속도 제한
	float maxVelocity = 1000;
	float groundSpeed = sqrt(mVelocity.x * mVelocity.x + mVelocity.y * mVelocity.y + mVelocity.z * mVelocity.z);
	if (groundSpeed > maxVelocity) {
		mVelocity.x *= maxVelocity / groundSpeed;
		mVelocity.y *= maxVelocity / groundSpeed;
		mVelocity.z *= maxVelocity / groundSpeed;
	}

	
	// 마찰
	XMFLOAT3 friction;
	XMStoreFloat3(&friction, -XMVector3Normalize(XMVectorSet(mVelocity.x, mVelocity.y, mVelocity.z, 0.0f)) * mFriction * deltaTime);
	mVelocity.x = (mVelocity.x >= 0.0f) ? max(0.0f, mVelocity.x + friction.x) : min(0.0f, mVelocity.x + friction.x);
	mVelocity.y = (mVelocity.y >= 0.0f) ? max(0.0f, mVelocity.y + friction.z) : min(0.0f, mVelocity.y + friction.z);
	mVelocity.z = (mVelocity.z >= 0.0f) ? max(0.0f, mVelocity.z + friction.z) : min(0.0f, mVelocity.z + friction.z);

	// 위치 변환
	SetPosition(Vector3::Add(GetPosition(), Vector3::ScalarProduct(mVelocity, deltaTime, false)));
}

void TPPlayer::UpdateCamera()
{
	XMVECTOR cameraLook = XMVector3TransformNormal(XMLoadFloat3(&GetLook()), XMMatrixRotationAxis(XMLoadFloat3(&GetRight()), mPitch));

	//XMVECTOR playerPosition = XMLoadFloat3(&GetPosition()) + XMLoadFloat3(&mCameraOffsetPosition);
	//XMVECTOR cameraPosition = playerPosition - cameraLook * 500.f; // distance는 카메라와 플레이어 사이의 거리

	//XMMATRIX viewMatrix = XMMatrixLookAtLH(cameraPosition, playerPosition, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	XMVECTOR temp = XMLoadFloat3(&this->GetPosition());
	mCamera->LookAt(temp, cameraLook, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	mCamera->UpdateViewMatrix();
}



void TPPlayer::OnKeyboardMessage(UINT nMessageID, WPARAM wParam)
{
	switch (nMessageID)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case 'W':
		case 'w':
			mKeyInput.isPressedW = true;
			//this->MoveForward(500);
			break;
		case 'A':
		case 'a':
			mKeyInput.isPressedA = true;
			//this->MoveStrafe(-500);
			break;
		case 'S':
		case 's':
			mKeyInput.isPressedS = true;
			//this->MoveForward(-500);
			break;
		case 'D':
		case 'd':
			//this->MoveStrafe(500);
			mKeyInput.isPressedD = true;
			break;
		case 'F':
			mKeyInput.isPressedF = true;
			break;
		case VK_SHIFT:
			mKeyInput.isPressedShift = true;
			break;
		case 'Q':
		case 'q':
			mKeyInput.isPressedQ = true;
			//this->MoveUp(-500);
			break;
		case 'E':
		case 'e':
			mKeyInput.isPressedE = true;
			//this->MoveUp(500);
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
		case 'Q':
		case 'q':
			mKeyInput.isPressedQ = false;
			//this->MoveUp(-500);
			break;
		case 'E':
		case 'e':
			mKeyInput.isPressedE = false;
			//this->MoveUp(500);
			break;
		}
		break;
	}

}

void TPPlayer::MouseInput(float dx, float dy)
{
	mCamera->Pitch(dx);
	mCamera->RotateY(dy);

	//float maxPitchRaidan = XMConvertToRadians(MAX_PLAYER_CAMERA_PITCH);
	//mPitch += dy;
	//mPitch = (mPitch < -maxPitchRaidan) ? -maxPitchRaidan : (maxPitchRaidan < mPitch) ? maxPitchRaidan : mPitch;
}

void TPPlayer::WheelInput(WPARAM wParam)
{
	int delta = GET_WHEEL_DELTA_WPARAM(wParam);
	if (delta < 0) {
		//mCamera->SetPosition();
		mCamera->GetLook();
	}
	else {
		this->MoveForward(-50.f);
	}
}
