#pragma once
#include "d3dUtil.h"
#include "Mesh.h"
#include "GameTimer.h"
#include "Actor.h"

#define MAX_NUM_SUBMESHES 4


struct DrawIndex
{
	UINT mNumIndices = 0;
	UINT mBaseIndex = 0;
	UINT mBaseVertex = 0;
};

enum class FollowerKeyInput : int
{
	None,
	Move,
	Attack,
	Patrol
};

class GameObject : public Actor
{
public:
    GameObject();
    GameObject(const string name, ObjectsType type, XMFLOAT4X4 world, XMFLOAT4X4 texTransform );
	GameObject(const string name, ObjectsType type, XMMATRIX world, XMMATRIX texTransform);
	~GameObject();

	virtual void Update(const GameTimer& gt);

	void SetMesh(Mesh* mesh) { mMesh = mesh; }
	void SetMaterial(Material* material);
	void SetMaterials(UINT numMaterial, const vector<Material*>& materials);
	void SetCBIndex(int& objCBIndex);
	void SetCBIndex(int& objCBIndex, int& skinnedCBIndex);
	void SetCBIndex(int numObjCBs, int& objCBIndices, int& skinnedCBIndex);
	void SetPrimitiveType(D3D12_PRIMITIVE_TOPOLOGY primitiveType) { mPrimitiveType = primitiveType; }
	void SetFrameDirty() { mNumFramesDirty = gNumFrameResources; }
	void DecreaseFrameDirty() { mNumFramesDirty--; }

	// renderItem
	Mesh* GetMesh() { return mMesh; };
	Material* GetMeterial(UINT index) { return mMaterials[index]; };
	D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveType() { return mPrimitiveType; };
	int GetObjCBIndex(UINT index) { return mObjCBIndex[index]; };
	int GetSkinnedCBIndex() { return mSkinnedCBIndex; };
	UINT GetNumIndices(UINT index) { return mDrawIndex[index].mNumIndices; };
	UINT GetBaseIndex(UINT index) { return mDrawIndex[index].mBaseIndex; };
	UINT GetBaseVertex(UINT index) { return mDrawIndex[index].mBaseVertex; };
	UINT GetFramesDirty() { return mNumFramesDirty; }
	UINT GetNumSubmeshes() { return mNumSubmeshes; }

	void AddSubmesh(const Submesh& submesh);
	void SetScale(float x, float y, float z);
	void SetScale(XMFLOAT3 scale);
	void SetTextureScale(float x, float y, float z);
	void SetTextureScale(XMFLOAT3 scale);

	std::string GetName() { return mName; }
	
	XMFLOAT4X4 GetTexTransform() { return mTexTransform; }

	void CreateBoundingBox(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* commandList);

	void SetBoundingBox(XMFLOAT3 center, XMFLOAT3 extents) { mBoundingBox = BoundingBox(center, extents); }
	void SetBoundingBox(BoundingBox boundingBox) { mBoundingBox = boundingBox; }
	BoundingBox GetBoundingBox() { return mBoundingBox; }

	D3D12_VERTEX_BUFFER_VIEW BoundingBoxVertexBufferView() const;
	D3D12_INDEX_BUFFER_VIEW BoundingBoxIndexBufferView() const;

private:
	
	bool mWorldMatDirty = true;
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	BoundingBox mBoundingBox;

	XMFLOAT4X4 mTexTransform = MathHelper::Identity4x4();

	// 이 렌더 항목의 물체 상수 버퍼에 해당하는 
	// GPU 상수 버퍼의 색인
	int mObjCBIndex[MAX_NUM_SUBMESHES] = { -1, };
	int mSkinnedCBIndex = -1;
	
	Material* mMaterials[MAX_NUM_SUBMESHES] = { nullptr, };
	Mesh* mMesh = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced 매개변수
	UINT mNumSubmeshes = 0;
	DrawIndex mDrawIndex[MAX_NUM_SUBMESHES];

	// draw BoundingBox
	Microsoft::WRL::ComPtr<ID3D12Resource> mBoundVertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBoundIndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> mBoundVertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBoundIndexBufferUploader = nullptr;
};

