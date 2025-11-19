#pragma once

#include "d3dApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"
#include "Terrain.h"
#include "Camera.h"
#include "GameObject.h"
#include "SkinnedMesh.h"
#include "Player.h"
#include "PhysicsHelper.h"

#include "Button.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

extern const int gNumFrameResources;

enum class RenderLayer : int
{
	Opaque = 0,
	SkinnedOpaque,
	Debug,
	Sky,
	UI,
	Count
};

enum class GameObjectLayer : int
{
	Sky = 0,
	Environment,
	Object,
	Picking,
	Count
};

struct SpecialKeyInput
{
	bool isShift = false;
	bool isCtrl = false;

};

struct UIKeyInput
{
	bool isB = false; // Build -> House or Tower

	bool isU = false; // Upgrade
	
	bool isO = false; // Objects -> Hunter Knight Slave -> Summon
	
	bool isK = false; //Knight
	bool isN = false; //Hunter
	bool isL = false; //Slave
	bool isH = false; //House
	bool isT = false; //Tower
};

class DummyApp : public D3DApp
{
public:

	DummyApp(HINSTANCE hInstance, boost::asio::io_context& IOContext);
	DummyApp(const DummyApp& rhs) = delete;
	DummyApp& operator=(const DummyApp& rhs) = delete;
	~DummyApp();

	void SummonKnight();
	void SummonHunter();
	void SummonSlave();

	void DoUpgrade();
	void SetBuilding();


	virtual bool Initialize()override;

private:

	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	void DrawCursor();
	void DrawSelectionRect();

	void DrawDebug();
	void DrawBoundingBox();

	virtual void OnMouseDown(UINT msg, WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(UINT msg, WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
	virtual void OnMouseWheel(WPARAM wheeldelta)override;
	virtual bool OnKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void DragEvent();
	void FollowerKeyEvent();
	void AddPicking();
	void PickingMove();
	void PickingAttackMove();
	void PickingPatrolMove();
	void UIPicking(WPARAM wParam);
	void RsetUIInput();
	void SummonObject();

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	//void UpdateSkinnedCBs(const GameTimer& gt);
	void UpdateSkinnedCB(const GameTimer& gt, GameObject* skinnobj);

	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	
	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void LoadSkinnedMesh();
	void LoadMeshes();
	void LoadTerrain();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildGameObjects();

	void BuildUICursor();

	void BuildCrystal(const float& x, const float& y, const float& degree);

	void DrawGameObjects(ID3D12GraphicsCommandList* cmdList, const std::vector<GameObject*>& ritems);
	void DrawBoundingBox(ID3D12GraphicsCommandList* cmdList, const std::vector<GameObject*>& ritems);

	void DrawButtons(ID3D12GraphicsCommandList* cmdList);





	void ReleseMemory();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	Terrain mTerrain;
	std::unordered_map<std::string, Mesh*> mMeshes;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mColorInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mUIInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSelectionInputLayout;

	// List of all the render items.
	//std::vector<std::unique_ptr<RenderItem>> mAllRi
	// 
	// tems;
	std::vector<GameObject*> mAllGameObjects;

	bool mPicking = false;
	bool mDragFlag = false;
	bool mRoateFlag = true;

	int objCBIndex = 0;
	int skinnedCBIndex = 0;
	
	FollowerKeyInput mFollowerinput = FollowerKeyInput::None;

	UIKeyInput mUIkey;

	SpecialKeyInput mSpecialKeyinput;


	std::vector<Button*>mButtons;

	//std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	std::vector<GameObject*> mRenderLayer[(int)RenderLayer::Count];
	std::vector<GameObject*> mGameObjectLayer[(int)GameObjectLayer::Count];

	PassConstants mMainPassCB;
	
	bool mDebugMode = true;

	//SkinnedMesh* mSkinnedMesh;

	Player* mPlayer = nullptr;

	bool mFPSmode = false;
	//CamInput input;
	
	
	Camera* mMainCamera = nullptr;
	
	
	std::vector<Camera*> mSubCamera;
	POINT mStartMousePos;
	POINT mLastMousePos;


	UINT mSkyTexHeapIndex = 0;
	
	UINT mCursorTexHeapIndex = 0;

	ComPtr<ID3D12Resource> mCursorVB;
	ComPtr<ID3D12Resource> mCursorIB;
	ComPtr<ID3D12Resource> mCursorVBUpload;
	ComPtr<ID3D12Resource> mCursorIBUpload;

	D3D12_VERTEX_BUFFER_VIEW mCursorVBView;
	D3D12_INDEX_BUFFER_VIEW  mCursorIBView;

	RECT GetDragRect() const;


	// --- 선택 박스용 버퍼 ---
	ComPtr<ID3D12Resource> mSelectionVB;
	ComPtr<ID3D12Resource> mSelectionIB;
	ComPtr<ID3D12Resource> mSelectionVBUpload;
	ComPtr<ID3D12Resource> mSelectionIBUpload;

	D3D12_VERTEX_BUFFER_VIEW mSelectionVBView;
	D3D12_INDEX_BUFFER_VIEW  mSelectionIBView;

	void BuildSelectionGeometry();  // 선택 박스용 지오메트리

};


