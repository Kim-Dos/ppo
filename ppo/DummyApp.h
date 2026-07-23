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
#include "Pathfinder.h"

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

struct SpawnInfo
{
	XMFLOAT3 playerPos;
	XMFLOAT3 cameraPos;
	XMFLOAT3 cameraLook;
};

class DummyApp : public D3DApp
{
public:

	DummyApp(HINSTANCE hInstance, NetworkBridge* bridge);
	DummyApp(const DummyApp& rhs) = delete;
	DummyApp& operator=(const DummyApp& rhs) = delete;
	~DummyApp();

	void SummonKnight();
	void SummonHunter();
	void SummonSlave();
	void SummonCommandCenter();

	void DoUpgrade();

	virtual bool Initialize()override;

private:

	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	void DrawCursor();
	void DrawSelectionRect();
	void DrawDarkness();
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

	void UpdateDarknessCB(const GameTimer& gt);
	void BuildDarknessGeometry();

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

	void BuildStaticColliders();
	void BuildDynamicColliders();                       
	void ResolveAllCollisions();                        

	void InitPathfinder();

	void BuildUICursor();

	void BuildCrystal(const float& x, const float& y, const float& degree);

	void DrawGameObjects(ID3D12GraphicsCommandList* cmdList, const std::vector<GameObject*>& ritems);
	void DrawBoundingBox(ID3D12GraphicsCommandList* cmdList, const std::vector<GameObject*>& ritems);

	void DrawButtons(ID3D12GraphicsCommandList* cmdList);

	void ReleseMemory();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();


	// 송신 헬퍼 (피킹 이동 시 호출)
	void SendMoveRequest(unsigned char objNumber, const XMFLOAT3& dest);
	void SendMultiMoveRequest(const std::vector<unsigned char>& objNumbers, const XMFLOAT3& dest);
	void SendStopRequest(unsigned char objNumber);

private:

	bool mGameStarted = false;

	// (owner, objNumber) → GameObject* 매핑. 키 = owner*256 + objNumber
	std::unordered_map<int, GameObject*> mNetworkObjects;

	static int NetKey(int owner, unsigned char objNumber) {
		return owner * 256 + objNumber;
	}


	void SendLinkRequest();
	void OnLinkResult(const SCLinkResult* p);
	void OnGameStart(const SCGameStart* p);

	// 시작 유닛 1개를 로컬 오브젝트와 연결(또는 생성)
	void BindNetworkUnit(const SCStartUnit& u);

	// 통신용 
	NetworkBridge* mNetworkBridge = nullptr;
	int mMyPlayerNumber = -1;

	void ProcessReceivedPackets();

	// 수신 패킷별 핸들러
	void OnMoveObjResult(const SCMoveObjResult* p);
	void OnMoveMultiResult(const SCMoveMultiResult* p);
	void OnPositionSync(const SCPositionSync* p);
	void OnMoveRejected(const SCMoveRejected* p);
	void OnHackWarning(const SCHackWarning* p);

	// objNumber → GameObject 매핑 (컨테이너에 맞게 구현 필요)
	GameObject* FindNetworkObject(int ownerPlayer, unsigned char objNumber);

	// 프레임용
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
	std::vector<D3D12_INPUT_ELEMENT_DESC> mDarknessInputLayout;


	// 오브젝트 관리용
	std::vector<GameObject*> mAllGameObjects;
	std::vector<GameObject*> mTeamObjects;
	std::vector<GameObject*> mEnemyObjects;

	// 충돌	관리용
	std::vector<BoundingBox> mStaticColliders;          
	std::vector<GameObject*> mDynamicColliders;         

	Pathfinder mPathfinder;

	// UI 관리용
	bool mDarknessEnabled = true;
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


	ComPtr<ID3D12Resource> mSelectionVB;
	ComPtr<ID3D12Resource> mSelectionIB;
	ComPtr<ID3D12Resource> mSelectionVBUpload;
	ComPtr<ID3D12Resource> mSelectionIBUpload;

	D3D12_VERTEX_BUFFER_VIEW mSelectionVBView;
	D3D12_INDEX_BUFFER_VIEW  mSelectionIBView;

	void BuildSelectionGeometry();

	ComPtr<ID3D12Resource> mDarknessVB;
	ComPtr<ID3D12Resource> mDarknessIB;
	ComPtr<ID3D12Resource> mDarknessVBUpload;
	ComPtr<ID3D12Resource> mDarknessIBUpload;

	D3D12_VERTEX_BUFFER_VIEW mDarknessVBView;
	D3D12_INDEX_BUFFER_VIEW  mDarknessIBView;

	D3D12_GPU_DESCRIPTOR_HANDLE mDepthSrvGpuHandle{};
};