#pragma once

//#include "stdafx.h"
#include "d3dApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"
#include "Terrain.h"
#include "Camera.h"
#include "GameObject.h"
#include "Mesh.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;


extern const int gNumFrameResources;


// 하나의 물체를 그리는 데 필요한 매개변수들을 담는 가벼운 구조체
/*
struct RenderItem
{
	RenderItem() = default;

	// 세계 공간을 기준으로 물체의 국소 공간을 서술하는 세계 행렬
	// 이 행렬은 세계 공간 안에서의 물체의 의치와 방향, 크기를 결정
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
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced 매개변수들.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};
*/

enum class RenderLayer : int
{
	Opaque = 0,
	Sky,
	Count
};

class DummyApp : public D3DApp
{
public:

	DummyApp(HINSTANCE hInstance);
	DummyApp(const DummyApp& rhs) = delete;
	DummyApp& operator=(const DummyApp& rhs) = delete;
	~DummyApp();

	virtual bool Initialize()override;

private:

	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
	virtual bool OnKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	
	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildSkullGeometry();
	void BuildSkullGeometryTest();
	void LoadTerrain();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	Terrain mTerrain;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	PassConstants mMainPassCB;

	bool mIsWireframe = false;
	bool mIsToonShading = false;

	Camera mCamera;

	POINT mLastMousePos;

	UINT mSkyTexHeapIndex = 0;
};


