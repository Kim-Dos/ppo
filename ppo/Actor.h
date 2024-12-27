#pragma once

#include "d3dUtil.h"
#include "Mesh.h"
#include "GameTimer.h"

class Actor {
public:
	Actor();
	Actor(const string name, XMFLOAT4X4 world, ObjectsType type);
	Actor(const string name, XMMATRIX world, ObjectsType type);
	~Actor();

	virtual void Update(const GameTimer& gt);

	void SetName(const string name) { mName = name; }

	void SetFrameDirty() { mNumFramesDirty = gNumFrameResources; }
	void DecreaseFrameDirty() { mNumFramesDirty--; }

	// renderItem
	UINT GetFramesDirty() { return mNumFramesDirty; }

	void SetWorldMat(XMFLOAT4X4 world);
	void SetPosition(float x, float y, float z);
	void SetPosition(XMFLOAT3 position);

	std::string GetName() { return mName; }

	XMFLOAT4X4 GetWorld();
	XMFLOAT3 GetPosition();
	XMFLOAT3 GetLook();
	XMFLOAT3 GetUp();
	XMFLOAT3 GetRight();

	void MoveStrafe(float distance = 1.0f);
	void MoveUp(float distance = 1.0f);
	void MoveForward(float distance = 1.0f);
	void MoveFront(float distance = 1.0f);

	void Rotate(float pitch, float yaw, float roll);
	void Rotate(XMFLOAT3* axis, float angle);
	void Rotate(XMFLOAT4* quaternion);


protected:
	string mName;

	bool mWorldMatDirty = true;
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();

	// 물체의 자료가 변해서 상수버퍼를 갱신해야 하는지의 여부를 뜻하는 'Dirty'플래그
	// FrameResource마다 물체의 cBuffer가 있으므로 물체의 자료를 수정할 떄는 반드시
	// NumFramesDirty = gNumFrameResources로 설정해야 한다.
	// 그래야 각각의 프레임 자원이 갱신된다.
	int mNumFramesDirty = gNumFrameResources;

	ObjectsType mObjectType;


};
