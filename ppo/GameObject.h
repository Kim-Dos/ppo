#pragma once
#include "d3dUtil.h"

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
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced �Ű�������.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

class GameObject
{
public:
    GameObject();
    ~GameObject();

	void SetName(std::string name) { mName = name; }
	void SetName(const char* name) { mName = name; }
	void SetMaterial(const std::vector<std::string> materials) { mMaterials = materials; }

	void SetPosition(float x, float y, float z);
	void SetPosition(XMFLOAT3 position);
	void SetScale(float x, float y, float z);

	void SetChild(GameObject* child);

	std::string GetName() { return mName; }
	std::vector<std::string> GetMaterials() { return mMaterials; }
	
	XMFLOAT3 GetPosition();
	XMFLOAT3 GetLook();
	XMFLOAT3 GetUp();
	XMFLOAT3 GetRight();

	RenderItem GetRenderItem() { return mRenderItem; }

	void MoveStrafe(float distance = 1.0f);
	void MoveUp(float distance = 1.0f);
	void MoveForward(float distance = 1.0f);

	void Rotate(float pitch, float yaw, float roll);
	void Rotate(XMFLOAT3* axis, float angle);
	void Rotate(XMFLOAT4* quaternion);

	GameObject* GetParent() { return mParent; };
	GameObject* FindFrame(const std::string frameName);

	static GameObject* LoadFrameHierarchyFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, GameObject* parent, FILE* file);
	static GameObject* LoadGeometryFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, std::string fileName);

private:
	std::string mName;

	std::vector<std::string> mMaterials;
	RenderItem mRenderItem;

	GameObject* mParent = nullptr;
	GameObject* mChild = nullptr;
	GameObject* mSibling = nullptr;
};

