#pragma once
#include "d3dUtil.h"
#include "Mesh.h"
#include "GameTimer.h"

// 하나의 물체를 그리는 데 필요한 매개변수들을 담는 가벼운 구조체
struct RenderItem
{
	RenderItem() = default;

	// 세계 공간을 기준으로 물체의 국소 공간을 서술하는 세계 행렬
	// 이 행렬은 세계 공간 안에서의 물체의 의치와 방행, 크기를 결정
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// 물체의 자료가 변해서 상수버퍼를 갱신해야 하는지의 여부를 뜻하는 'Dirty'플래그
	// FrameResource마다 물체의 cBuffer가 있으므로 물체의 자료를 수정할 떄는 반드시
	// NumFramesDirty = gNumFrameResources로 설정해야 한다.
	// 그래야 각각의 프레임 자원이 갱신된다.
	int NumFramesDirty = gNumFrameResources;

	// 이 렌더 항목의 물체 상수 버퍼에 해당하는 
	// GPU 상수 버퍼의 색인
	UINT ObjCBIndex = -1;

	// 이 렌더 항목에 연관된 기하구조. 여러 렌더항목이 같은 기하구조를 참조할 수 있다.
	Material* Mat = nullptr;
	Mesh* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced 매개변수들.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	// Only applicable to skinned render-items.
	UINT SkinnedCBIndex = -1;

	// nullptr if this render-item is not animated by skinned mesh.
	//SkinnedModelInstance* SkinnedModelInst = nullptr;
};

class GameObject
{
public:
    GameObject();
    GameObject(const string name, XMFLOAT4X4 world, XMFLOAT4X4 texTransform);
	GameObject(const string name, XMMATRIX world, XMMATRIX texTransform);
	~GameObject();

	virtual void Update(const GameTimer& gt);

	void SetName(const string name) { mName = name; }

	void SetMesh(Mesh* mesh) { mMesh = mesh; }
	void SetMaterial(Material* material) { mMaterial = material; }
	void SetCBIndex(int objCBIndex, int skinnedCBIndex = -1);
	void SetPrimitiveType(D3D12_PRIMITIVE_TOPOLOGY primitiveType) { mPrimitiveType = primitiveType; }
	void SetDrawIndexedInstanced(UINT numIndex, UINT baseIndex, UINT baseVertex);
	void SetFrameDirty() { mNumFramesDirty = gNumFrameResources; }
	void DecreaseFrameDirty() { mNumFramesDirty--; }

	// renderItem
	Mesh* GetMesh() { return mMesh; };
	Material* GetMeterial() { return mMaterial; };
	D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveType() { return mPrimitiveType; };
	int GetObjCBIndex() { return mObjCBIndex; };
	int GetSkinnedCBIndex() { return mSkinnedCBIndex; };
	UINT GetNumIndices() { return mNumIndices; };
	UINT GetBaseIndex() { return mBaseIndex; };
	UINT GetBaseVertex() { return mBaseVertex; };
	UINT GetFramesDirty() { return mNumFramesDirty; }

	void SetPosition(float x, float y, float z);
	void SetPosition(XMFLOAT3 position);
	void SetScale(float x, float y, float z);
	void SetScale(XMFLOAT3 scale);
	void SetTextureScale(float x, float y, float z);
	void SetTextureScale(XMFLOAT3 scale);

	std::string GetName() { return mName; }
	
	XMFLOAT4X4 GetWorld();
	XMFLOAT4X4 GetTexTransform() { return mTexTransform; }
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
private:

	string mName;
	
	bool mWorldMatDirty = true;
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mTexTransform = MathHelper::Identity4x4();

	// 물체의 자료가 변해서 상수버퍼를 갱신해야 하는지의 여부를 뜻하는 'Dirty'플래그
	// FrameResource마다 물체의 cBuffer가 있으므로 물체의 자료를 수정할 떄는 반드시
	// NumFramesDirty = gNumFrameResources로 설정해야 한다.
	// 그래야 각각의 프레임 자원이 갱신된다.
	int mNumFramesDirty = gNumFrameResources;

	// 이 렌더 항목의 물체 상수 버퍼에 해당하는 
	// GPU 상수 버퍼의 색인
	int mObjCBIndex = -1;
	int mSkinnedCBIndex = -1;

	Material* mMaterial = nullptr;
	Mesh* mMesh = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced 매개변수
	UINT mNumIndices = 0;
	UINT mBaseIndex = 0;
	UINT mBaseVertex = 0;
};

