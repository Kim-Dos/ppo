#pragma once

#include "Camera.h"
#include "GameObject.h"
#include <map>

#define MAX_PLAYER_CAMERA_PITCH 85.0f

class Player;

struct KeyInput
{
	bool isPressedW = false;
	bool isPressedA = false;
	bool isPressedS = false;
	bool isPressedD = false;
	bool isPressedSpaceBar = false;
	bool isPressedShift = false;
	bool isPressedF = false;
};

namespace PlayerConstance
{
	const float MAX_VELOCITY_WALK = 1.8f;
	const float MAX_VELOCITY_RUN = 6.0f;
	const float MAX_VELOCITY_FALLING = 10.0f;
}

enum class StateId : UINT
{
	Defalut = 0,
	Idle,
	Walk,
	Run,
	Jump,
	Fall,
	Land,
	Attack1,
	Attack2,
	Count
};

class PlayerState
{
public:
	PlayerState() { id = StateId::Defalut; }
	virtual ~PlayerState() {}

	virtual void HandleInput(Player& player, KeyInput input) {}
	virtual void Enter(Player& player) {}
	virtual void Update(Player& player, const float deltaTime) {}
	virtual void Exit(Player& player) {}
	virtual vector<string> GetAnimationName() = 0;

	StateId GetId() { return id; }
protected:
	StateId id;
};

class Player : public GameObject
{
public:
	Player();
	Player(const string name, XMFLOAT4X4 world, XMFLOAT4X4 texTransform);
	Player(const string name, XMMATRIX world, XMMATRIX texTransform);
	~Player();

	virtual void Update(const GameTimer& gt);
	void HandleInput();
	void Move(const float deltaTime);
	void UpdateCamera();

	void KeyboardInput(float dt);
	void OnKeyboardMessage(UINT nMessageID, WPARAM wParam);
	void MouseInput(float dx, float dy);

	Camera* GetCamera() { return mCamera; }

	void ChangeState(PlayerState* nextState);
	StateId GetStateId() { return mCurrentState->GetId(); }

	void SetAnimationTime() { mAnimationTime = 0.0f; }
	float GetAnimationTime() { return mAnimationTime; }
	vector<string> GetAnimationName() { return mCurrentState->GetAnimationName(); }

	void SetVelocity(XMFLOAT3 velocity) { mVelocity = velocity; }
	XMFLOAT3 GetVelocity() { return mVelocity; }
	float GetAcc() { return mAcceleration; }

	bool IsFalling() { return mIsFalling; }

	const float mMaxVelocityWalk = 1.8f;
	const float mMaxVelocityRun = 6.0f;
	const float mMaxVelocityFalling = 10.0f;
private:
	void InitPlayer();

	float mPitch = 0.0f;
	
	XMFLOAT3 mVelocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float mAcceleration = 30.0f;

	float mJumpForce = 10.0f;
	float mGravity = 10.0f;
	bool mIsFalling = false;
	bool mIsAttack = false;
	float mFriction = 10.0f;

	Camera* mCamera = nullptr;
	XMFLOAT3 mCameraOffsetPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float mAnimationTime = 0.0f;

	KeyInput mKeyInput;

	PlayerState* mCurrentState = nullptr;
};

class OnGroundPlayerState : public PlayerState {
public:
	virtual void HandleInput(Player& player, KeyInput keyInput);
	virtual void Update(Player& player, const float deltaTime);
protected:
	int moveX = 0, moveY = 0;
};

class IdlePlayerState : public OnGroundPlayerState
{
public:
	IdlePlayerState() { id = StateId::Idle; }
	virtual vector<string> GetAnimationName() { return vector<string>{"Idle"}; }
};

class WalkPlayerState : public OnGroundPlayerState
{
public:
	WalkPlayerState() { id = StateId::Walk; }
	virtual vector<string> GetAnimationName()
	{
		vector<string> animationNames;
		if (moveY == 1) {
			animationNames.push_back("WalkForward");
			if (moveX == 1) {
				animationNames.push_back("WalkRight1");
			}
			else if (moveX == -1) {
				animationNames.push_back("WalkLeft1");
			}
		}
		else if (moveY == -1) {
			animationNames.push_back("WalkBack");
			if (moveX == 1) {
				animationNames.push_back("WalkRight2");
			}
			else if (moveX == -1) {
				animationNames.push_back("WalkLeft2");
			}
		}
		else {
			if (moveX == 1) {
				animationNames.push_back("WalkRight1");
			}
			else if (moveX == -1) {
				animationNames.push_back("WalkLeft1");
			}
			else {
				animationNames.push_back("Idle");
			}
		}

		return animationNames;
	}
};

class RunPlayerState : public OnGroundPlayerState
{
public:
	RunPlayerState() { id = StateId::Run; }
	virtual vector<string> GetAnimationName() 
	{ 
		vector<string> animationNames = { "RunForward" };

		if (moveX == 1) {
			animationNames.push_back("WalkRight1");
		}
		else if (moveX == -1) {
			animationNames.push_back("WalkLeft1");
		}

		return animationNames;
	}
};

/*
class PlayerStateIdle : public PlayerState
{
public:
	PlayerStateIdle() { mId = StateId::Idle; }

	void Enter(Player& player) override {}
	void Update(Player& player, const float deltaTime) override {}
	void Exit(Player& player) override {}
};

class PlayerStateWalk : public PlayerState
{
public:
	PlayerStateWalk() { mId = StateId::Walk; }

	void Enter(Player& player) override {}
	void Update(Player& player, const float deltaTime) override {}
	void Exit(Player& player) override {}
};

class PlayerStateRun : public PlayerState
{
public:
	PlayerStateRun() { mId = StateId::Run; }

	void Enter(Player& player) override {}
	void Update(Player& player, const float deltaTime) override {}
	void Exit(Player& player) override {}
};

class PlayerStateLand : public PlayerState
{
public:
	PlayerStateLand() { mId = StateId::Land; }

	void Enter(Player& player) override
	{
		player.SetAnimationTime();
	}
	void Update(Player& player, const float deltaTime) override
	{
		if (player.GetAnimationTime() > 0.85f) {
			player.SetAnimationTime();
			player.ChangeState(new PlayerStateIdle);
		}
	}
	void Exit(Player& player) override {}
};

class PlayerStateFall : public PlayerState
{
public:
	PlayerStateFall() { mId = StateId::Fall; }

	void Enter(Player& player) override {}
	void Update(Player& player, const float deltaTime) override
	{
		if (!player.IsFalling()) {
			player.SetAnimationTime();
			player.ChangeState(new PlayerStateLand);
		}
	}
	void Exit(Player& player) override {}
};

class PlayerStateJump : public PlayerState
{
public:
	PlayerStateJump() { mId = StateId::Jump; }

	virtual void Enter(Player& player) override
	{
		player.SetAnimationTime();
	}
	virtual void Update(Player& player, const float deltaTime) override
	{
		if (player.GetAnimationTime() > 0.85f) {
			player.SetAnimationTime();
			player.ChangeState(new PlayerStateFall);
		}
	}
	virtual void Exit(Player& player) override {}
};

class PlayerStateAttack : public PlayerState
{
public:
	PlayerStateAttack() { mId = StateId::Attack1; }

	virtual void Enter(Player& player) override
	{
		player.SetAnimationTime();
	}
	virtual void Update(Player& player, const float deltaTime) override
	{
		if (player.GetAnimationTime() > 0.85f) {
			player.SetAnimationTime();
			player.ChangeState(new PlayerStateIdle);
		}
	}
	virtual void Exit(Player& player) override {}
};*/
