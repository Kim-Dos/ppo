#pragma once

#include "Camera.h"
#include "GameObject.h"

#define MAX_PLAYER_CAMERA_PITCH 85.0f

class Player;

enum class StateId : UINT
{
	Defalut = 0,
	Idle,
	Walk,
	Run,
	Jump,
	Fall,
	Land,
	Count
};

class BaseState
{
public:
	BaseState() { mPlayer = nullptr; }
	BaseState(GameObject* player) { mPlayer = player; }
	~BaseState() {}

	StateId ID() { return mId; }

	virtual void Enter() {};
	virtual void Update(const GameTimer& gt) {};
	virtual void Exit() {};
protected:
	StateId mId = StateId::Defalut;
	GameObject* mPlayer;
};

class FSM
{
public:
	FSM() : mCurrentState(BaseState()) {}
	FSM(BaseState initState)
	{
		mCurrentState = initState;
	}
	~FSM() {}
	BaseState GetState() { return mCurrentState; }

	void ChangeState(BaseState nextState)
	{
		if (nextState.ID() == mCurrentState.ID())
			return;

		if ((UINT)nextState.ID() < 0 || (UINT)nextState.ID() >= (UINT)StateId::Count)
			return;

		mCurrentState.Exit();
		mCurrentState = nextState;
		mCurrentState.Enter();
	}

	void UpdateState(const GameTimer& gt)
	{
		mCurrentState.Update(gt);
	}

private:
	BaseState mCurrentState;
};

class PlayerIdleState : public BaseState
{
public:
	PlayerIdleState(GameObject* player) : BaseState(player) { mId = StateId::Idle; }

	void Enter() override {}
	void Update(const GameTimer& gt) override {}
	void Exit() override {}
};

class PlayerWalkState : public BaseState
{
public:
	PlayerWalkState(GameObject* player) : BaseState(player) { mId = StateId::Walk; }

	void Enter() override {}
	void Update(const GameTimer& gt) override {}
	void Exit() override {}
};

class PlayerRunState : public BaseState
{
public:
	PlayerRunState(GameObject* player) : BaseState(player) { mId = StateId::Run; }

	void Enter() override {}
	void Update(const GameTimer& gt) override {}
	void Exit() override {}
};

class Player : public GameObject
{
public:
	Player();
	Player(const string name, XMFLOAT4X4 world, XMFLOAT4X4 texTransform);
	Player(const string name, XMMATRIX world, XMMATRIX texTransform);
	~Player();

	virtual void Update(const GameTimer& gt);
	void UpdateCamera();
	void UpdateState(const GameTimer& gt);		// PlayerState º¯È¯

	void KeyInput(float dt);
	void MouseInput(float dx, float dy);

	Camera* GetCamera() { return mCamera; }

	UINT GetAnimationTime() { return mAnimationTime; }
	UINT GetStateId() { return (UINT)mFSM.GetState().ID(); }
	UINT GetAnimationIndex() { return mAnimationIndex[GetStateId()]; }
private:
	void InitPlayer();

	float mPitch = 0.0f;
	
	XMFLOAT3 mVelocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 mAcceleration = XMFLOAT3(30.0f, 10.0f, 30.0f);

	float mMaxWalkVelocityXZ = 3.0f;
	float mMaxRunVelocityXZ = 6.0f;
	float mMaxVelocityY = 10.0f;
	float mFriction = 10.0f;
	
	Camera* mCamera = nullptr;
	XMFLOAT3 mCameraOffsetPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float mAnimationTime = 0.0f;

	FSM mFSM;
	UINT mAnimationIndex[(UINT)StateId::Count];
};


