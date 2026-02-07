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

enum class FogState : uint8_t
{
	Hidden = 0,
	Seen = 1,
	Visible = 2
};

struct FogSystem
{
	int gridX = 0;
	int gridZ = 0;
	float cellSize = 4.0f;

	// 한번이라도 본 적 있는가 (Seen+Visible)
	std::vector<uint8_t> explored;      // 0/1

	// 현재 몇 개 유닛이 이 셀을 보고 있는가 (Visible 판단용)
	std::vector<uint16_t> visibleCount; // >=0

	// Terrain 높이 캐시
	std::vector<float> heights;

	// Dirty rect(변경된 영역) 누적
	bool dirty = false;
	int dirtyMinX = 0, dirtyMinZ = 0, dirtyMaxX = 0, dirtyMaxZ = 0; // inclusive

	inline bool InBounds(int gx, int gz) const
	{
		return gx >= 0 && gz >= 0 && gx < gridX && gz < gridZ;
	}

	inline int Index(int gx, int gz) const
	{
		return gz * gridX + gx;
	}

	inline float HeightAt(int gx, int gz) const
	{
		return heights[Index(gx, gz)];
	}

	inline void MarkDirtyRect(int minX, int minZ, int maxX, int maxZ)
	{
		minX = MathHelper::Clamp(minX, 0, gridX - 1);
		maxX = MathHelper::Clamp(maxX, 0, gridX - 1);
		minZ = MathHelper::Clamp(minZ, 0, gridZ - 1);
		maxZ = MathHelper::Clamp(maxZ, 0, gridZ - 1);

		if (!dirty)
		{
			dirty = true;
			dirtyMinX = minX; dirtyMinZ = minZ;
			dirtyMaxX = maxX; dirtyMaxZ = maxZ;
		}
		else
		{
			dirtyMinX = (std::min)(dirtyMinX, minX);
			dirtyMinZ = (std::min)(dirtyMinZ, minZ);
			dirtyMaxX = (std::max)(dirtyMaxX, maxX);
			dirtyMaxZ = (std::max)(dirtyMaxZ, maxZ);
		}
	}

	inline void ClearDirty() { dirty = false; }
};

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

	void InitFog();
	void WorldToFog(float wx, float wz, int& gx, int& gz);
	void CastLight(int cx, int cz, int row, float startSlope, float endSlope,
		int radius, int octant, float unitHeight, float maxSlope);
	void ComputeFOVForUnit(GameObject* unit);
	void UpdateFogOfWar();
	void UpdateFogTexture();
	void BuildFogResources();

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

	void BuildUICursor();

	void BuildCrystal(const float& x, const float& y, const float& degree);

	void DrawGameObjects(ID3D12GraphicsCommandList* cmdList, const std::vector<GameObject*>& ritems);
	void DrawBoundingBox(ID3D12GraphicsCommandList* cmdList, const std::vector<GameObject*>& ritems);

	void DrawButtons(ID3D12GraphicsCommandList* cmdList);

	void BuildCircleOffsets(int maxRadius);
	void ApplyVisionStamp(int cx, int cz, int radius, int delta); // delta: +1 또는 -1



	void ReleseMemory();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();


	inline void FogTileToWorld(int gx, int gz, float& wx, float& wz)
	{
		float mapWidth = mTerrain.GetWidth();
		float mapLength = mTerrain.GetLength();

		float nx = (gx + 0.5f) / mFog.gridX;
		float nz = (gz + 0.5f) / mFog.gridZ;

		wx = nx * mapWidth - mapWidth * 0.5f;

		wz = mapLength - nz * mapLength - mapLength * 0.5f;
	}


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
	std::vector<D3D12_INPUT_ELEMENT_DESC> mDarknessInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mFogInputLayout;

	// List of all the render items.
	//std::vector<std::unique_ptr<RenderItem>> mAllRi
	// 
	// tems;
	std::vector<GameObject*> mAllGameObjects;
	std::vector<GameObject*> mTeamObjects;
	std::vector<GameObject*> mEnemyObjects;

	bool mFogFlag = true;
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

	FogSystem mFog;
	bool mFogDirty = true;

	float mFogUpdateTime = 0.0f;
	float mFogUpdateInterval = 0.2f; // 0.2초마다 한 번 (원하면 0.1f로 줄여도 됨)

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


	ComPtr<ID3D12Resource> mDarknessVB;
	ComPtr<ID3D12Resource> mDarknessIB;
	ComPtr<ID3D12Resource> mDarknessVBUpload;
	ComPtr<ID3D12Resource> mDarknessIBUpload;

	D3D12_VERTEX_BUFFER_VIEW mDarknessVBView;
	D3D12_INDEX_BUFFER_VIEW  mDarknessIBView;

	ComPtr<ID3D12Resource> mFogTex;      // GPU texture
	ComPtr<ID3D12Resource> mFogUpload;   // Upload heap

	// FogTex 상태 추적(초기: COPY_DEST, 이후: PIXEL_SHADER_RESOURCE)
	D3D12_RESOURCE_STATES mFogTexState = D3D12_RESOURCE_STATE_COPY_DEST;
	ComPtr<ID3D12DescriptorHeap> mFogSrvHeap; // SRV heap (필요하다면)

	D3D12_GPU_DESCRIPTOR_HANDLE mDepthSrvGpuHandle{};

	// FogTex가 올라간 SRV 위치 기억용
	D3D12_CPU_DESCRIPTOR_HANDLE mFogSrvCpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE mFogSrvGpuHandle{};

	struct FogUnitCache
	{
		int gx = 0, gz = 0;
		bool valid = false;
	};

	std::unordered_map<GameObject*, FogUnitCache> mFogUnitCache;

	// 반경별 원 오프셋 LUT (radius -> (dx,dz) 리스트)
	std::vector<std::vector<std::pair<int, int>>> mCircleOffsets;

	// Fog 텍스처 업데이트용 재사용 버퍼
	std::vector<uint8_t> mFogTexStaging;

};


