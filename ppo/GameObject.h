#pragma once
#include "d3dUtil.h"
#include "Mesh.h"
#include "GameTimer.h"

// �ϳ��� ��ü�� �׸��� �� �ʿ��� �Ű��������� ��� ������ ����ü
struct RenderItem
{
	RenderItem() = default;

	// ���� ������ �������� ��ü�� ���� ������ �����ϴ� ���� ���
	// �� ����� ���� ���� �ȿ����� ��ü�� ��ġ�� ����, ũ�⸦ ����
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// ��ü�� �ڷᰡ ���ؼ� ������۸� �����ؾ� �ϴ����� ���θ� ���ϴ� 'Dirty'�÷���
	// FrameResource���� ��ü�� cBuffer�� �����Ƿ� ��ü�� �ڷḦ ������ ���� �ݵ��
	// NumFramesDirty = gNumFrameResources�� �����ؾ� �Ѵ�.
	// �׷��� ������ ������ �ڿ��� ���ŵȴ�.
	int NumFramesDirty = gNumFrameResources;

	// �� ���� �׸��� ��ü ��� ���ۿ� �ش��ϴ� 
	// GPU ��� ������ ����
	UINT ObjCBIndex = -1;

	// �� ���� �׸� ������ ���ϱ���. ���� �����׸��� ���� ���ϱ����� ������ �� �ִ�.
	Material* Mat = nullptr;
	Mesh* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced �Ű�������.
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

	// ��ü�� �ڷᰡ ���ؼ� ������۸� �����ؾ� �ϴ����� ���θ� ���ϴ� 'Dirty'�÷���
	// FrameResource���� ��ü�� cBuffer�� �����Ƿ� ��ü�� �ڷḦ ������ ���� �ݵ��
	// NumFramesDirty = gNumFrameResources�� �����ؾ� �Ѵ�.
	// �׷��� ������ ������ �ڿ��� ���ŵȴ�.
	int mNumFramesDirty = gNumFrameResources;

	// �� ���� �׸��� ��ü ��� ���ۿ� �ش��ϴ� 
	// GPU ��� ������ ����
	int mObjCBIndex = -1;
	int mSkinnedCBIndex = -1;

	Material* mMaterial = nullptr;
	Mesh* mMesh = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY mPrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced �Ű�����
	UINT mNumIndices = 0;
	UINT mBaseIndex = 0;
	UINT mBaseVertex = 0;
};

