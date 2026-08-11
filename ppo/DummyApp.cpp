#include "DummyApp.h"

const int gNumFrameResources = 3;
constexpr float VisionRadiusWorld = 800.f;

DummyApp::DummyApp(HINSTANCE hInstance, NetworkBridge* bridge)
	: D3DApp(hInstance), mNetworkBridge(bridge)
{

}

DummyApp::~DummyApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();

	ReleseMemory();
}

bool DummyApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// 초기화 명령을 위해 명령목록을 재설정하다.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// 이 힙 유형에 있는 설명자의 증분 크기를 가져옵니다. 이는 하드웨어에 따라 다르다.
	mCbvSrvDescriptorSize = md3dDevice->
		GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	ShowCursor(false);
	CenterMouseCursor();

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildSelectionGeometry();
	BuildDarknessGeometry();
	BuildUICursor();
	LoadMeshes();
	LoadTerrain();

	BuildMaterials();
	BuildGameObjects();

	BuildStaticColliders();
	InitPathfinder();

	BuildFrameResources();
	BuildPSOs();



	// 초기화 명령 실행
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 초기화 명령들이 모두 처리되기 기다린다.
	FlushCommandQueue();

	SendLinkRequest();

	return true;
}

void DummyApp::OnResize()
{
	D3DApp::OnResize();

	// 창의 크기가 변경되었기 때문에 종횡비를 갱신하고
	// 투영행렬을 다시 계산한다.
	if (!mMainCamera) {
		mMainCamera = new Camera;
	}

	mMainCamera->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 10.f, 30000.f);
}

void DummyApp::Update(const GameTimer& gt)
{
	ProcessReceivedPackets();

	for (auto& x : mAllGameObjects) {
		x->Update(gt);
	}

	//OnKeyboardInput(gt);

	//float terrainY = mTerrain.GetHeight(mPlayer->GetPosition().x, mPlayer->GetPosition().z);
	//DebugPrint("height: %f\n", terrainY);
	//std::cout << terrainY << std::endl;
	for (auto& x : mGameObjectLayer[(int)GameObjectLayer::Object]) {
		float terrainY = mTerrain.GetHeight(x->GetPosition().x, x->GetPosition().z);

		if (x->GetPosition().y < terrainY) {
			x->SetPosition(x->GetPosition().x, terrainY, x->GetPosition().z);

			if (x->GetObjType() == ObjectsType::CHARACTER) {
				auto k = dynamic_cast<Player*>(x);
				k->SetVelocity(XMFLOAT3(k->GetVelocity().x, 0.0f, k->GetVelocity().z));
				k->SetFalling(false);
				k->SetFrameDirty();
			}
		}
	}

	ResolveAllCollisions();

	if (mFPSmode) {
	}
	else if(!mSpecialKeyinput.isCtrl) {
		mMainCamera->Move(gt);

		XMFLOAT3 camPos = mMainCamera->GetPosition3f();
		float terrainY = mTerrain.GetHeight(camPos.x, camPos.z);
		float minCamHeight = terrainY + 50.0f; // 지형 위 최소 높이 (조절 가능)
		if (camPos.y < minCamHeight) {
			mMainCamera->SetPosition(camPos.x, minCamHeight, camPos.z);
		}

		mMainCamera->UpdateViewMatrix();
		//std::cout << mMainCamera->GetPosition3f().x << ", " << mMainCamera->GetPosition3f().z << std::endl;

	}


	static float cooltime = 1.0f;
	cooltime -= gt.DeltaTime();
	static int b = 0;

	// 순환적으로 자원 프레임 배열의 다음 원소에 접근한다.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// GPU가 현재 프레임 자원의 명령들을 다 처리했는지 확인
	// 아직 다 처리하지 않았으면 GPU가 이 울타리 지점까지의 명령들을 처리할 때까지 기다린다.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	// mCurrFrameResource의 자원 갱신
	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	//UpdateSkinnedCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);

	UpdateDarknessCB(gt);
}

void DummyApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// 명령 기록에 관련된 메모리의 재활용을 위해 명령 할당자 재설정
	// 재설정은 GPU가 명령 목록을 모두 처리한 후에 일어난다.
	ThrowIfFailed(cmdListAlloc->Reset());

	// 명령 목록을 ExecuteCommandList를 통해서 명령 대기열에
	// 추가했다면 명령 목록을 재설정할 수 있다. 
	// 명령 목록을 재성정하면 메모리가 재활용된다.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	BuildPendingBoundingBox();

	// 뷰포트와 가위 직사각형을 설정한다.
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// 자원 용도에 관련된 상태 전이를 D3D에 통지한다.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// 후면 버퍼, 깊이 버퍼를 지운다.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// 렌더링 결과가 기록될 렌더 대상 버퍼들을 지정한다.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	// 이 장면에 쓰이는 모든 재질을 묶는다.
	// 구조적 버퍼는 힙을 생략하고 그냥 하나의 루트 서술자로 묶을 수 있다.
	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(3, matBuffer->GetGPUVirtualAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	skyTexDescriptor.Offset(mSkyTexHeapIndex, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(4, skyTexDescriptor);

	// 이 장면에 쓰이는 모든 텍스처를 묶는다. 
	// 테이블의 첫 서술자만 지정하면 된다.
	// 테이블에 몇 개의 서술자가 있는지는 루트 서명에 설정되어 있다.
	mCommandList->SetGraphicsRootDescriptorTable(5, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	mCommandList->SetPipelineState(mPSOs["opaque"].Get());
	DrawGameObjects(mCommandList.Get(), mRenderLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["skinnedOpaque"].Get());
	DrawGameObjects(mCommandList.Get(), mRenderLayer[(int)RenderLayer::SkinnedOpaque]);

	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawGameObjects(mCommandList.Get(), mRenderLayer[(int)RenderLayer::Sky]);

	if (mDebugMode)
		DrawDebug();

	if (mDarknessEnabled)
	{
		// Depth buffer는 기본적으로 DEPTH_WRITE 상태라서 픽셀 셰이더에서 샘플링 불가.
		// Darkness 패스에서 gDepthTex를 읽기 위해 잠깐 PS_RESOURCE로 전이했다가 다시 되돌린다.
		auto depthToSrv = CD3DX12_RESOURCE_BARRIER::Transition(
			mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mCommandList->ResourceBarrier(1, &depthToSrv);

		DrawDarkness();

		auto depthToWrite = CD3DX12_RESOURCE_BARRIER::Transition(
			mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
		mCommandList->ResourceBarrier(1, &depthToWrite);
	}
	DrawSelectionRect();
	if (mSpecialKeyinput.isCtrl &&  !mFPSmode) { DrawCursor(); }

	// test
	DrawButtons(mCommandList.Get());

	// 자원 용도에 관련된 상태 전이를 D3D에 통지한다.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// 명령 기록을 마친다.
	ThrowIfFailed(mCommandList->Close());

	// 명령 실행을 위해 명령 목록을 명령 대기열에 추가한다.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 후면버퍼와 전면버퍼를 바꾼다.
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// 현재 울타리 지점까지의 명령들을 표시하도록 울타리 값을 전진시킨다.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// 새 울타리 지점을 설정하는 명령을 명령대기열에 추가한다.
	// 지금 우리는 GPU 시간선 상에 있으므로, 새울타리 지점은 GPU가 이 Signal() 
	// 명령까지의 모든 명령을 처리하기 전까지는 설정되지 않는다.
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void DummyApp::DrawCursor()
{
	// 마우스 위치 가져오기 (윈도우 클라이언트 좌표)
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(mhMainWnd, &p);

	float x = (float)p.x;
	float y = (float)p.y;

	float w = (float)mClientWidth;
	float h = (float)mClientHeight;

	// 화면 좌표 → NDC(-1~1)
	float ndcX = x / w * 2.0f - 1.0f;
	float ndcY = -y / h * 2.0f + 1.0f;

	// 커서 크기 (NDC 기준) – 화면에서 대략 32x32픽셀 정도라고 치면:
	float cursorPixelW = 32.0f;
	float cursorPixelH = 32.0f;
	float sizeNDCX = cursorPixelW / w * 2.0f;
	float sizeNDCY = cursorPixelH / h * 2.0f;

	CursorConstants cbData;
	cbData.CursorPosNDC = XMFLOAT2(ndcX, ndcY);
	cbData.CursorSizeNDC = XMFLOAT2(sizeNDCX, sizeNDCY);

	mCurrFrameResource->CursorCB->CopyData(0, cbData);

	auto cmdList = mCommandList.Get();

	cmdList->SetPipelineState(mPSOs["ui"].Get());

	// RootSignature는 이미 SetRootSignature 되어 있음 (Draw 앞부분에서)

	// CBV b1에 CursorCB 세팅
	auto cursorCBAddress = mCurrFrameResource->CursorCB->Resource()->GetGPUVirtualAddress();
	cmdList->SetGraphicsRootConstantBufferView(1, cursorCBAddress);

	// 커서 텍스처 SRV를 descriptor table 4번에 세팅
	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	CD3DX12_GPU_DESCRIPTOR_HANDLE cursorTexHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	cursorTexHandle.Offset(mCursorTexHeapIndex, mCbvSrvDescriptorSize);
	cmdList->SetGraphicsRootDescriptorTable(4, cursorTexHandle);

	// 정점/인덱스 버퍼 설정 + topology
	cmdList->IASetVertexBuffers(0, 1, &mCursorVBView);
	cmdList->IASetIndexBuffer(&mCursorIBView);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void DummyApp::DrawSelectionRect()
{
	if (!mDragFlag)
		return;

	RECT dragRect = GetDragRect();

	float w = static_cast<float>(mClientWidth);
	float h = static_cast<float>(mClientHeight);

	float left = static_cast<float>(dragRect.left);
	float right = static_cast<float>(dragRect.right);
	float top = static_cast<float>(dragRect.top);
	float bottom = static_cast<float>(dragRect.bottom);

	float cx = (left + right) * 0.5f;
	float cy = (top + bottom) * 0.5f;
	float sx = (right - left);
	float sy = (bottom - top);

	float ndcX = cx / w * 2.0f - 1.0f;
	float ndcY = -cy / h * 2.0f + 1.0f;

	float sizeNDCX = sx / w * 2.0f;
	float sizeNDCY = sy / h * 2.0f;

	SelectionConstants selCB{};
	selCB.PosNDC = XMFLOAT2(ndcX, sizeNDCX > 0 ? ndcY : 0.0f);
	selCB.SizeNDC = XMFLOAT2(sizeNDCX, sizeNDCY);
	selCB.Color = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f); // 진한 초록, 알파 1.0 (선이니까)

	mCurrFrameResource->SelectionCB->CopyData(0, selCB);

	auto cmdList = mCommandList.Get();
	cmdList->SetPipelineState(mPSOs["selection"].Get());

	auto selCBAddress =
		mCurrFrameResource->SelectionCB->Resource()->GetGPUVirtualAddress();
	cmdList->SetGraphicsRootConstantBufferView(1, selCBAddress);

	// ★ 라인 리스트!
	cmdList->IASetVertexBuffers(0, 1, &mSelectionVBView);
	cmdList->IASetIndexBuffer(&mSelectionIBView);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	// 인덱스 8개 → 선 4개
	cmdList->DrawIndexedInstanced(8, 1, 0, 0, 0);
}

void DummyApp::DrawDarkness()
{
	auto cmdList = mCommandList.Get();

	// Darkness PSO 설정
	cmdList->SetPipelineState(mPSOs["darkness"].Get());

	// SRV/CBV heap 설정
	ID3D12DescriptorHeap* heaps[] = { mSrvDescriptorHeap.Get() };
	cmdList->SetDescriptorHeaps(1, heaps);

	// b1 = DarknessCB
	cmdList->SetGraphicsRootConstantBufferView(
		1, mCurrFrameResource->DarknessCB->Resource()->GetGPUVirtualAddress()
	);

	// b2(PassCB)는 Draw() 상단에서 이미 설정됨

	// ★ slot = 6번 → 깊이버퍼 SRV 테이블 (t0, space2)
	cmdList->SetGraphicsRootDescriptorTable(6, mDepthSrvGpuHandle);

	// 풀스크린 quad
	cmdList->IASetVertexBuffers(0, 1, &mDarknessVBView);
	cmdList->IASetIndexBuffer(&mDarknessIBView);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

RECT DummyApp::GetDragRect() const
{
	RECT dragRect;
	dragRect.left = min(mStartMousePos.x, mLastMousePos.x);
	dragRect.right = max(mStartMousePos.x, mLastMousePos.x);
	dragRect.top = min(mStartMousePos.y, mLastMousePos.y);
	dragRect.bottom = max(mStartMousePos.y, mLastMousePos.y);
	return dragRect;
}

void DummyApp::BuildSelectionGeometry()
{
	struct SelVertex
	{
		XMFLOAT2 Pos;
	};

	// 로컬 좌표 (-0.5~0.5) 기준 꼭짓점 4개
	SelVertex vertices[4] =
	{
		{ XMFLOAT2(-0.5f, -0.5f) }, // 0: 좌하
		{ XMFLOAT2(-0.5f,  0.5f) }, // 1: 좌상
		{ XMFLOAT2(0.5f,  0.5f) }, // 2: 우상
		{ XMFLOAT2(0.5f, -0.5f) }, // 3: 우하
	};

	// 선 4개 (0-1, 1-2, 2-3, 3-0)
	std::uint16_t indices[8] =
	{
		0, 1,
		1, 2,
		2, 3,
		3, 0
	};

	const UINT vbByteSize = sizeof(vertices);
	const UINT ibByteSize = sizeof(indices);

	mSelectionVB = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		vertices,
		vbByteSize,
		mSelectionVBUpload);

	mSelectionIB = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		indices,
		ibByteSize,
		mSelectionIBUpload);

	mSelectionVBView.BufferLocation = mSelectionVB->GetGPUVirtualAddress();
	mSelectionVBView.StrideInBytes = sizeof(SelVertex);
	mSelectionVBView.SizeInBytes = vbByteSize;

	mSelectionIBView.BufferLocation = mSelectionIB->GetGPUVirtualAddress();
	mSelectionIBView.Format = DXGI_FORMAT_R16_UINT;
	mSelectionIBView.SizeInBytes = ibByteSize;
}

void DummyApp::DrawDebug()
{
	DrawBoundingBox();
}

void DummyApp::DrawBoundingBox()
{
	//std::cout << "Debug draw" << std::endl;
	mCommandList->SetPipelineState(mPSOs["debug"].Get());
	DrawBoundingBox(mCommandList.Get(), mGameObjectLayer[(int)GameObjectLayer::Object]);
	DrawBoundingBox(mCommandList.Get(), mGameObjectLayer[(int)GameObjectLayer::Environment]);
}

void DummyApp::BuildPendingBoundingBox()
{
	for (const auto& pending : mPendingBoundingBuilds)
	{
		if (pending.object == nullptr)
			continue;

		if (pending.shape == BoundingShape::Cylinder)
		{
			pending.object->CreateCylinderBoundingBox(
				md3dDevice.Get(),
				mCommandList.Get(),
				pending.segments);
		}
		else
		{
			pending.object->CreateBoundingBox(
				md3dDevice.Get(),
				mCommandList.Get());
		}
	}

	mPendingBoundingBuilds.clear();
}

void DummyApp::OnMouseDown(UINT msg, WPARAM btnState, int x, int y)
{
	mStartMousePos.x = x;
	mStartMousePos.y = y;

	mLastMousePos.x = x;
	mLastMousePos.y = y;

	if (mFPSmode) {
	}
	else {
		if (msg == WM_RBUTTONDOWN) {
			mRotateFlag = false;
		}
		//else if ((btnState & MK_RBUTTON) != 0 && (btnState & MK_LBUTTON) == 0) { }
		else if (msg == WM_LBUTTONDOWN) {
			mDragFlag = false;
		}

	}

	//for (auto v : mButtons) {
	//	if (v->isActive() && v->isInButton(x, y))
	//		v->ButtonAction();
	//}

	SetCapture(mhMainWnd);
}

void DummyApp::OnMouseUp(UINT msg, WPARAM btnState, int x, int y)
{

	if (mFPSmode) {

	}
	else {
		mLastMousePos.x = x;
		mLastMousePos.y = y;

		if (msg == WM_LBUTTONUP) {
			if (mSpecialKeyinput.isCtrl) {
				if (mUIkey.isO || mUIkey.isB) {
					SummonObject();
					std::cout << mUIkey.isB << mUIkey.isH << std::endl;
				}
				else {
					//std::cout << "DragFlag - " << mDragFlag << std::endl;
					//드래그 이벤트 입력
					if (mDragFlag) DragEvent();
					else FollowerKeyEvent();
				}
			}
				mDragFlag = false;
		}
		else if (msg == WM_RBUTTONUP && !mRotateFlag && mSpecialKeyinput.isCtrl) {
			if (mPicking) {
				mFollowerinput = FollowerKeyInput::Move;
				FollowerKeyEvent();
			}
		}
		mRotateFlag = false;
	}

	ReleaseCapture();

}

void DummyApp::OnMouseMove(WPARAM btnState, int x, int y)
{

	// Make each pixel correspond to a quarter of a degree.

	if(mIgnoreMouseMove) {
		mIgnoreMouseMove = false;
		return;
	}

	if (mFPSmode) {

		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mPlayer->MouseInput(dx, dy);
		mLastMousePos = { x, y };
	}
	else if (!mSpecialKeyinput.isCtrl) {

		int centerX = mClientWidth / 2;
		int centerY = mClientHeight / 2;
		
		int deltaX = x - centerX;
		int deltaY = y - centerY;

		if (deltaX == 0 && deltaY == 0) return;

		float dx = XMConvertToRadians(0.25f * static_cast<float>(deltaX));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(deltaY));

		mMainCamera->Pitch(dy);
		mMainCamera->RotateY(dx);
		mMainCamera->UpdateViewMatrix();

		if ((btnState & MK_RBUTTON) != 0) mRotateFlag = true;
		
		CenterMouseCursor();

		return;
	}
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	if (!mFPSmode && mSpecialKeyinput.isCtrl && (btnState & MK_LBUTTON) != 0) {
		if (mLastMousePos.x != mStartMousePos.x || mLastMousePos.y != mStartMousePos.y) mDragFlag = true;
	}

}

void DummyApp::CenterMouseCursor()
{
	int centerX = mClientWidth / 2;
	int centerY = mClientHeight / 2;

	mLastMousePos.x = centerX;
	mLastMousePos.y = centerY;

	POINT CenterPoint = { centerX, centerY };

	ClientToScreen(mhMainWnd, &CenterPoint);

	mIgnoreMouseMove = true;

	SetCursorPos(CenterPoint.x, CenterPoint.y);
}

void DummyApp::DragEvent()
{
	RECT dragRect = GetDragRect();

	XMMATRIX viewMatrix = mMainCamera->GetView();
	XMMATRIX projMatrix = mMainCamera->GetProj();

	mGameObjectLayer[(int)GameObjectLayer::Picking].clear();

	for (const auto& obj : mGameObjectLayer[(int)GameObjectLayer::Object])
	{
		if (GetNetworkObjNumber(obj) < 0) continue;  // 네트워크 오브젝트가 아닌 경우 건너뜀

		// 예시: 바운딩 박스 center를 화면 좌표로 투영
		XMFLOAT3 center = obj->GetBoundingBox().Center;
		XMVECTOR centerW = XMVector3Transform(XMLoadFloat3(&center),
			XMLoadFloat4x4(&obj->GetWorld()));
		XMVECTOR viewPos = XMVector3Transform(centerW, viewMatrix);
		XMVECTOR projPos = XMVector3Transform(viewPos, projMatrix);

		XMFLOAT4 projPosF;
		XMStoreFloat4(&projPosF, projPos);

		if (projPosF.w <= 0.0f)
			continue;

		float ndcX = projPosF.x / projPosF.w;
		float ndcY = projPosF.y / projPosF.w;

		float screenX = (ndcX * 0.5f + 0.5f) * mClientWidth;
		float screenY = (-ndcY * 0.5f + 0.5f) * mClientHeight;

		if (screenX >= dragRect.left && screenX <= dragRect.right &&
			screenY >= dragRect.top && screenY <= dragRect.bottom &&
			XMVectorGetZ(viewPos) > 0.0f)
		{
			mGameObjectLayer[(int)GameObjectLayer::Picking].push_back(obj);
		}
	}

	mPicking = !mGameObjectLayer[(int)GameObjectLayer::Picking].empty();

#ifdef _DEBUG
	//for (auto& x : mGameObjectLayer[(int)GameObjectLayer::Picking])
		//cout << "Pickings - " << x->GetName() << endl;
#endif
}

void DummyApp::AddPicking()
{
	//std::cout << "AddPicking" << std::endl;

	float ndcX = (2.0f * mLastMousePos.x) / mClientWidth - 1.0f;
	float ndcY = 1.0f - (2.0f * mLastMousePos.y) / mClientHeight;

	// near plane과 살짝 더 먼 거리만 사용
	XMVECTOR rayOrigin = XMVectorSet(ndcX, ndcY, 0.1f, 1.f);    // near plane
	XMVECTOR rayTarget = XMVectorSet(ndcX, ndcY, 0.11f, 1.f);    // far plane

	XMMATRIX invViewProj = XMMatrixInverse(nullptr, XMMatrixMultiply(mMainCamera->GetView(), mMainCamera->GetProj()));
	XMVECTOR worldRayOrigin = XMVector3TransformCoord(rayOrigin, invViewProj);
	XMVECTOR worldRayTarget = XMVector3TransformCoord(rayTarget, invViewProj);

	XMVECTOR rayDirection = XMVector3Normalize(worldRayTarget - worldRayOrigin);

	//// XMFLOAT3로 변환
	//XMFLOAT3 rayPos, rayDir;
	//XMStoreFloat3(&rayPos, worldRayOrigin);
	//XMStoreFloat3(&rayDir, rayDirection);

	float closestDist = MathHelper::Infinity;
	GameObject* closestObject = nullptr;

	// 모든 게임 오브젝트에 대해 레이 충돌 검사
	float dist = 0.f;

	for (const auto& obj : mGameObjectLayer[(int)GameObjectLayer::Object])
	{
		if (GetNetworkObjNumber(obj) < 0) continue;  // 네트워크 오브젝트가 아닌 경우 건너뜀

		auto k = obj->GetBoundingBox().Center;
		auto j = obj->GetBoundingBox().Extents;
		//std::cout << obj->GetName() << std::endl;
		//std::cout << "Center" << k.x << ", " << k.y << ", " << k.z << std::endl;
		//std::cout << "Extents" << j.x << ", " << j.y << ", " << j.z << std::endl;
		bool xo = obj->GetBoundingBox().Intersects(worldRayOrigin, rayDirection, dist);
		//std::cout << "BBIntersectOX - " << xo << std::endl;
		if (xo)
		{

			std::cout << "dist - " << dist << std::endl;
			// 가장 가까운 오브젝트 찾기
			if (dist < closestDist)
			{
				std::cout << "dist - " << dist << std::endl;
				closestDist = dist;
				closestObject = obj;
			}
		}
	}
	if (closestObject)
	{
		mGameObjectLayer[(int)GameObjectLayer::Picking].push_back(closestObject);

#ifdef _DEBUG
		//std::cout << "Picked Object - " << closestObject->GetName() << "\n";
#endif
		mPicking = !mGameObjectLayer[(int)GameObjectLayer::Picking].empty();
	}

}

void DummyApp::PickingMove()
{
	if (mPicking)
	{

		int enemyOwner; unsigned char enemyObj;
		if (PickEnemyUnit(mLastMousePos.x, mLastMousePos.y,
			enemyOwner, enemyObj))
		{
			for (auto& x : mGameObjectLayer[(int)GameObjectLayer::Picking]) {
				int myObj = GetNetworkObjNumber(x);
				if (myObj >= 0)
					SendAttackRequest((unsigned char)myObj,
						(unsigned char)enemyOwner, enemyObj);
			}
			return;   // 이동 로직 건너뜀
		}


		XMFLOAT3 pickedTerrainPoint;
		if(!PickTerrainPoint(mLastMousePos.x, mLastMousePos.y, pickedTerrainPoint))
			return;

		std::vector<unsigned char> netObjNumbers;
		XMFLOAT3 netDest = pickedTerrainPoint;
		for (auto& x : mGameObjectLayer[(int)GameObjectLayer::Picking])
		{
			XMFLOAT3 destPos = pickedTerrainPoint;

			if (x->GetObjType() == ObjectsType::CHARACTER)
			{
				Player* player = dynamic_cast<Player*>(x);
				if (!player) continue;

				int netNum = GetNetworkObjNumber(x);
				if (netNum < 0) continue;

				netObjNumbers.push_back((unsigned char)netNum);

				// 멈춰있는 동적 오브젝트만 임시 장애물로 마킹
				// (이동 중인 유닛은 곧 자리를 비우므로 장애물 취급 안 함)
				mPathfinder.ClearDynamicObstacles();
				for (auto& other : mDynamicColliders)
				{
					if (other == x) continue; // 자기 자신 제외

					// 멈춰있는지 판별
					bool isStationary = true;
					if (other->GetObjType() == ObjectsType::CHARACTER)
					{
						Player* pOther = dynamic_cast<Player*>(other);
						if (pOther)
						{
							StateId st = pOther->GetLowerStateId();
							if (st == StateId::Walk || st == StateId::Run)
								isStationary = false;
						}
					}

					if (isStationary)
					{
						XMFLOAT3 otherPos = other->GetPosition();
						int gx, gz;
						mPathfinder.WorldToGrid(otherPos.x, otherPos.z, gx, gz);
						mPathfinder.SetDynamicObstacle(gx, gz);

						// 유닛 BB 크기가 셀보다 크면 주변 셀도 마킹
						float maxExt = max(other->GetBoundingBox().Extents.x,
							other->GetBoundingBox().Extents.z);
						int expand = (int)(maxExt / mPathfinder.GetCellSize());
						for (int dz = -expand; dz <= expand; ++dz)
							for (int dx = -expand; dx <= expand; ++dx)
								if (dx != 0 || dz != 0)
									mPathfinder.SetDynamicObstacle(gx + dx, gz + dz);
					}
				}

				// A* 경로 탐색
				std::vector<XMFLOAT3> path;
				bool found = mPathfinder.FindPath(
					player->GetPosition(),
					destPos,
					XMFLOAT3(
						player->GetBoundingBox().Extents.x,
						0.f,
						player->GetBoundingBox().Extents.z),
					path
				);

				// 동적 장애물 마킹 해제
				mPathfinder.ClearDynamicObstacles();

				if (found && !path.empty())
				{
					// Y 좌표를 terrain 높이로 보정
					for (auto& wp : path)
					{
						wp.y = mTerrain.GetHeight(wp.x, wp.z);
					}

					// waypoint 경로 설정
					player->SetFinalDestination(destPos);
					player->SetPath(path);
					player->SetFollowerKeyInput(FollowerKeyInput::Move);
					player->FollowerEvent();
				}
				else
				{
					player->SetFinalDestination(destPos);
					// 경로를 찾지 못함 → 직선 이동 fallback
					player->ClearPath();
					player->SetDestination(destPos);
					player->SetFollowerKeyInput(FollowerKeyInput::Move);
					player->FollowerEvent();
				}
			}
		}

		if (!netObjNumbers.empty()) {
			if (netObjNumbers.size() == 1)
				SendMoveRequest(netObjNumbers[0], netDest);
			else
				SendMultiMoveRequest(netObjNumbers, netDest);
		}
	/*	for (const auto& x : mGameObjectLayer[(int)GameObjectLayer::Object])
		{
		}*/
	}
}

bool DummyApp::PickTerrainPoint(int sx, int sy, XMFLOAT3& outPoint)
{
	XMVECTOR rayOriginVector;
	XMVECTOR rayDirectionVector;

	MathHelper::ScreenToRay(sx, sy, mClientWidth, mClientHeight, mMainCamera->GetView(), mMainCamera->GetProj(), rayOriginVector, rayDirectionVector);

	XMFLOAT3 rayOrigin;
	XMFLOAT3 rayDirection;

	XMStoreFloat3(&rayOrigin, rayOriginVector);

	XMStoreFloat3(&rayDirection, rayDirectionVector);
	const float halfWidth = mTerrain.GetWidth() * 0.5f;

	const float halfLength = mTerrain.GetLength() * 0.5f;

	// 현재 지형 셀 간격이 약 20이므로 절반 정도 사용
	const float stepDistance = 10.0f;
	const float maxDistance = mMainCamera->GetFarZ();

	bool hasPreviousSample = false;

	float previousT = 0.0f;
	float previousDifference = 0.0f;

	for (float t = 0.0f;
		t <= maxDistance;
		t += stepDistance)
	{
		XMFLOAT3 rayPoint = {
			rayOrigin.x + rayDirection.x * t,
			rayOrigin.y + rayDirection.y * t,
			rayOrigin.z + rayDirection.z * t
		};

		bool insideTerrain =
			rayPoint.x >= -halfWidth &&
			rayPoint.x <= halfWidth &&
			rayPoint.z >= -halfLength &&
			rayPoint.z <= halfLength;

		if (!insideTerrain)
		{
			hasPreviousSample = false;
			continue;
		}

		float terrainY = mTerrain.GetHeight( rayPoint.x, rayPoint.z);

		// 양수: 레이가 지형 위
		// 음수: 레이가 지형 아래
		float difference = rayPoint.y - terrainY;

		if (hasPreviousSample &&
			previousDifference > 0.0f &&
			difference <= 0.0f)
		{
			// 이전 샘플과 현재 샘플 사이에서 첫 접점 정밀 탐색
			float low = previousT;
			float high = t;

			for (int i = 0; i < 18; ++i)
			{
				float middle =
					(low + high) * 0.5f;

				float x = rayOrigin.x + rayDirection.x * middle;

				float y =
					rayOrigin.y +
					rayDirection.y * middle;

				float z =
					rayOrigin.z +
					rayDirection.z * middle;

				float height =
					mTerrain.GetHeight(x, z);

				if (y > height)
					low = middle;
				else
					high = middle;
			}

			outPoint.x =
				rayOrigin.x +
				rayDirection.x * high;

			outPoint.z =
				rayOrigin.z +
				rayDirection.z * high;

			outPoint.y =
				mTerrain.GetHeight(
					outPoint.x,
					outPoint.z);

			return true;
		}

		hasPreviousSample = true;
		previousT = t;
		previousDifference = difference;
	}
	return false;
}

void DummyApp::PickingAttackMove()
{
}

void DummyApp::PickingPatrolMove()
{
}

void DummyApp::UIPicking(WPARAM wParam)
{
	switch (wParam)
	{
	case 'O':
		mUIkey.isO = true;
		break;
	case 'B':
		mUIkey.isB = true;
		break;
	case 'U':
		//바로 확인해도 됨
		DoUpgrade();
		break;
	case 'L':
		mUIkey.isL = true;
		break;
	case 'K':
		mUIkey.isK = true;
		break;
	case 'H':
		mUIkey.isH = true;
		break;
	case 'N':
		mUIkey.isN = true;
		break;
	case 'T':
		mUIkey.isT = true;
		break;
	}
}

void DummyApp::RsetUIInput()
{
	mUIkey.isO = false;
	mUIkey.isB = false;
	mUIkey.isL = false;
	mUIkey.isU = false;
	mUIkey.isK = false;
	mUIkey.isH = false;
	mUIkey.isN = false;
	mUIkey.isT = false;
}

void DummyApp::SummonObject()
{

	std::cout << mUIkey.isB << mUIkey.isH << std::endl;

	if (mUIkey.isO) {
		if (mUIkey.isN) {
			SummonHunter();
		}
		else if (mUIkey.isL) {
			SummonSlave();
		}
		else if (mUIkey.isK) {
			SummonKnight();
		}
	}
	else if (mUIkey.isB) {
		if (mUIkey.isT) {
			//SummonTower();
		}
		else if (mUIkey.isH) {
			SummonCommandCenter();
		}
	}

}

void DummyApp::RegisterVisionObject(GameObject* obj, int ownerPlayer)
{
	if (!obj) return;

	mTeamObjects.erase(std::remove(mTeamObjects.begin(), mTeamObjects.end(), obj), mTeamObjects.end());	
	mEnemyObjects.erase(std::remove(mEnemyObjects.begin(), mEnemyObjects.end(), obj), mEnemyObjects.end());

	if (ownerPlayer == mMyPlayerNumber) {
		mTeamObjects.push_back(obj);
	}
	else {
		mEnemyObjects.push_back(obj);
	}
}

bool DummyApp::isInTeamVision(GameObject* obj) const
{
	if (!obj) return false;

	XMFLOAT3 objPos = obj->GetPosition();
	float visionRadiusSquared = VisionRadiusWorld * VisionRadiusWorld;

	for (GameObject* teamObj : mTeamObjects)
	{
		XMFLOAT3 teamPos = teamObj->GetPosition();
		float dx = objPos.x - teamPos.x;
		//float dy = objPos.y - teamPos.y;
		float dz = objPos.z - teamPos.z;
		float distanceSquared = dx * dx + dz * dz;
		if (distanceSquared <= visionRadiusSquared)
		{
			return true; // 팀 유닛의 시야 범위 안에 있음
		}
	}
}

bool DummyApp::ShouldRenderObject(GameObject* obj) const
{
	if(!mDarknessEnabled || !obj) return true;

	//GameObject* visibilityObj = obj;

	bool isEnemy = std::find(mEnemyObjects.begin(), mEnemyObjects.end(), obj) != mEnemyObjects.end();

	if (!isEnemy) return true;

	return isInTeamVision(obj);
}


void DummyApp::UpdateDarknessCB(const GameTimer& gt)
{
	DarknessConstants dc = {};

	dc.MapSize = XMFLOAT2(mTerrain.GetWidth(), mTerrain.GetLength());

	// 어두운 색, 가장자리 발광 색은 네가 쓰던 값으로
	dc.DarkColor = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.85f);
	dc.GlowColor = DirectX::XMFLOAT4(1.2f, 1.1f, 0.9f, 0.4f);

	// 월드 기준 시야 반경 (맵 스케일에 맞춰 값 조정)
	//const float visionRadiusWorld = 800.0f;

	int count = 0;

	// 네가 실제로 관리하는 유닛 리스트에 맞게 변경
	for (GameObject* obj : mTeamObjects)
	{
		if (count >= MaxFogUnits)
			break;

		// 유닛의 월드 좌표
		DirectX::XMFLOAT3 pos = obj->GetPosition();

		dc.Units[count].CenterPosRadius =
			DirectX::XMFLOAT4(pos.x, pos.y, pos.z, VisionRadiusWorld);

		++count;
	}

	dc.UnitCount = count;

	// FrameResource 안의 DarknessCB 업로드
	mCurrFrameResource->DarknessCB->CopyData(0, dc);
}

void DummyApp::BuildDarknessGeometry()
{
	struct DarkVertex
	{
		XMFLOAT2 Pos;
	};

	DarkVertex vertices[4] =
	{
		{ XMFLOAT2(-1.0f, -1.0f) }, // 좌하
		{ XMFLOAT2(-1.0f,  1.0f) }, // 좌상
		{ XMFLOAT2(1.0f,  1.0f) }, // 우상
		{ XMFLOAT2(1.0f, -1.0f) }, // 우하
	};

	uint16_t indices[6] = { 0,1,2, 0,2,3 };

	const UINT vbByteSize = sizeof(vertices);
	const UINT ibByteSize = sizeof(indices);

	mDarknessVB = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(),
		vertices, vbByteSize, mDarknessVBUpload);

	mDarknessIB = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(),
		indices, ibByteSize, mDarknessIBUpload);

	mDarknessVBView.BufferLocation = mDarknessVB->GetGPUVirtualAddress();
	mDarknessVBView.StrideInBytes = sizeof(DarkVertex);
	mDarknessVBView.SizeInBytes = vbByteSize;

	mDarknessIBView.BufferLocation = mDarknessIB->GetGPUVirtualAddress();
	mDarknessIBView.Format = DXGI_FORMAT_R16_UINT;
	mDarknessIBView.SizeInBytes = ibByteSize;
}

void DummyApp::FollowerKeyEvent()
{
	switch (mFollowerinput)
	{
	case FollowerKeyInput::None:
		AddPicking();
		break;
	case FollowerKeyInput::Move:
		PickingMove();
		break;
	case FollowerKeyInput::Patrol:
		PickingPatrolMove();
		break;
	case FollowerKeyInput::Attack:
		PickingAttackMove();
		break;
	default:
		break;
	}

	mFollowerinput = FollowerKeyInput::None;

}
void DummyApp::OnMouseWheel(WPARAM wheeldelta)
{
}

bool DummyApp::OnKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	if (mFPSmode) {
		mPlayer->OnKeyboardMessage(nMessageID, wParam);
	}
	else { //WASD QE 만 받음
		mMainCamera->OnKeyboardMessage(nMessageID, wParam);
	}

	if (nMessageID == WM_KEYDOWN) {
		switch (wParam)
		{
		case VK_F1:
			mDebugMode = !mDebugMode;
			break;
		case VK_F2:
			mDarknessEnabled = !mDarknessEnabled;
			break;
		case VK_TAB:
			if (mFPSmode) {
				mFPSmode = false;
				mRotateFlag = false;
				mMainCamera = mSubCamera[0];
				mMainCamera->ResetKeyInput();
				mMainCamera->SetPosition(Vector3::Add(XMFLOAT3(0.f, 1000.f, 0.f), mPlayer->GetPosition()));
				//mMainCamera->LookAt(mMainCamera->GetPosition3f(), mPlayer->GetPosition(), mPlayer->GetUp());
				mMainCamera->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 10.f, 30000.f);
			}
			else {
				mFPSmode = true;
				mRotateFlag = true;
				mPlayer->ResetKeyInput();
				mMainCamera->SetVelocity(XMFLOAT3(0.f, 0.f, 0.f));
				mMainCamera = mPlayer->GetCamera();
				mMainCamera->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 10.f, 30000.f);
			}
			break;
		case VK_CONTROL:

			break;
		case VK_LSHIFT:
			mSpecialKeyinput.isShift = true;
			break;
		case VK_SPACE:
			if (mFPSmode) {

			}
			else {

			}
			break;
		default:
			UIPicking(wParam);
			break;
		}
	}
	else if (nMessageID == WM_KEYUP) {
		switch (wParam)
		{
		case VK_F1:
			break;
		case VK_CONTROL:
			if (!mFPSmode) {
				mSpecialKeyinput.isCtrl = !mSpecialKeyinput.isCtrl;
				
				mDragFlag = false;
				mRotateFlag = false;

				if(mSpecialKeyinput.isCtrl){
					mIgnoreMouseMove = false;
					//mMainCamera->SetVelocity(XMFLOAT3(0.f, 0.f, 0.f));
				}
				else CenterMouseCursor();
			}
			break;
		case VK_LSHIFT:
			mSpecialKeyinput.isShift = false;
			break;
		default:
			break;
		}

	}

	return(false);
}

void DummyApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	mPlayer->KeyboardInput(dt);

	//mCamera->UpdateViewMatrix();
}

void DummyApp::AnimateMaterials(const GameTimer& gt)
{
}

void DummyApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();

	for (auto& e : mAllGameObjects)
	{
		// 무기의 경우 무기의 주인의 cbuffer를 업데이트한다.
		if (e->GetObjType() == ObjectsType::WEAPON) {
			auto k = dynamic_cast<Weapon*>(e);
			UpdateSkinnedCB(gt, k->GetOwner());
		}
		// 상수들이 바뀌었을 때에만 cbuffer 자료를 갱신한다.
		// 이러한 갱신을 프레임 자원마다 수행해야한다.
		if (e->GetFramesDirty() > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->GetWorld());
			XMMATRIX texTransform = XMLoadFloat4x4(&e->GetTexTransform());

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			for (UINT i = 0; i < e->GetNumSubmeshes(); i++)
			{
				objConstants.MaterialIndex = e->GetMeterial(i)->MatCBIndex;
				currObjectCB->CopyData(e->GetObjCBIndex(i), objConstants);
			}

			// 다음 프레임 자원으로 넘어간다.
			e->DecreaseFrameDirty();
		}
	}
}

void DummyApp::UpdateSkinnedCB(const GameTimer& gt, GameObject* skinnobj)
{
	auto currSkinnedCB = mCurrFrameResource->SkinnedCB.get();
	std::vector<XMFLOAT4X4> boneTransforms;
	SkinnedConstants skinnedConstants;

	// 스킨 메쉬의 경우 뼈의 변환 행렬을 계산한다.
	auto mSkinnedMesh = dynamic_cast<SkinnedMesh*>(skinnobj->GetMesh());
	auto mPlayer = dynamic_cast<Player*>(skinnobj);
	mSkinnedMesh->GetCombinationBoneTransforms(mPlayer->GetUpperAnimationTime(), mPlayer->GetLowerAnimationTime(),
		boneTransforms, mPlayer->GetUpperAnimationName(), mPlayer->GetLowerAnimationName());

	// 가지고있는 무기의 matrix 업데이트
	mPlayer->SetWeaponMatrix();
	int numBones = boneTransforms.size();
	for (int i = 0; i < numBones; i++)
	{
		skinnedConstants.BoneTransforms[i] = boneTransforms[i];
	}
	// 최소한 4개의 행렬을 초기화함
	if (numBones < 4) {
		for (int i = numBones; i < 4; i++)
			skinnedConstants.BoneTransforms[i] = Matrix4x4::Identity();
	}
	currSkinnedCB->CopyData(mPlayer->GetSkinnedCBIndex(), skinnedConstants);
}

void DummyApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : mMaterials)
	{
		// 상수들이 바뀌었을 때에만 cbuffer 자료를 갱신한다.
		// 이러한 갱신을 프레임 자원마다 수행해야한다.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;

			currMaterialCB->CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void DummyApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mMainCamera->GetView();
	XMMATRIX proj = mMainCamera->GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mMainCamera->GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = mMainCamera->GetNearZ();
	mMainPassCB.FarZ = mMainCamera->GetFarZ();
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	mMainPassCB.AmbientLight = { 0.2f, 0.2f, 0.2f, 1.0f };

	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	//// ============================
	//// 2) 유닛 시야용 Point Light들
	//// ============================
	//const float visionWorldRadius = 800.0f;  // 유닛이 보는 월드 거리(암흑시야랑 맞추기)
	//const float pointHeight = 5.0f;    // 라이트를 살짝 위로 올려서

	//int pointIndex = 3;         // 3부터 시작

	//for (GameObject* obj : mTeamObjects)
	//{
	//	if (!obj) continue;
	//	if (pointIndex >= 3 + 32)
	//		break;

	//	// 여기서 "내 유닛만" 골라야 함 (팀/진영 조건은 프로젝트 enum에 맞게)
	//	// 예:
	//	// if (obj->GetObjectType() != ObjectsType::ALLY_UNIT) continue;

	//	XMFLOAT3 pos = obj->GetPosition();

	//	Light& L = mMainPassCB.Lights[pointIndex++];

	//	L.Strength = XMFLOAT3(1.0f, 1.0f, 0.9f);      // 약간 노란 불
	//	L.FalloffStart = visionWorldRadius * 0.3f;       // 이 거리부터 서서히 어두워지고
	//	L.FalloffEnd = visionWorldRadius;              // 이 거리에서 0이 됨
	//	L.Position = XMFLOAT3(pos.x, pos.y + pointHeight, pos.z);
	//}

	//auto currPassCB = mCurrFrameResource->PassCB.get();
	//currPassCB->CopyData(0, mMainPassCB);
	/*
	for (int i = 0; i < 5; i++)
	{
		float fallOffStart = 2.0f;
		float fallOffEnd = 10.0f;
		XMFLOAT3 strength = { 0.7f, 0.7f, 0.7f };

		mMainPassCB.Lights[2 * i].Strength = strength;
		mMainPassCB.Lights[2 * i].FalloffStart = fallOffStart;
		mMainPassCB.Lights[2 * i].FalloffEnd = fallOffEnd;
		mMainPassCB.Lights[2 * i].Position = { -5.0f, 3.5f, -10.0f + 5.0f * i };

		mMainPassCB.Lights[2 * i + 1].Strength = strength;
		mMainPassCB.Lights[2 * i + 1].FalloffStart = fallOffStart;
		mMainPassCB.Lights[2 * i + 1].FalloffEnd = fallOffEnd;
		mMainPassCB.Lights[2 * i + 1].Position = { +5.0f, 3.5f, -10.0f + 5.0f * i };
	}
	*/

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void DummyApp::LoadTextures()
{
	std::vector<std::string> texNames =
	{
		"skyCubeMap",
		"vanguardDiffuse",
		"swordDiffuse",
		"bricksDiffuseMap",
		"stoneDiffuseMap",
		"tileDiffuseMap",
		"terrainDiffuseMap",
		"crystalDiffuse",
		"bowDiffuse",
		"hunterDiffuse",
		"commandcenterDiffuse",
		"cursor"
	};

	std::vector<std::wstring> texFilenames =
	{
		L"Textures/Environment/grasscube1024.dds",
		L"Textures/Character/Vanguard_diffuse.dds",
		L"Textures/Weapon/Sword/Sword.dds",
		L"Textures/bricks.dds",
		L"Textures/stone.dds",
		L"Textures/tile.dds",
		L"Textures/Environment/Python.dds",
		L"Textures/Environment/crystal.dds",
		L"Textures/Weapon/Bow/BowDiffuse.dds",
		L"Textures/Character/Archer_diffuse.dds",
		L"Textures/Building/CommandCenter_diffuse.dds",
		L"Textures/cursor.dds"
	};

	for (int i = 0; i < (int)texNames.size(); ++i)
	{
		// 같은 이름의 텍스처가 생기지 않도록한다.
		if (mTextures.find(texNames[i]) == std::end(mTextures))
		{
			auto texMap = std::make_unique<Texture>();
			texMap->Name = texNames[i];

			//cout << texMap->Name << endl;

			texMap->Filename = texFilenames[i];
			ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
				mCommandList.Get(), texMap->Filename.c_str(),
				texMap->Resource, texMap->UploadHeap));

			mTextures[texMap->Name] = std::move(texMap);
		}
	}
}

void DummyApp::BuildRootSignature()
{
	// 일반적으로 셰이더 프로그램은 특정 자원들(상수 버퍼, 텍스처, 표본추출기 등)이 입력된다고 기대한다.
	// 루트 서명은 셰이더 프로그램이 기대하는 자원들을 정의한다.
	// 셰이더 프로그램은 본질적으로 하나의 함수이고 셰이더에 입력되는 자원들은 
	// 함수의 매개변수들에 해당하므로, 루트 서명은 곧 함수 서명을 정의하는 수단이라 할 수 있다.

// SkyCubeMap용 SRV 테이블 (t0, space0)
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	// Diffuse 텍스처들용 SRV 테이블 (t1~t20, space0)
	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 20, 1, 0);

	// 깊이 텍스처용 SRV 테이블 (t0, space2)
	CD3DX12_DESCRIPTOR_RANGE depthTable;
	depthTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 2);

	// 루트 파라미터는 총 7개
	CD3DX12_ROOT_PARAMETER slotRootParameter[7];

	// 0: ObjectCB (b0)
	slotRootParameter[0].InitAsConstantBufferView(0);

	// 1: DarknessCB / MaterialCB 등 (b1) – Darkness에서는 b1을 사용
	slotRootParameter[1].InitAsConstantBufferView(1);

	// 2: PassCB (b2) – gInvViewProj 사용
	slotRootParameter[2].InitAsConstantBufferView(2);

	// 3: StructuredBuffer 등용 SRV (t0, space1) – 기존 그대로
	slotRootParameter[3].InitAsShaderResourceView(0, 1);

	// 4: SkyCubeMap (t0, space0)
	slotRootParameter[4].InitAsDescriptorTable(
		1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);

	// 5: Diffuse 텍스처 배열 (t1~, space0)
	slotRootParameter[5].InitAsDescriptorTable(
		1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	// 6: 깊이 텍스처 (t0, space2)
	slotRootParameter[6].InitAsDescriptorTable(
		1, &depthTable, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		7, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void DummyApp::BuildDescriptorHeaps()
{
	// CBV, SRV, UAV를 저장할수있고, 셰이더들이 접근할 수 있는 힙을 생성
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = mTextures.size() + 1; // +1 depth SRV
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	// 텍스처 자원이 이미 로드되어 있을때

	// 힙의 시작을 가리키는 포인터를 얻는다.
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	CD3DX12_GPU_DESCRIPTOR_HANDLE gDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	auto swordTex = mTextures["swordDiffuse"]->Resource;
	auto vanguardTex = mTextures["vanguardDiffuse"]->Resource;

	auto hunterTex = mTextures["hunterDiffuse"]->Resource;

	auto bricksTex = mTextures["bricksDiffuseMap"]->Resource;
	auto stoneTex = mTextures["stoneDiffuseMap"]->Resource;
	auto tileTex = mTextures["tileDiffuseMap"]->Resource;
	auto terrainTex = mTextures["terrainDiffuseMap"]->Resource;

	auto crystalTex = mTextures["crystalDiffuse"]->Resource;
	//auto test = mTextures["test"]->Resource;
	auto skyTex = mTextures["skyCubeMap"]->Resource;

	auto bowTex = mTextures["bowDiffuse"]->Resource;

	auto CommandCenterTex = mTextures["commandcenterDiffuse"]->Resource;

	auto cursorTex = mTextures["cursor"]->Resource;

	// 텍스처에 대한 실제 서술자들을 앞에서 생성한 힙에 생성한다.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	// skyCubeMap 디스크립터 생성
	// 텍스처 큐브 디스크립터
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = skyTex->GetDesc().MipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = skyTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(skyTex.Get(), &srvDesc, hDescriptor);

	///// 2D 텍스쳐 디스크립터 기본

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// 다음 서술자로 넘어간다.
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = swordTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = swordTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(swordTex.Get(), &srvDesc, hDescriptor);

	// 다음 서술자로 넘어간다.
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = vanguardTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = vanguardTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(vanguardTex.Get(), &srvDesc, hDescriptor);

	// 다음 서술자로 넘어간다.
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = bricksTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

	// 다음 서술자로 넘어간다.
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = stoneTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = stoneTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(stoneTex.Get(), &srvDesc, hDescriptor);

	// 다음 서술자로 넘어간다.
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = tileTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = tileTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(tileTex.Get(), &srvDesc, hDescriptor);

	// 다음 서술자로 넘어간다.
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = terrainTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = terrainTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(terrainTex.Get(), &srvDesc, hDescriptor);

	////// 다음 서술자로 넘어간다.
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = crystalTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = crystalTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(crystalTex.Get(), &srvDesc, hDescriptor);

	////// 다음 서술자로 넘어간다.
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = bowTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = bowTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(bowTex.Get(), &srvDesc, hDescriptor);

	//// 다음 서술자로 넘어간다.
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = hunterTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = hunterTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(hunterTex.Get(), &srvDesc, hDescriptor);

	//// 다음 서술자로 넘어간다.
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = CommandCenterTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = CommandCenterTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(CommandCenterTex.Get(), &srvDesc, hDescriptor);
	
	//// 다음 서술자로 넘어간다.
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = cursorTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = cursorTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(cursorTex.Get(), &srvDesc, hDescriptor);






	// --- Depth SRV는 힙의 맨 끝 1개를 고정 인덱스로 사용한다.
	//   depthIndex = mTextures.size()
	const int depthIndex = (int)mTextures.size();

	// Depth SRV (t0, space2)
	D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
	depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // D24S8 SRV 포맷
	depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	depthSrvDesc.Texture2D.MostDetailedMip = 0;
	depthSrvDesc.Texture2D.MipLevels = 1;

	CD3DX12_CPU_DESCRIPTOR_HANDLE depthCpu(
		mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		depthIndex,
		mCbvSrvDescriptorSize);

	md3dDevice->CreateShaderResourceView(
		mDepthStencilBuffer.Get(),
		&depthSrvDesc,
		depthCpu);

	// GPU handles 저장
	mDepthSrvGpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
		mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
		depthIndex,
		mCbvSrvDescriptorSize);

}
void DummyApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"SKINNED", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["toonLightingOpaquePS"] = d3dUtil::CompileShader(L"Shaders/ToonLighting.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", skinnedDefines, "VS", "vs_5_1");

	mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders/Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders/Sky.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["colorVS"] = d3dUtil::CompileShader(L"Shaders/not light/Color.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["colorPS"] = d3dUtil::CompileShader(L"Shaders/not light/Color.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["UIVS"] = d3dUtil::CompileShader(L"Shaders/UIShader.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["UIPS"] = d3dUtil::CompileShader(L"Shaders/UIShader.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["selectionVS"] = d3dUtil::CompileShader(L"Shaders/SelectionRect.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["selectionPS"] = d3dUtil::CompileShader(L"Shaders/SelectionRect.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["darknessVS"] = d3dUtil::CompileShader(L"Shaders/Darkness.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["darknessPS"] = d3dUtil::CompileShader(L"Shaders/Darkness.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mColorInputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	mSkinnedInputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		/*
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }*/
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mUIInputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	mSelectionInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
		  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mDarknessInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
		  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

}

void DummyApp::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 0);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	Submesh boxSubmesh;
	boxSubmesh.name = string("box");
	boxSubmesh.numIndices = (UINT)box.Indices32.size();
	boxSubmesh.baseIndex = boxIndexOffset;
	boxSubmesh.baseVertex = boxVertexOffset;

	Submesh gridSubmesh;
	gridSubmesh.name = string("grid");
	gridSubmesh.numIndices = (UINT)grid.Indices32.size();
	gridSubmesh.baseIndex = gridIndexOffset;
	gridSubmesh.baseVertex = gridVertexOffset;

	Submesh sphereSubmesh;
	sphereSubmesh.name = string("sphere");
	sphereSubmesh.numIndices = (UINT)sphere.Indices32.size();
	sphereSubmesh.baseIndex = sphereIndexOffset;
	sphereSubmesh.baseVertex = sphereVertexOffset;

	Submesh cylinderSubmesh;
	cylinderSubmesh.name = string("cylinder");
	cylinderSubmesh.numIndices = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.baseIndex = cylinderIndexOffset;
	cylinderSubmesh.baseVertex = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

	std::vector<UINT> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(UINT);

	Mesh* ShapeGeometryMesh = new Mesh;
	ShapeGeometryMesh->mName = "shapeGeo";

	ShapeGeometryMesh->CreateBlob(vertices, indices);
	ShapeGeometryMesh->UploadBuffer(md3dDevice.Get(), mCommandList.Get(), vertices, indices);

	ShapeGeometryMesh->mSubmeshes.resize(4);
	ShapeGeometryMesh->mSubmeshes[0] = boxSubmesh;
	ShapeGeometryMesh->mSubmeshes[1] = gridSubmesh;
	ShapeGeometryMesh->mSubmeshes[2] = sphereSubmesh;
	ShapeGeometryMesh->mSubmeshes[3] = cylinderSubmesh;

	mMeshes[ShapeGeometryMesh->mName] = (ShapeGeometryMesh);

}

void DummyApp::LoadSkinnedMesh()
{
	//Attacker
	{
		auto mSkinnedMesh = new SkinnedMesh;

		mSkinnedMesh->SetOffsetMatrix(XMFLOAT3(0.0f, 1.0f, 0.0f), 180.f);
		mSkinnedMesh->LoadMesh("Models/Character/Vanguard.fbx");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/Idle.fbx", "Idle");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/WalkForward.fbx", "WalkForward");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/WalkBack.fbx", "WalkBack");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/StrafeRight1.fbx", "WalkRight1");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/StrafeRight2.fbx", "WalkRight2");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/StrafeLeft1.fbx", "WalkLeft1");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/StrafeLeft2.fbx", "WalkLeft2");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/RunForward.fbx", "RunForward");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/Jump.fbx", "Jump");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/Falling.fbx", "Falling");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/Landing.fbx", "Landing");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/MeleeAttack1.fbx", "MeleeAttack1");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/MeleeAttack2.fbx", "MeleeAttack2");

		UINT vcount = 0;
		UINT tcount = 0;
		std::vector<SkinnedVertex> vertices;
		std::vector<UINT> indices;
		UINT index;
		UINT dindex = 0;

		UINT numVertices = mSkinnedMesh->mPositions.size();
		for (int j = 0; j < numVertices; j++)
		{
			SkinnedVertex vertex;
			vertex.Pos.x = mSkinnedMesh->mPositions[j].x;
			vertex.Pos.y = mSkinnedMesh->mPositions[j].y;
			vertex.Pos.z = mSkinnedMesh->mPositions[j].z;

			vertex.Normal.x = mSkinnedMesh->mNormals[j].x;
			vertex.Normal.y = mSkinnedMesh->mNormals[j].y;
			vertex.Normal.z = mSkinnedMesh->mNormals[j].z;

			vertex.TexC.x = mSkinnedMesh->mTexCoords[j].x;
			vertex.TexC.y = mSkinnedMesh->mTexCoords[j].y;

			vertices.push_back(vertex);
		}

		for (int i = 0; i < numVertices; i++)
		{
			vertices[i].BoneIndices[0] = (BYTE)mSkinnedMesh->mBones[i].BoneIDs[0];
			vertices[i].BoneIndices[1] = (BYTE)mSkinnedMesh->mBones[i].BoneIDs[1];
			vertices[i].BoneIndices[2] = (BYTE)mSkinnedMesh->mBones[i].BoneIDs[2];
			vertices[i].BoneIndices[3] = (BYTE)mSkinnedMesh->mBones[i].BoneIDs[3];

			float weights = mSkinnedMesh->mBones[i].Weights[0] + mSkinnedMesh->mBones[i].Weights[1] + mSkinnedMesh->mBones[i].Weights[2] + mSkinnedMesh->mBones[i].Weights[3];

			vertices[i].BoneWeights.x = mSkinnedMesh->mBones[i].Weights[0] / weights;
			vertices[i].BoneWeights.y = mSkinnedMesh->mBones[i].Weights[1] / weights;
			vertices[i].BoneWeights.z = mSkinnedMesh->mBones[i].Weights[2] / weights;
		}

		UINT numIndices = mSkinnedMesh->mIndices.size();
		for (UINT i = 0; i < numIndices; i++)
		{
			indices.push_back(mSkinnedMesh->mIndices[i]);
		}

		mSkinnedMesh->mName = "Vanguard";

		mSkinnedMesh->CreateBlob(vertices, indices);
		mSkinnedMesh->UploadBuffer(md3dDevice.Get(), mCommandList.Get(), vertices, indices);

		mMeshes[mSkinnedMesh->mName] = mSkinnedMesh;
	}

	{
		auto mSkinnedMesh = new SkinnedMesh;

		mSkinnedMesh->SetOffsetMatrix(XMFLOAT3(0.0f, 1.0f, 0.0f), 180.f);
		mSkinnedMesh->LoadMesh("Models/Character/Archer.fbx");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/Idle.fbx", "Idle");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/WalkForward.fbx", "WalkForward");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/WalkBack.fbx", "WalkBack");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/StrafeRight1.fbx", "WalkRight1");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/StrafeRight2.fbx", "WalkRight2");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/StrafeLeft1.fbx", "WalkLeft1");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/StrafeLeft2.fbx", "WalkLeft2");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/RunForward.fbx", "RunForward");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/Jump.fbx", "Jump");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/Falling.fbx", "Falling");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/Landing.fbx", "Landing");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/MeleeAttack1.fbx", "MeleeAttack1");
		mSkinnedMesh->LoadAnimation("Models/Character/Animations/MeleeAttack2.fbx", "MeleeAttack2");

		UINT vcount = 0;
		UINT tcount = 0;
		std::vector<SkinnedVertex> vertices;
		std::vector<UINT> indices;
		UINT index;
		UINT dindex = 0;

		UINT numVertices = mSkinnedMesh->mPositions.size();
		for (int j = 0; j < numVertices; j++)
		{
			SkinnedVertex vertex;
			vertex.Pos.x = mSkinnedMesh->mPositions[j].x;
			vertex.Pos.y = mSkinnedMesh->mPositions[j].y;
			vertex.Pos.z = mSkinnedMesh->mPositions[j].z;

			vertex.Normal.x = mSkinnedMesh->mNormals[j].x;
			vertex.Normal.y = mSkinnedMesh->mNormals[j].y;
			vertex.Normal.z = mSkinnedMesh->mNormals[j].z;

			vertex.TexC.x = mSkinnedMesh->mTexCoords[j].x;
			vertex.TexC.y = mSkinnedMesh->mTexCoords[j].y;

			vertices.push_back(vertex);
		}

		for (int i = 0; i < numVertices; i++)
		{
			vertices[i].BoneIndices[0] = (BYTE)mSkinnedMesh->mBones[i].BoneIDs[0];
			vertices[i].BoneIndices[1] = (BYTE)mSkinnedMesh->mBones[i].BoneIDs[1];
			vertices[i].BoneIndices[2] = (BYTE)mSkinnedMesh->mBones[i].BoneIDs[2];
			vertices[i].BoneIndices[3] = (BYTE)mSkinnedMesh->mBones[i].BoneIDs[3];

			float weights = mSkinnedMesh->mBones[i].Weights[0] + mSkinnedMesh->mBones[i].Weights[1] + mSkinnedMesh->mBones[i].Weights[2] + mSkinnedMesh->mBones[i].Weights[3];

			vertices[i].BoneWeights.x = mSkinnedMesh->mBones[i].Weights[0] / weights;
			vertices[i].BoneWeights.y = mSkinnedMesh->mBones[i].Weights[1] / weights;
			vertices[i].BoneWeights.z = mSkinnedMesh->mBones[i].Weights[2] / weights;
		}

		UINT numIndices = mSkinnedMesh->mIndices.size();
		for (UINT i = 0; i < numIndices; i++)
		{
			indices.push_back(mSkinnedMesh->mIndices[i]);
		}

		mSkinnedMesh->mName = "Hunter";

		mSkinnedMesh->CreateBlob(vertices, indices);
		mSkinnedMesh->UploadBuffer(md3dDevice.Get(), mCommandList.Get(), vertices, indices);

		mMeshes[mSkinnedMesh->mName] = std::move(mSkinnedMesh);
	}
}

void DummyApp::LoadMeshes()
{
	//Sword Mesh
	{
		Mesh* swordMesh = new Mesh;

		swordMesh->SetOffsetMatrix(XMFLOAT3(0.0f, 1.0f, 0.0f), 180.f);
		swordMesh->LoadMesh("Models/Weapon/Sword.fbx");

		UINT vcount = 0;
		UINT tcount = 0;
		std::vector<Vertex> vertices;
		std::vector<UINT> indices;
		UINT index;
		UINT dindex = 0;

		XMFLOAT3 axis = XMFLOAT3(0.0f, 1.0f, 0.0f);
		XMMATRIX offsetMat = XMMatrixScaling(7.0f, 7.0f, 7.0f);

		UINT numVertices = swordMesh->mPositions.size();
		for (int j = 0; j < numVertices; j++)
		{
			Vertex vertex;
			vertex.Pos.x = swordMesh->mPositions[j].x;
			vertex.Pos.y = swordMesh->mPositions[j].y;
			vertex.Pos.z = swordMesh->mPositions[j].z;
			XMStoreFloat3(&vertex.Pos, XMVector3Transform(XMLoadFloat3(&vertex.Pos), offsetMat));

			vertex.Normal.x = swordMesh->mNormals[j].x;
			vertex.Normal.y = swordMesh->mNormals[j].y;
			vertex.Normal.z = swordMesh->mNormals[j].z;
			XMStoreFloat3(&vertex.Normal, XMVector3TransformNormal(XMLoadFloat3(&vertex.Normal), offsetMat));

			vertex.TexC.x = swordMesh->mTexCoords[j].x;
			vertex.TexC.y = swordMesh->mTexCoords[j].y;

			vertices.push_back(vertex);
		}

		UINT numIndices = swordMesh->mIndices.size();
		for (UINT i = 0; i < numIndices; i++)
		{
			indices.push_back(swordMesh->mIndices[i]);
		}

		//
		// Pack the indices of all the meshes into one index buffer.
		swordMesh->mName = "Sword";

		swordMesh->CreateBlob(vertices, indices);
		swordMesh->UploadBuffer(md3dDevice.Get(), mCommandList.Get(), vertices, indices);

		swordMesh->mSubmeshes[0].name = "sword";

		mMeshes[swordMesh->mName] = swordMesh;
	}

	//Crystal Mesh
	{
		Mesh* crystalMesh = new Mesh;

		crystalMesh->SetOffsetMatrix(XMFLOAT3(0.0f, 1.0f, 0.0f), 0.f);
		crystalMesh->LoadMesh("Models/Environment/crystal.fbx");
		//crystalMesh->LoadMesh("Models/mech.fbx");

		UINT vcount = 0;
		UINT tcount = 0;
		std::vector<Vertex> vertices;
		std::vector<UINT> indices;
		UINT index;
		UINT dindex = 0;

		XMFLOAT3 axis = XMFLOAT3(0.0f, 1.0f, 0.0f);
		XMMATRIX offsetMat = XMMatrixScaling(7.0f, 7.0f, 7.0f);

		UINT numVertices = crystalMesh->mPositions.size();
		for (int j = 0; j < numVertices; j++)
		{
			Vertex vertex;
			vertex.Pos.x = crystalMesh->mPositions[j].x;
			vertex.Pos.y = crystalMesh->mPositions[j].y;
			vertex.Pos.z = crystalMesh->mPositions[j].z;
			XMStoreFloat3(&vertex.Pos, XMVector3Transform(XMLoadFloat3(&vertex.Pos), offsetMat));

			vertex.Normal.x = crystalMesh->mNormals[j].x;
			vertex.Normal.y = crystalMesh->mNormals[j].y;
			vertex.Normal.z = crystalMesh->mNormals[j].z;
			XMStoreFloat3(&vertex.Normal, XMVector3TransformNormal(XMLoadFloat3(&vertex.Normal), offsetMat));

			vertex.TexC.x = crystalMesh->mTexCoords[j].x;
			vertex.TexC.y = crystalMesh->mTexCoords[j].y;

			vertices.push_back(vertex);
		}

		UINT numIndices = crystalMesh->mIndices.size();
		for (UINT i = 0; i < numIndices; i++)
		{
			indices.push_back(crystalMesh->mIndices[i]);
		}

		//
		// Pack the indices of all the meshes into one index buffer.
		crystalMesh->mName = "Crystal";

		crystalMesh->CreateBlob(vertices, indices);
		crystalMesh->UploadBuffer(md3dDevice.Get(), mCommandList.Get(), vertices, indices);

		crystalMesh->mSubmeshes[0].name = "crystal";

		mMeshes[crystalMesh->mName] = crystalMesh;
	}

	//Bow Mesh
	{
		Mesh* bowMesh = new Mesh;

		bowMesh->SetOffsetMatrix(XMFLOAT3(0.0f, 1.0f, 0.0f), 90.f);
		bowMesh->LoadMesh("Models/Weapon/WoodenBow.fbx");

		UINT vcount = 0;
		UINT tcount = 0;
		std::vector<Vertex> vertices;
		std::vector<UINT> indices;
		UINT index;
		UINT dindex = 0;

		XMMATRIX offsetMat = XMMatrixScaling(100.0f, 100.0f, 100.0f);

		UINT numVertices = bowMesh->mPositions.size();
		for (int j = 0; j < numVertices; j++)
		{
			Vertex vertex;
			vertex.Pos.x = bowMesh->mPositions[j].x;
			vertex.Pos.y = bowMesh->mPositions[j].y;
			vertex.Pos.z = bowMesh->mPositions[j].z;
			XMStoreFloat3(&vertex.Pos, XMVector3Transform(XMLoadFloat3(&vertex.Pos), offsetMat));

			vertex.Normal.x = bowMesh->mNormals[j].x;
			vertex.Normal.y = bowMesh->mNormals[j].y;
			vertex.Normal.z = bowMesh->mNormals[j].z;
			XMStoreFloat3(&vertex.Normal, XMVector3TransformNormal(XMLoadFloat3(&vertex.Normal), offsetMat));

			vertex.TexC.x = bowMesh->mTexCoords[j].x;
			vertex.TexC.y = bowMesh->mTexCoords[j].y;

			vertices.push_back(vertex);
		}

		UINT numIndices = bowMesh->mIndices.size();
		for (UINT i = 0; i < numIndices; i++)
		{
			indices.push_back(bowMesh->mIndices[i]);
		}


		//
		// Pack the indices of all the meshes into one index buffer.
		bowMesh->mName = "Bow";

		bowMesh->CreateBlob(vertices, indices);
		bowMesh->UploadBuffer(md3dDevice.Get(), mCommandList.Get(), vertices, indices);

		bowMesh->mSubmeshes[0].name = "bow";

		mMeshes[bowMesh->mName] = bowMesh;
	}

	//Load CommandCenter Mesh
	{
		Mesh* commandcenterMesh = new Mesh;

		commandcenterMesh->SetOffsetMatrix(XMFLOAT3(0.0f, 1.0f, 0.0f), 0.f);
		commandcenterMesh->LoadMesh("Models/Building/CommandCenter.fbx");
		//commandcenterMesh->LoadMesh("Models/mech.fbx");
		UINT vcount = 0;
		UINT tcount = 0;
		std::vector<Vertex> vertices;
		std::vector<UINT> indices;
		UINT index;
		UINT dindex = 0;

		XMFLOAT3 axis = XMFLOAT3(0.0f, 1.0f, 0.0f);
		XMMATRIX offsetMat = XMMatrixRotationX(XM_PIDIV2) * XMMatrixScaling(1.5f, 1.5f, 1.5f);

		UINT numVertices = commandcenterMesh->mPositions.size();
		for (int j = 0; j < numVertices; j++)
		{
			Vertex vertex;
			vertex.Pos.x = commandcenterMesh->mPositions[j].x;
			vertex.Pos.y = commandcenterMesh->mPositions[j].y;
			vertex.Pos.z = commandcenterMesh->mPositions[j].z;
			XMStoreFloat3(&vertex.Pos, XMVector3Transform(XMLoadFloat3(&vertex.Pos), offsetMat));

			vertex.Normal.x = commandcenterMesh->mNormals[j].x;
			vertex.Normal.y = commandcenterMesh->mNormals[j].y;
			vertex.Normal.z = commandcenterMesh->mNormals[j].z;
			XMStoreFloat3(&vertex.Normal, XMVector3TransformNormal(XMLoadFloat3(&vertex.Normal), offsetMat));

			vertex.TexC.x = commandcenterMesh->mTexCoords[j].x;
			vertex.TexC.y = commandcenterMesh->mTexCoords[j].y;

			vertices.push_back(vertex);
		}

		UINT numIndices = commandcenterMesh->mIndices.size();
		for (UINT i = 0; i < numIndices; i++)
		{
			indices.push_back(commandcenterMesh->mIndices[i]);
		}

		//
	// Pack the indices of all the meshes into one index buffer.
		commandcenterMesh->mName = "CommandCenter";

		commandcenterMesh->CreateBlob(vertices, indices);
		commandcenterMesh->UploadBuffer(md3dDevice.Get(), mCommandList.Get(), vertices, indices);

		commandcenterMesh->mSubmeshes[0].name = "commandcenter";

		mMeshes[commandcenterMesh->mName] = commandcenterMesh;
	}
	LoadSkinnedMesh();
}

void DummyApp::LoadTerrain()
{
	mTerrain.LoadHeightMap(L"HeightMap/1sec127.r16", 1017, 1017, 1.f);

	UINT vcount = 1017 * 1017;
	UINT tcount = 1017 * 1017 * 2 * 3;

	//
	// Pack the indices of all the meshes into one index buffer.
	//

	std::vector<Vertex> vertices(vcount);
	std::vector<uint32_t> indices(tcount);

	mTerrain.CreateTerrain(20000.0f, 20000.f, vertices, indices);
	std::cout << mTerrain.GetSize() << std::endl;
	Mesh* terrainMesh = new Mesh;
	terrainMesh->mName = "terrain";

	terrainMesh->CreateBlob(vertices, indices);
	terrainMesh->UploadBuffer(md3dDevice.Get(), mCommandList.Get(), vertices, indices);

	terrainMesh->AddSubmesh("terrain", indices.size());

	mMeshes[terrainMesh->mName] = terrainMesh;
}

void DummyApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), mShaders["standardVS"]->GetBufferSize() };
	opaquePsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()), mShaders["opaquePS"]->GetBufferSize() };
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc,
		IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// PSO for opaque wireframe objects.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc,
		IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));

	//
	// PSO for skinned pass.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePsoDesc;
	skinnedOpaquePsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	skinnedOpaquePsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()), mShaders["skinnedVS"]->GetBufferSize() };
	skinnedOpaquePsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()), mShaders["opaquePS"]->GetBufferSize() };
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc, IID_PPV_ARGS(&mPSOs["skinnedOpaque"])));

	//
	// PSO for toon shading.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC toonShadingPsoDesc = opaquePsoDesc;
	toonShadingPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["toonLightingOpaquePS"]->GetBufferPointer()),
		mShaders["toonLightingOpaquePS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&toonShadingPsoDesc,
		IID_PPV_ARGS(&mPSOs["opaque_toonShading"])));

	//
	// PSO for sky.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;

	// 카메라가 스카이 박스 안에 있기때문에 컬링을 비활성화한다.
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	// 깊이 판정 통과를 위해 깊이비교를 LESS_EQUAL로 설정한다.
	// LESS가 아닌 이유: 만약 깊이 버퍼를 1로 지우는 경우 z = 1에서 정규화된 깊이값이 깊이 판정에 실패한다.
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.pRootSignature = mRootSignature.Get();
	skyPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()), mShaders["skyVS"]->GetBufferSize() };
	skyPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()), mShaders["skyPS"]->GetBufferSize() };
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

	//D3D12_GRAPHICS_PIPELINE_STATE_DESC cursorPsoDesc = opaquePsoDesc;
	//cursorPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	//cursorPsoDesc.pRootSignature = mRootSignature.Get();
	//cursorPsoDesc.VS = { mShaders["UIVS"]->GetBufferPointer(),
	//					 mShaders["UIVS"]->GetBufferSize() };
	//cursorPsoDesc.PS = { mShaders["UIPS"]->GetBufferPointer(),
	//					 mShaders["UIPS"]->GetBufferSize() };

	//cursorPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//cursorPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	//cursorPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	//cursorPsoDesc.DepthStencilState.DepthEnable = FALSE;

	//ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&cursorPsoDesc, IID_PPV_ARGS(&mPSOs["cursor"])));

	//
	// PSO for debug(line)
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = opaquePsoDesc;
	debugPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	debugPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	debugPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	debugPsoDesc.InputLayout = { mColorInputLayout.data(), (UINT)mColorInputLayout.size() };
	debugPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["colorVS"]->GetBufferPointer()), mShaders["colorVS"]->GetBufferSize() };
	debugPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["colorPS"]->GetBufferPointer()), mShaders["colorPS"]->GetBufferSize() };
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc,
		IID_PPV_ARGS(&mPSOs["debug"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC uiPsoDesc = {};
	uiPsoDesc.InputLayout = { mUIInputLayout.data(), (UINT)mUIInputLayout.size() };
	uiPsoDesc.pRootSignature = mRootSignature.Get();
	uiPsoDesc.VS = { mShaders["UIVS"]->GetBufferPointer(), mShaders["UIVS"]->GetBufferSize() };
	uiPsoDesc.PS = { mShaders["UIPS"]->GetBufferPointer(), mShaders["UIPS"]->GetBufferSize() };
	uiPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	uiPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	uiPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	uiPsoDesc.DepthStencilState.DepthEnable = FALSE; // ★ 깊이 끔
	uiPsoDesc.SampleMask = UINT_MAX;
	uiPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	uiPsoDesc.NumRenderTargets = 1;
	uiPsoDesc.RTVFormats[0] = mBackBufferFormat;
	uiPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	uiPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&uiPsoDesc, IID_PPV_ARGS(&mPSOs["ui"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC selPsoDesc = {};
	selPsoDesc.InputLayout = { mSelectionInputLayout.data(),
							   (UINT)mSelectionInputLayout.size() };
	selPsoDesc.pRootSignature = mRootSignature.Get();
	selPsoDesc.VS = { mShaders["selectionVS"]->GetBufferPointer(),
					  mShaders["selectionVS"]->GetBufferSize() };
	selPsoDesc.PS = { mShaders["selectionPS"]->GetBufferPointer(),
					  mShaders["selectionPS"]->GetBufferSize() };
	selPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	// selPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; // 이건 필요 없음
	selPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	selPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	selPsoDesc.DepthStencilState.DepthEnable = FALSE;
	selPsoDesc.SampleMask = UINT_MAX;

	// ★ 여기! 라인용으로 변경
	selPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

	selPsoDesc.NumRenderTargets = 1;
	selPsoDesc.RTVFormats[0] = mBackBufferFormat;
	selPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	selPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
		&selPsoDesc, IID_PPV_ARGS(&mPSOs["selection"])));

	//// 
	//// ui
	////
	//skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	//skyPsoDesc.pRootSignature = mRootSignature.Get();
	//skyPsoDesc.VS = {
	//	reinterpret_cast<BYTE*>(mShaders["UIVS"]->GetBufferPointer()), mShaders["UIVS"]->GetBufferSize() };
	//skyPsoDesc.PS = {
	//	reinterpret_cast<BYTE*>(mShaders["UIPS"]->GetBufferPointer()), mShaders["UIPS"]->GetBufferSize() };
	//ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["ui"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC darknessPsoDesc = {};
	darknessPsoDesc.InputLayout = { mDarknessInputLayout.data(), (UINT)mDarknessInputLayout.size() };
	darknessPsoDesc.pRootSignature = mRootSignature.Get();
	darknessPsoDesc.VS = {
		mShaders["darknessVS"]->GetBufferPointer(),
		mShaders["darknessVS"]->GetBufferSize() };
	darknessPsoDesc.PS = {
		mShaders["darknessPS"]->GetBufferPointer(),
		mShaders["darknessPS"]->GetBufferSize() };

	darknessPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	// 알파 블렌딩 켜기
	auto blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	darknessPsoDesc.BlendState = blendDesc;

	darknessPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	darknessPsoDesc.DepthStencilState.DepthEnable = FALSE; // UI처럼 깊이 끔
	darknessPsoDesc.SampleMask = UINT_MAX;
	darknessPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	darknessPsoDesc.NumRenderTargets = 1;
	darknessPsoDesc.RTVFormats[0] = mBackBufferFormat;
	darknessPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	darknessPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	darknessPsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
		&darknessPsoDesc, IID_PPV_ARGS(&mPSOs["darkness"])));

}

void DummyApp::BuildFrameResources()
{
	UINT numObjCBs = 0;
	for (auto& gameObj : mAllGameObjects)
		numObjCBs += gameObj->GetNumSubmeshes();

	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1,
			numObjCBs + 800,
			800, // skinned obj
			(UINT)mMaterials.size()));
	}
}

void DummyApp::BuildMaterials()
{
	int matCBIndex = 0;
	int SRVIndex = 1;

	auto sky = std::make_unique<Material>();
	sky->Name = "sky";
	sky->MatCBIndex = matCBIndex++;
	sky->DiffuseSrvHeapIndex = mSkyTexHeapIndex; // SkyTexHeapIndex == 0
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;

	mMaterials["sky"] = std::move(sky);

	//int SRVIndex = 0; //이걸 1로 두고 나중에 한다면?

	auto sword = std::make_unique<Material>();
	sword->Name = "sword";
	sword->MatCBIndex = matCBIndex++;
	sword->DiffuseSrvHeapIndex = SRVIndex++;
	sword->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sword->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	sword->Roughness = 0.1f;

	mMaterials["sword"] = std::move(sword);

	auto vanguard = std::make_unique<Material>();
	vanguard->Name = "vanguardDiffuse";
	vanguard->MatCBIndex = matCBIndex++;
	vanguard->DiffuseSrvHeapIndex = SRVIndex++;
	vanguard->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vanguard->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	vanguard->Roughness = 0.1f;

	mMaterials["vanguard"] = std::move(vanguard);

	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = matCBIndex++;
	bricks0->DiffuseSrvHeapIndex = SRVIndex++;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	mMaterials["bricks0"] = std::move(bricks0);

	auto stone0 = std::make_unique<Material>();
	stone0->Name = "stone0";
	stone0->MatCBIndex = matCBIndex++;
	stone0->DiffuseSrvHeapIndex = SRVIndex++;
	stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	stone0->Roughness = 0.3f;

	mMaterials["stone0"] = std::move(stone0);

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	tile0->MatCBIndex = matCBIndex++;
	tile0->DiffuseSrvHeapIndex = SRVIndex++;
	tile0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.3f;

	mMaterials["tile0"] = std::move(tile0);

	auto terrainMat = std::make_unique<Material>();
	terrainMat->Name = "terrainMat";
	terrainMat->MatCBIndex = matCBIndex++;
	terrainMat->DiffuseSrvHeapIndex = SRVIndex++;
	terrainMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	terrainMat->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	terrainMat->Roughness = 0.05f;

	mMaterials["terrainMat"] = std::move(terrainMat);

	auto crystal = std::make_unique<Material>();
	crystal->Name = "crystal";
	crystal->MatCBIndex = matCBIndex++;
	crystal->DiffuseSrvHeapIndex = SRVIndex++;
	crystal->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	crystal->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	crystal->Roughness = 0.1f;

	mMaterials["crystal"] = std::move(crystal);

	auto bow = std::make_unique<Material>();
	bow->Name = "bow";
	bow->MatCBIndex = matCBIndex++;
	bow->DiffuseSrvHeapIndex = SRVIndex++;
	bow->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bow->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bow->Roughness = 0.1f;

	mMaterials["bow"] = std::move(bow);

	auto hunter = std::make_unique<Material>();
	hunter->Name = "hunterDiffuse";
	hunter->MatCBIndex = matCBIndex++;
	hunter->DiffuseSrvHeapIndex = SRVIndex++;
	hunter->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	hunter->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	hunter->Roughness = 0.1f;

	mMaterials["hunter"] = std::move(hunter);

	auto commandCenter = std::make_unique<Material>();
	commandCenter->Name = "commandcenterDiffuse";
	commandCenter->MatCBIndex = matCBIndex++;
	commandCenter->DiffuseSrvHeapIndex = SRVIndex++;
	commandCenter->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	commandCenter->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	commandCenter->Roughness = 0.1f;

	mMaterials["commandCenter"] = std::move(commandCenter);

	//Cursor Material는 무조건 마지막에 있어야한다.
	mCursorTexHeapIndex = SRVIndex++;
}

void DummyApp::BuildGameObjects()
{

	//Submesh submesh;

	// ------------------------------------------
	// sky sphere
	// ------------------------------------------
	GameObject* skyGameObject = new GameObject("sky", ObjectsType::ENVIRONMENT, XMMatrixIdentity(), XMMatrixIdentity());
	skyGameObject->SetCBIndex(objCBIndex);
	skyGameObject->SetMesh(mMeshes["shapeGeo"]);
	skyGameObject->SetMaterial(mMaterials["sky"].get());
	skyGameObject->AddSubmesh(skyGameObject->GetMesh()->GetSubmesh("sphere"));

	mRenderLayer[(int)RenderLayer::Sky].push_back(skyGameObject);
	mGameObjectLayer[(int)GameObjectLayer::Sky].push_back(skyGameObject);
	mAllGameObjects.push_back(skyGameObject);

	// ------------------------------------------
	// terrain
	// ------------------------------------------
	GameObject* terrainGameObject = new GameObject("terrain", ObjectsType::ENVIRONMENT, XMMatrixIdentity(), XMMatrixIdentity());
	terrainGameObject->SetCBIndex(objCBIndex);
	terrainGameObject->SetMesh(mMeshes["terrain"]);
	terrainGameObject->SetMaterial(mMaterials["terrainMat"].get());
	terrainGameObject->AddSubmesh(terrainGameObject->GetMesh()->GetSubmesh("terrain"));

	mRenderLayer[(int)RenderLayer::Opaque].push_back(terrainGameObject);
	mGameObjectLayer[(int)GameObjectLayer::Environment].push_back(terrainGameObject);
	mAllGameObjects.push_back(terrainGameObject);

	// ------------------------------------------
	// Opaque objects
	// ------------------------------------------

	//1----------------------------------------------

	BuildCrystal(-8900, -9350, 180);
	BuildCrystal(-8700, -9500, 180);
	BuildCrystal(-8500, -9500, 180);
	BuildCrystal(-8300, -9350, 180);

	BuildCrystal(-9500, -8580, 180);
	BuildCrystal(-9630, -8780, 180);
	BuildCrystal(-9630, -8980, 180);
	BuildCrystal(-9500, -9180, 180);

	//2----------------------------------------------

	BuildCrystal(-3920, -8080, 180);
	BuildCrystal(-4060, -8280, 180);
	BuildCrystal(-4260, -8280, 0);
	BuildCrystal(-4060, -8430, 0);

	BuildCrystal(-4080, -8950, 180);
	BuildCrystal(-4250, -9120, 180);
	BuildCrystal(-4080, -9090, 0);

	BuildCrystal(-4060, -9580, 180);
	BuildCrystal(-3920, -9730, 180);

	//3----------------------------------------------

	BuildCrystal(1750, -9530, 180);
	BuildCrystal(1980, -9680, 180);
	BuildCrystal(2180, -9530, 180);
	BuildCrystal(2380, -9700, 180);
	BuildCrystal(2600, -9700, 180);
	BuildCrystal(2780, -9530, 180);
	BuildCrystal(2900, -9330, 180);

	//4----------------------------------------------

	BuildCrystal(8100, -9530, 180);
	BuildCrystal(8280, -9700, 180);
	BuildCrystal(8550, -9700, 180);
	BuildCrystal(8820, -9500, 180);

	BuildCrystal(9450, -9000, 0);
	BuildCrystal(9700, -9000, 180);
	BuildCrystal(9650, -8550, 0);
	BuildCrystal(9550, -8600, 180);

	//5----------------------------------------------

	BuildCrystal(-5000, -4800, 180);
	BuildCrystal(-5170, -5100, 180);
	BuildCrystal(-5080, -5100, 0);

	BuildCrystal(-4500, -5550, 180);
	BuildCrystal(-4450, -5580, 0);
	BuildCrystal(-3900, -5600, 180);

	//6----------------------------------------------

	BuildCrystal(-9500, -3250, 180);
	BuildCrystal(-9700, -3400, 180);
	BuildCrystal(-9600, -3410, 0);
	BuildCrystal(-9750, -3600, 0);

	BuildCrystal(-9600, -4050, 180);
	BuildCrystal(-9520, -4200, 180);
	BuildCrystal(-9500, -4200, 0);
	BuildCrystal(-9400, -4350, 0);
	BuildCrystal(-9150, -4350, 0);

	//7----------------------------------------------

	BuildCrystal(-9450, 200, 180);
	BuildCrystal(-9650, 350, 180);
	BuildCrystal(-9450, 500, 180);
	BuildCrystal(-9450, 650, 180);
	BuildCrystal(-9650, 850, 180);
	BuildCrystal(-9450, 1000, 180);
	BuildCrystal(-9650, 1200, 180);

	//8----------------------------------------------

	BuildCrystal(9550, -250, 180);
	BuildCrystal(9650, -250, 0);
	BuildCrystal(9550, -600, 180);
	BuildCrystal(9600, -600, 0);
	BuildCrystal(9550, -950, 180);
	BuildCrystal(9600, -950, 0);
	BuildCrystal(9600, -1150, 0);

	//9----------------------------------------------

	BuildCrystal(9400, 4200, 180);
	BuildCrystal(9600, 4000, 180);
	BuildCrystal(9650, 4000, 0);
	BuildCrystal(9450, 3700, 180);

	BuildCrystal(9550, 3500, 180);
	BuildCrystal(9600, 3450, 0);

	BuildCrystal(9700, 3000, 180);
	BuildCrystal(9600, 2800, 180);
	BuildCrystal(9450, 2650, 180);

	BuildCrystal(4800, 4600, 0);
	BuildCrystal(4700, 4600, 180);
	BuildCrystal(4800, 5050, 0);

	BuildCrystal(4300, 5350, 0);
	BuildCrystal(4000, 5350, 0);
	BuildCrystal(3850, 5350, 180);

	//10----------------------------------------------

	BuildCrystal(-9550, 8750, 0);
	BuildCrystal(-9700, 8900, 0);

	BuildCrystal(-9700, 9100, 180);
	BuildCrystal(-9550, 9250, 180);

	BuildCrystal(-8800, 9700, 0);
	BuildCrystal(-8550, 9850, 0);
	BuildCrystal(-8300, 9850, 0);
	BuildCrystal(-8050, 9700, 0);

	//11----------------------------------------------

	BuildCrystal(-3200, 9650, 0);
	BuildCrystal(-3050, 9850, 0);
	BuildCrystal(-2750, 9850, 0);
	BuildCrystal(-2550, 9650, 0);
	BuildCrystal(-2450, 9850, 0);
	BuildCrystal(-2200, 9850, 0);
	BuildCrystal(-2000, 9650, 0);

	//12----------------------------------------------

	BuildCrystal(4100, 9500, 180);
	BuildCrystal(4250, 9300, 180);
	BuildCrystal(4350, 9300, 0);

	BuildCrystal(4350, 9000, 0);
	BuildCrystal(4200, 8800, 0);
	BuildCrystal(4350, 8600, 0);
	BuildCrystal(4200, 8400, 0);

	BuildCrystal(4250, 7900, 180);
	BuildCrystal(4100, 7700, 180);

	//13----------------------------------------------

	BuildCrystal(7900, 9600, 0);
	BuildCrystal(8100, 9800, 0);
	BuildCrystal(8350, 9800, 0);
	BuildCrystal(8500, 9600, 0);

	BuildCrystal(9350, 9400, 0);
	BuildCrystal(9500, 9250, 0);

	BuildCrystal(9600, 8750, 180);
	BuildCrystal(9400, 8600, 180);

	//----------------------------------------------
	Weapon* swordGameObject = new Weapon("sword", ObjectsType::WEAPON, XMMatrixIdentity(), XMMatrixIdentity());
	swordGameObject->SetCBIndex(objCBIndex);
	swordGameObject->SetMesh(mMeshes["Sword"]);
	swordGameObject->SetMaterial(mMaterials["sword"].get());
	swordGameObject->AddSubmesh(swordGameObject->GetMesh()->GetSubmesh("sword"));
	swordGameObject->SetBoundingBox(XMFLOAT3(0.0f, 0.0f, 60.0f), XMFLOAT3(1.0f, 8.0f, 65.0f));
	swordGameObject->CreateBoundingBox(md3dDevice.Get(), mCommandList.Get());

	mRenderLayer[(int)RenderLayer::Opaque].push_back(swordGameObject);
	mGameObjectLayer[(int)GameObjectLayer::Environment].push_back(swordGameObject);
	mAllGameObjects.push_back(swordGameObject);

	Weapon* bowGameObject = new Weapon("bow", ObjectsType::WEAPON, XMMatrixIdentity(), XMMatrixIdentity());
	bowGameObject->SetCBIndex(objCBIndex);
	bowGameObject->SetMesh(mMeshes["Bow"]);
	bowGameObject->SetMaterial(mMaterials["bow"].get());
	bowGameObject->AddSubmesh(bowGameObject->GetMesh()->GetSubmesh("bow"));
	bowGameObject->SetBoundingBox(XMFLOAT3(0.0f, 0.0f, 60.0f), XMFLOAT3(1.0f, 8.0f, 65.0f));
	bowGameObject->CreateBoundingBox(md3dDevice.Get(), mCommandList.Get());

	mRenderLayer[(int)RenderLayer::Opaque].push_back(bowGameObject);
	mGameObjectLayer[(int)GameObjectLayer::Environment].push_back(bowGameObject);
	mAllGameObjects.push_back(bowGameObject);

	// ------------------------------------------
	// Skinned objects - player
	// ------------------------------------------
	Player* Skinned1 = new Player("skinned1", ObjectsType::CHARACTER, XMMatrixTranslation(1000.0f, 0.0f, 0.0f), XMMatrixIdentity());
	Skinned1->SetCBIndex(2, objCBIndex, skinnedCBIndex);
	Skinned1->SetMesh(mMeshes["Vanguard"]);
	Skinned1->SetMaterials(2, { mMaterials["vanguard"].get(),  mMaterials["vanguard"].get() });
	Skinned1->AddSubmesh(Skinned1->GetMesh()->mSubmeshes[0]);
	Skinned1->AddSubmesh(Skinned1->GetMesh()->mSubmeshes[1]);
	Skinned1->SetBoundingBox(XMFLOAT3(0.0f, 85.0f, 0.0f), XMFLOAT3(40.0f, 85.0f, 40.0f));
	Skinned1->CreateCylinderBoundingBox(md3dDevice.Get(), mCommandList.Get(), 16);

	mRenderLayer[(int)RenderLayer::SkinnedOpaque].push_back(Skinned1);
	mGameObjectLayer[(int)GameObjectLayer::Object].push_back(Skinned1);
	mAllGameObjects.push_back(Skinned1);
	mTeamObjects.push_back(Skinned1);

	Player* Knight = new Player("skinned", ObjectsType::CHARACTER, XMMatrixTranslation(1000.0f, 0.0f, 200.0f), XMMatrixIdentity());
	Knight->SetMesh(mMeshes["Vanguard"]);
	Knight->SetCBIndex(2, objCBIndex, skinnedCBIndex);
	Knight->SetMaterials(2, { mMaterials["vanguard"].get(),  mMaterials["vanguard"].get() });
	Knight->AddSubmesh(Knight->GetMesh()->mSubmeshes[0]);
	Knight->AddSubmesh(Knight->GetMesh()->mSubmeshes[1]);
	Knight->SetBoundingBox(XMFLOAT3(0.0f, 85.0f, 0.0f), XMFLOAT3(40.0f, 85.0f, 40.0f));
	Knight->CreateCylinderBoundingBox(md3dDevice.Get(), mCommandList.Get(), 16);

	mRenderLayer[(int)RenderLayer::SkinnedOpaque].push_back(Knight);
	mGameObjectLayer[(int)GameObjectLayer::Object].push_back(Knight);
	mAllGameObjects.push_back(Knight);
	mTeamObjects.push_back(Knight);

	// ------------------------------------------
	// Buttobn
	// -----------------------------------------

	//Button* test1 = new LobbyButton({ mClientWidth/2 , mClientHeight/2 }, { 100,100 });
	//mButtons.push_back(test1);

	mPlayer = Skinned1;
	//if (mMainCamera) {
	//	delete mMainCamera;
	//	mMainCamera = nullptr;
	//}

	Camera* m = new Camera();
	//m->SetPlayerDirections(mPlayer);
	//mMainCamera = m;
	//m->SetPosition(1100.f, mTerrain.GetHeight(1100.f, 0.f)+1000, 0.f);

	m->SetPosition(9500, mTerrain.GetHeight(9500, 9000) + 2000, 9000);
	m->LookAt(m->GetPosition3f(), mPlayer->GetPosition(), mPlayer->GetUp());
	mSubCamera.push_back(m);

	if (mFPSmode) mMainCamera = mPlayer->GetCamera();
	else mMainCamera = m;

	mMainCamera->SetLens(0.25f * MathHelper::Pi, AspectRatio(), 10.f, 30000.f);

	Skinned1->SetWeapon(swordGameObject);
	swordGameObject->SetOwner(Skinned1);

	Knight->SetWeapon(bowGameObject);
	bowGameObject->SetOwner(Knight);

}

void DummyApp::BuildUICursor()
{
	struct UIVertex
	{
		XMFLOAT2 Pos;
		XMFLOAT2 Tex;
	};

	// 2. 로컬 좌표(-0.5 ~ +0.5) 기준 사각형 1개
	UIVertex vertices[4] =
	{
		//   Pos                    Tex
		{ XMFLOAT2(-0.5f, -0.5f),  XMFLOAT2(0.0f, 1.0f) }, // 0: 좌하
		{ XMFLOAT2(-0.5f,  0.5f),  XMFLOAT2(0.0f, 0.0f) }, // 1: 좌상
		{ XMFLOAT2(0.5f,  0.5f),  XMFLOAT2(1.0f, 0.0f) }, // 2: 우상
		{ XMFLOAT2(0.5f, -0.5f),  XMFLOAT2(1.0f, 1.0f) }, // 3: 우하
	};

	// 3. 삼각형 2개 인덱스
	std::uint16_t indices[6] =
	{
		0, 1, 2,    // 첫 번째 삼각형
		0, 2, 3     // 두 번째 삼각형
	};

	const UINT vbByteSize = sizeof(vertices);
	const UINT ibByteSize = sizeof(indices);

	// 4. GPU용 정점/인덱스 버퍼 생성 (CreateDefaultBuffer 패턴 그대로)
	mCursorVB = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		vertices,
		vbByteSize,
		mCursorVBUpload);

	mCursorIB = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		indices,
		ibByteSize,
		mCursorIBUpload);

	// 5. 나중에 IA에 세팅할 뷰 정보
	mCursorVBView.BufferLocation = mCursorVB->GetGPUVirtualAddress();
	mCursorVBView.StrideInBytes = sizeof(UIVertex);
	mCursorVBView.SizeInBytes = vbByteSize;

	mCursorIBView.BufferLocation = mCursorIB->GetGPUVirtualAddress();
	mCursorIBView.Format = DXGI_FORMAT_R16_UINT;
	mCursorIBView.SizeInBytes = ibByteSize;
}

void DummyApp::BuildCrystal(const float& x, const float& y, const float& degree)
{
	GameObject* crystalGameObject = new GameObject("crystal", ObjectsType::ENVIRONMENT, XMMatrixScaling(10.0f, 10.0f, 10.0f) * XMMatrixTranslation(x, mTerrain.GetHeight(x, y), y), XMMatrixIdentity());
	crystalGameObject->Rotate(0.f, degree * XM_PI / 180, 0.f);
	crystalGameObject->SetCBIndex(objCBIndex);
	crystalGameObject->SetMesh(mMeshes["Crystal"]);
	crystalGameObject->SetMaterial(mMaterials["crystal"].get());
	crystalGameObject->AddSubmesh(crystalGameObject->GetMesh()->GetSubmesh("crystal"));
	crystalGameObject->SetBoundingBox(XMFLOAT3(0.0f, 10.f, -10.0f), XMFLOAT3(10.f, 10.f, 10.f));
	crystalGameObject->CreateBoundingBox(md3dDevice.Get(), mCommandList.Get());

	mRenderLayer[(int)RenderLayer::Opaque].push_back(crystalGameObject);
	mGameObjectLayer[(int)GameObjectLayer::Object].push_back(crystalGameObject);
	mAllGameObjects.push_back(crystalGameObject);
}


// ============================================================
// [2] BuildStaticColliders() - 정적 충돌체 수집
// ============================================================
// Initialize()에서 BuildGameObjects() 뒤에 호출

void DummyApp::BuildStaticColliders()
{
	mStaticColliders.clear();

	for (auto& obj : mGameObjectLayer[(int)GameObjectLayer::Object])
	{
		// 캐릭터는 동적이므로 제외
		if (obj->GetObjType() == ObjectsType::CHARACTER)
			continue;

		if (obj->GetName() == "terrain" || obj->GetName() == "sky")
			continue;

		// 월드 변환 적용된 BB
		BoundingBox worldBB;
		obj->GetBoundingBox().Transform(worldBB, XMLoadFloat4x4(&obj->GetWorld()));
		mStaticColliders.push_back(worldBB);
	}

	// Environment 레이어에서도 충돌 대상 수집 (크리스탈 등)
	// 무기(WEAPON)는 캐릭터에 붙어있으므로 제외
	for (auto& obj : mGameObjectLayer[(int)GameObjectLayer::Environment])
	{
		if (obj->GetObjType() == ObjectsType::WEAPON)
			continue;

		if (obj->GetName() == "terrain" || obj->GetName() == "sky")
			continue;

		BoundingBox worldBB;
		obj->GetBoundingBox().Transform(worldBB, XMLoadFloat4x4(&obj->GetWorld()));
		mStaticColliders.push_back(worldBB);
	}

	// 필요하면 맵 경계 벽도 수동 추가
	/*
	BoundingBox leftWall;
	leftWall.Center = XMFLOAT3(-10010.0f, 500.0f, 0.0f);
	leftWall.Extents = XMFLOAT3(10.0f, 500.0f, 10000.0f);
	mStaticColliders.push_back(leftWall);
	// 나머지 벽도 동일...
	*/

#ifdef _DEBUG
	std::cout << "[Collision] Static colliders: " << mStaticColliders.size() << std::endl;
#endif
}

void DummyApp::BuildDynamicColliders()
{
	mDynamicColliders.clear();

	for (auto& obj : mAllGameObjects)
	{
		// 캐릭터 = 동적 충돌 대상
		if (obj->GetObjType() == ObjectsType::CHARACTER)
		{
			mDynamicColliders.push_back(obj);
			continue;
		}

		// 여기에 이동하는 장애물 등 추가 조건을 넣을 수 있음
		// 예: if (obj->GetObjType() == ObjectsType::DYNAMIC_OBSTACLE)
		//         mDynamicColliders.push_back(obj);
	}
}

void DummyApp::ResolveAllCollisions()
{
	// 동적 충돌 대상 목록 갱신
	BuildDynamicColliders();

	// ----------------------------------------------------------
	// Phase 1: 동적 오브젝트 vs 정적 충돌체
	// ----------------------------------------------------------
	for (auto& obj : mDynamicColliders)
	{
		XMFLOAT3 currentPos = obj->GetPosition();

		XMFLOAT3 resolvedPos = PhysicsHelper::ResolveStaticCollision(
			obj->GetBoundingBox(),
			currentPos,
			mStaticColliders
		);

		float diffX = std::abs(resolvedPos.x - currentPos.x);
		float diffZ = std::abs(resolvedPos.z - currentPos.z);

		if (diffX > 0.001f || diffZ > 0.001f)
		{
			obj->SetPosition(resolvedPos.x, currentPos.y, resolvedPos.z);

			if (obj->GetObjType() == ObjectsType::CHARACTER)
			{
				Player* player = dynamic_cast<Player*>(obj);
				if (player)
				{
					XMFLOAT3 vel = player->GetVelocity();
					PhysicsHelper::AdjustVelocityAfterCollision(vel, currentPos, resolvedPos);
					player->SetVelocity(vel);
				}
			}

			obj->SetFrameDirty();
		}
	}

	// ----------------------------------------------------------
	// Phase 2: 동적 오브젝트 vs 동적 오브젝트
	// ----------------------------------------------------------
	for (size_t i = 0; i < mDynamicColliders.size(); ++i)
	{
		for (size_t j = i + 1; j < mDynamicColliders.size(); ++j)
		{
			GameObject* objA = mDynamicColliders[i];
			GameObject* objB = mDynamicColliders[j];

			XMFLOAT3 posA = objA->GetPosition();
			XMFLOAT3 posB = objB->GetPosition();

			// 거리 컬링
			float dx = posA.x - posB.x;
			float dz = posA.z - posB.z;
			float distSq = dx * dx + dz * dz;

			float maxRangeA = max(objA->GetBoundingBox().Extents.x, objA->GetBoundingBox().Extents.z);
			float maxRangeB = max(objB->GetBoundingBox().Extents.x, objB->GetBoundingBox().Extents.z);
			float cullDist = (maxRangeA + maxRangeB) * 2.0f;

			if (distSq > cullDist * cullDist)
				continue;

			XMFLOAT3 newPosA = posA;
			XMFLOAT3 newPosB = posB;

			// 이동 중인지 판별 (Walk/Run 상태이면 이동 중)
			bool movingA = false;
			bool movingB = false;

			if (auto pA = dynamic_cast<Player*>(objA))
			{
				XMFLOAT3 v = pA->GetVelocity();
				movingA = (fabsf(v.x) + fabsf(v.z)) > 0.01f;
			}
			if (auto pB = dynamic_cast<Player*>(objB))
			{
				XMFLOAT3 v = pB->GetVelocity();
				movingB = (fabsf(v.x) + fabsf(v.z)) > 0.01f;
			}

			bool collided = PhysicsHelper::ResolveDynamicCollision(
				newPosA, newPosB,
				objA->GetBoundingBox(),
				objB->GetBoundingBox(),
				movingA,
				movingB
			);

			if (collided)
			{
				objA->SetPosition(newPosA.x, posA.y, newPosA.z);
				if (objA->GetObjType() == ObjectsType::CHARACTER)
				{
					Player* playerA = dynamic_cast<Player*>(objA);
					if (playerA)
					{
						XMFLOAT3 velA = playerA->GetVelocity();
						PhysicsHelper::AdjustVelocityAfterCollision(velA, posA, newPosA);
						playerA->SetVelocity(velA);
					}
				}
				objA->SetFrameDirty();

				objB->SetPosition(newPosB.x, posB.y, newPosB.z);
				if (objB->GetObjType() == ObjectsType::CHARACTER)
				{
					Player* playerB = dynamic_cast<Player*>(objB);
					if (playerB)
					{
						XMFLOAT3 velB = playerB->GetVelocity();
						PhysicsHelper::AdjustVelocityAfterCollision(velB, posB, newPosB);
						playerB->SetVelocity(velB);
					}
				}
				objB->SetFrameDirty();
			}
		}
	}

	// ----------------------------------------------------------
	// Phase 3: 동적 충돌로 밀린 후 정적 충돌체에 끼는 경우 재검사
	// ----------------------------------------------------------
	for (auto& obj : mDynamicColliders)
	{
		XMFLOAT3 currentPos = obj->GetPosition();

		XMFLOAT3 resolvedPos = PhysicsHelper::ResolveStaticCollision(
			obj->GetBoundingBox(),
			currentPos,
			mStaticColliders
		);

		float diffX = std::abs(resolvedPos.x - currentPos.x);
		float diffZ = std::abs(resolvedPos.z - currentPos.z);

		if (diffX > 0.001f || diffZ > 0.001f)
		{
			obj->SetPosition(resolvedPos.x, currentPos.y, resolvedPos.z);
			obj->SetFrameDirty();
		}
	}

	// ----------------------------------------------------------
	// Phase 4: 충돌로 막힌 캐릭터 → A* 재탐색 or 멈춤
	// ----------------------------------------------------------
	// 주의: A* 재탐색은 비용이 크므로 쿨타임 적용 필요
	// Player.h에 다음 멤버 추가 필요:
	//   float mPathRetryTimer = 0.0f;
	//   static constexpr float PATH_RETRY_COOLDOWN = 0.5f; // 0.5초마다 재탐색

	for (auto& obj : mDynamicColliders)
	{
		if (obj->GetObjType() != ObjectsType::CHARACTER)
			continue;

		Player* player = dynamic_cast<Player*>(obj);
		if (!player)
			continue;

		// 이동 중이 아니면 스킵
		StateId lowerState = player->GetLowerStateId();
		if (lowerState != StateId::Walk && lowerState != StateId::Run)
			continue;

		XMFLOAT3 pos = player->GetPosition();
		XMFLOAT3 dest = player->GetDestination(); // 현재 waypoint 또는 최종 목적지

		float toDest = std::sqrt(
			(dest.x - pos.x) * (dest.x - pos.x) +
			(dest.z - pos.z) * (dest.z - pos.z)
		);

		if (toDest < 1.0f)
			continue;

		// "내 앞이 다른 동적 오브젝트에 막혀있는가?" 체크
		XMFLOAT3 dirToDest = {
			(dest.x - pos.x) / toDest,
			0.0f,
			(dest.z - pos.z) / toDest
		};

		float stepSize = max(player->GetBoundingBox().Extents.x,
			player->GetBoundingBox().Extents.z) * 0.5f;
		XMFLOAT3 testPos = {
			pos.x + dirToDest.x * stepSize,
			pos.y,
			pos.z + dirToDest.z * stepSize
		};

		BoundingBox testBB = PhysicsHelper::MakeWorldBB(player->GetBoundingBox(), testPos);

		bool blockedByDynamic = false;
		bool blockedByStatic = false;

		// 정적 충돌체에 막히는지
		for (const auto& collider : mStaticColliders)
		{
			if (testBB.Intersects(collider))
			{
				blockedByStatic = true;
				break;
			}
		}

		// 다른 동적 오브젝트(멈춘 것)에 막히는지
		if (!blockedByStatic)
		{
			for (auto& other : mDynamicColliders)
			{
				if (other == obj) continue;

				// 이동 중인 상대는 스킵 (서로 밀치는 건 Phase 2에서 처리)
				if (other->GetObjType() == ObjectsType::CHARACTER)
				{
					Player* pOther = dynamic_cast<Player*>(other);
					if (pOther)
					{
						StateId otherState = pOther->GetLowerStateId();
						if (otherState == StateId::Walk || otherState == StateId::Run)
							continue;
					}
				}

				BoundingBox otherBB = PhysicsHelper::MakeWorldBB(
					other->GetBoundingBox(), other->GetPosition());

				if (testBB.Intersects(otherBB))
				{
					blockedByDynamic = true;
					break;
				}
			}
		}

		bool blocked = blockedByStatic || blockedByDynamic;

		if (blocked)
		{

			if (!player->CanRetryPath())
				continue;
			player->ResetPathRetryTimer();
			// 재탐색 쿨타임 체크 (매 프레임 재탐색 방지)
			// Player.h에 mPathRetryTimer 추가 필요
			// player->mPathRetryTimer가 0 이하일 때만 재탐색
			// if (player->mPathRetryTimer > 0.f) continue;
			// player->mPathRetryTimer = Player::PATH_RETRY_COOLDOWN;

			// 최종 목적지 (마지막 waypoint 또는 path가 없으면 dest 자체)
			XMFLOAT3 finalDest = dest;
			if (player->HasPath())
			{
				// path의 마지막 waypoint가 진짜 최종 목적지
				// GetDestination()은 현재 waypoint이므로, 
				// 최종 목적지는 따로 저장해둬야 하지만
				// 없으면 현재 dest로 재탐색
				XMFLOAT3 finalDest = player->GetFinalDestination();

				// finalDest가 현재 위치와 동일하면 (초기값 or 미설정) dest로 fallback
				float distToFinal = std::sqrt(
					(finalDest.x - pos.x) * (finalDest.x - pos.x) +
					(finalDest.z - pos.z) * (finalDest.z - pos.z));
				if (distToFinal < 1.0f)
					finalDest = dest;
			}

			// A* 재탐색
			mPathfinder.ClearDynamicObstacles();
			for (auto& other : mDynamicColliders)
			{
				if (other == obj) continue;

				bool isStationary = true;
				if (other->GetObjType() == ObjectsType::CHARACTER)
				{
					Player* pOther = dynamic_cast<Player*>(other);
					if (pOther)
					{
						StateId st = pOther->GetLowerStateId();
						if (st == StateId::Walk || st == StateId::Run)
							isStationary = false;
					}
				}

				if (isStationary)
				{
					XMFLOAT3 otherPos = other->GetPosition();
					int gx, gz;
					mPathfinder.WorldToGrid(otherPos.x, otherPos.z, gx, gz);
					mPathfinder.SetDynamicObstacle(gx, gz);

					float maxExt = max(other->GetBoundingBox().Extents.x,
						other->GetBoundingBox().Extents.z);
					int expand = (int)(maxExt / mPathfinder.GetCellSize());
					for (int dz = -expand; dz <= expand; ++dz)
						for (int dx = -expand; dx <= expand; ++dx)
							if (dx != 0 || dz != 0)
								mPathfinder.SetDynamicObstacle(gx + dx, gz + dz);
				}
			}

			std::vector<XMFLOAT3> newPath;
			bool found = mPathfinder.FindPath(
				pos, finalDest,
				XMFLOAT3(player->GetBoundingBox().Extents.x, 0.f,
					player->GetBoundingBox().Extents.z),
				newPath);

			mPathfinder.ClearDynamicObstacles();

			if (found && newPath.size() >= 2)
			{
				for (auto& wp : newPath)
					wp.y = mTerrain.GetHeight(wp.x, wp.z);
				player->SetPath(newPath);
			}
			else
			{
				// 정말 갈 수 없으면 멈춤
				player->SetDestination(pos);
				player->SetVelocity(XMFLOAT3(0.f, player->GetVelocity().y, 0.f));
				player->ClearPath();
				player->ChangeLowerState(new IdlePlayerState());
			}
		}
	}
}
void DummyApp::InitPathfinder()
{
	float mapWidth = mTerrain.GetWidth();
	float mapLength = mTerrain.GetLength();

	// Fog와 동일한 cellSize 사용 (또는 유닛 크기에 맞게 조정)
	// Fog: cellSize = 24.f → pathfinding은 좀 더 세밀하게 하고 싶으면 줄일 수 있음
	float pathCellSize = 24.0f;

	mPathfinder.Initialize(mapWidth, mapLength, pathCellSize);
	mPathfinder.BakeStaticObstacles(mStaticColliders);

#ifdef _DEBUG
	int total = mPathfinder.GetGridX() * mPathfinder.GetGridZ();
	int blocked = 0;
	for (int z = 0; z < mPathfinder.GetGridZ(); ++z)
		for (int x = 0; x < mPathfinder.GetGridX(); ++x)
			if (!mPathfinder.IsWalkable(x, z)) ++blocked;
	std::cout << "[Pathfinder] Grid: " << mPathfinder.GetGridX() << "x" << mPathfinder.GetGridZ()
		<< " | Blocked: " << blocked << "/" << total << std::endl;
#endif
}

void DummyApp::DrawGameObjects(ID3D12GraphicsCommandList* cmdList, const std::vector<GameObject*>& gameObjects)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT skinnedCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto skinnedCB = mCurrFrameResource->SkinnedCB->Resource();

	// 각 렌더항목에 대해:
	for (UINT i = 0; i < gameObjects.size(); ++i)
	{
		auto gameObj = gameObjects[i];

		if(!ShouldRenderObject(gameObj)) continue;

		cmdList->IASetVertexBuffers(0, 1, &gameObj->GetMesh()->VertexBufferView());
		cmdList->IASetIndexBuffer(&gameObj->GetMesh()->IndexBufferView());
		cmdList->IASetPrimitiveTopology(gameObj->GetPrimitiveType());

		if (gameObj->GetSkinnedCBIndex() != -1) {
			D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + gameObj->GetSkinnedCBIndex() * skinnedCBByteSize;
			//std::cout<< gameObj->GetName() << ", skinnedCBAddress : " << skinnedCBAddress << std::endl;

			cmdList->SetGraphicsRootConstantBufferView(1, skinnedCBAddress);
		}
		else {
			cmdList->SetGraphicsRootConstantBufferView(1, 0);
		}

		for (UINT j = 0; j < gameObj->GetNumSubmeshes(); j++)
		{
			// 현재 프레임 자원에 대한 이 물체를 위한 CBV의 오프셋을 구한다.
			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + gameObj->GetObjCBIndex(j) * objCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

			cmdList->DrawIndexedInstanced(gameObj->GetNumIndices(j), 1, gameObj->GetBaseIndex(j), gameObj->GetBaseVertex(j), 0);
		}
	}
}

void DummyApp::DrawBoundingBox(ID3D12GraphicsCommandList* cmdList, const std::vector<GameObject*>& gameObjects)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();

	for (UINT i = 0; i < gameObjects.size(); ++i)
	{
		if (gameObjects[i]->GetName() == "terrain") continue;

		cmdList->IASetVertexBuffers(0, 1, &gameObjects[i]->BoundingBoxVertexBufferView());
		cmdList->IASetIndexBuffer(&gameObjects[i]->BoundingBoxIndexBufferView());
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

		cmdList->SetGraphicsRootConstantBufferView(1, 0);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + gameObjects[i]->GetObjCBIndex(0) * objCBByteSize;
		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		// ★ 변경: 고정 24 대신 실제 인덱스 수 사용
		cmdList->DrawIndexedInstanced(gameObjects[i]->GetBoundIndexCount(), 1, 0, 0, 0);
	}
}

void DummyApp::DrawButtons(ID3D12GraphicsCommandList* cmdList)
{
	//cmdList->SetPipelineState(mPSOs["ui"].Get());

	for (auto v : mButtons) {

		if (!v->isActive()) continue;

		cmdList->DrawInstanced(6, 1, 0, 0);

	}
}

void DummyApp::SummonKnight()
{
	if (!mNetworkBridge || mMyPlayerNumber == 0) {
		std::cout << "[Produce] not linked to server yet\n";
		mUIkey.isO = false; mUIkey.isK = false;
		return;
	}

	XMFLOAT3 pos;

	if(!PickTerrainPoint(mLastMousePos.x, mLastMousePos.y, pos)) {
		std::cout << "[Produce] invalid terrain point\n";
		mUIkey.isO = false; mUIkey.isK = false;
		return;
	}

	SendUnitProduceRequest(ObjType::Knight, pos);
	
	mUIkey.isO= false; mUIkey.isK = false;
	
}

void DummyApp::SummonHunter()
{
	if (!mNetworkBridge || mMyPlayerNumber == 0) {
		std::cout << "[Produce] not linked to server yet\n";
		mUIkey.isO = false; mUIkey.isN = false;   // 헌터 키 조합에 맞게
		return;
	}

	XMFLOAT3 pos;

	if(!PickTerrainPoint(mLastMousePos.x, mLastMousePos.y, pos)) {
		std::cout << "[Produce] invalid terrain point\n";
		mUIkey.isO = false; mUIkey.isN = false;
		return;
	}

	SendUnitProduceRequest(ObjType::Hunter, pos);

	mUIkey.isO = false; mUIkey.isN = false;

}

void DummyApp::SummonSlave()
{
}

void DummyApp::DoUpgrade()
{
}

int DummyApp::GetNetworkObjNumber(GameObject* obj) const
{
	for (const auto& [key, v] : mNetworkObjects) {
		if (v == obj && (key / 256) == mMyPlayerNumber)
			return key % 256;
	}
	return -1;
}

void DummyApp::OnPlayerLeft(const SCPlayerLeft* p)
{
	std::cout << "[Net] player " << (int)p->playerNumber << " left the game\n";

	// 나간 플레이어의 유닛 매핑 해제 (오브젝트 자체는 남겨두고 정지)
	for (auto it = mNetworkObjects.begin(); it != mNetworkObjects.end(); )
	{
		if (it->first / 256 == p->playerNumber) {
			if (Player* unit = dynamic_cast<Player*>(it->second)) {
				unit->ClearPath();
				unit->SetDestination(unit->GetPosition());  // 제자리 정지
			}
			it = mNetworkObjects.erase(it);
		}
		else ++it;
	}
}

void DummyApp::SendAttackRequest(unsigned char attackerObj, unsigned char targetOwner, unsigned char targetObj)
{
	if (!mNetworkBridge || mMyPlayerNumber == 0) return;

	CSAttackRequest req;
	req.playerNumber = (unsigned char)mMyPlayerNumber;
	req.attackerObj = attackerObj;
	req.targetOwner = targetOwner;
	req.targetObj = targetObj;

	mNetworkBridge->EnqueueSend(req);
}

void DummyApp::OnAttackResult(const SCAttackResult* p)
{
	// 공격자 애니메이션
	if (GameObject* atk = FindNetworkObject(p->attackerOwner, p->attackerObj)) {
		if (Player* unit = dynamic_cast<Player*>(atk)) {
			// 대상 바라보기 + 공격 모션
			if (GameObject* tgt = FindNetworkObject(p->targetOwner, p->targetObj)) {
				XMFLOAT3 tp = tgt->GetPosition();
				unit->SetDestination(unit->GetPosition());   // 제자리
				// TODO: unit->LookAt(tp) 같은 회전 함수가 있으면 호출
			}
			unit->SetFollowerKeyInput(FollowerKeyInput::Attack);  // ※ enum 이름 확인
			unit->FollowerEvent();
		}
	}

	// 대상 HP 반영
	std::cout << "[Combat] obj " << (int)p->targetObj
		<< " (owner " << (int)p->targetOwner << ") hp -> "
		<< p->targetHpRemaining << "\n";
	// TODO: HP바 UI. GameObject에 hp 멤버가 없으면 추가하거나
	//       DummyApp에 unordered_map<int,int> mUnitHp 로 관리.
}

void DummyApp::OnObjDead(const SCObjDead* p)
{
	std::cout << "[Combat] obj " << (int)p->objNumber
		<< " (owner " << (int)p->ownerPlayer << ") DIED\n";

	auto it = mNetworkObjects.find(NetKey(p->ownerPlayer, p->objNumber));
	if (it == mNetworkObjects.end()) return;

	if (Player* unit = dynamic_cast<Player*>(it->second)) {
		unit->ClearPath();
		unit->SetDestination(unit->GetPosition());
		// TODO: 사망 애니메이션 (StateId::Death 등이 있으면 전환)
		//       렌더 레이어에서 제거 or 눕히기. 당장은 매핑 해제로
		//       추가 명령 대상에서만 빠진다.
	}
	mNetworkObjects.erase(it);
}

GameObject* DummyApp::PickEnemyUnit(int sx, int sy, int& outOwner, unsigned char& outObjNum)
{
	XMVECTOR rayOrigin, rayDir;
	MathHelper::ScreenToRay(sx, sy, mClientWidth, mClientHeight,
		mMainCamera->GetView(), mMainCamera->GetProj(),	rayOrigin, rayDir);  

	for (const auto& [key, obj] : mNetworkObjects) {
		int owner = key / 256;
		if (owner == mMyPlayerNumber) continue;      // 적만
		if (obj->GetObjType() != ObjectsType::CHARACTER) continue; // 유닛만
		if (mDarknessEnabled && !isInTeamVision(obj)) continue;  // 시야 밖이면 스킵

		BoundingBox worldBB;
		obj->GetBoundingBox().Transform(worldBB,
			XMLoadFloat4x4(&obj->GetWorld()));

		float dist;
		if (worldBB.Intersects(rayOrigin, rayDir, dist)) {
			outOwner = owner;
			outObjNum = (unsigned char)(key % 256);
			return obj;
		}
	}
	return nullptr;
}

void DummyApp::SummonCommandCenter()
{
	if (!mNetworkBridge || mMyPlayerNumber == 0) {
		std::cout << "[Build] not linked to server yet\n";
		mUIkey.isH = false;
		mUIkey.isB = false;
		return;
	}

	XMVECTOR worldPos = MathHelper::ScreenToWorld(
		mLastMousePos.x, mLastMousePos.y,
		mClientWidth, mClientHeight,
		mMainCamera->GetView(), mMainCamera->GetProj());

	XMFLOAT3 pos;
	XMStoreFloat3(&pos, worldPos);

	CSBuildRequest req;
	req.playerNumber = (unsigned char)mMyPlayerNumber;
	req.buildType = (unsigned char)ObjType::Base;
	req.position = { pos.x, 0.0f, pos.z };

	mNetworkBridge->EnqueueSend(req);

	mUIkey.isH = false;
	mUIkey.isB = false;
}

void DummyApp::OnBuildResult(const SCBuildResult* p)
{
	if (!p->success) {
		std::cout << "[Build] rejected by server\n";
		// TODO: UI 피드백 (빨간 표시 등)
		return;
	}

	CreateCommandCenterAt(p->ownerPlayer, p->buildNumber,
		XMFLOAT3(p->position.x, 0.0f, p->position.z));
}

void DummyApp::SendUnitProduceRequest(unsigned char unitType, const XMFLOAT3& pos)
{
	CSUnitProduceRequest req;
	req.playerNumber = (unsigned char)mMyPlayerNumber;
	req.unitType = unitType;
	req.position = { pos.x, pos.y, pos.z };

	mNetworkBridge->EnqueueSend(req);
}

void DummyApp::OnUnitProduced(const SCUnitProduced* p)
{
	if (!p->success) {
		std::cout << "[Produce] rejected by server\n";
		return;
	}

	XMFLOAT3 pos(p->position.x, 0.0f, p->position.z);

	Player* unit = nullptr;
	if (p->unitType == (unsigned char)ObjType::Knight)
		unit = CreateKnightAt(p->ownerPlayer, p->objNumber, pos);
	else if (p->unitType == (unsigned char)ObjType::Hunter)
		unit = CreateHunterAt(p->ownerPlayer, p->objNumber, pos);

	if (!unit) {
		std::cout << "[Produce] local creation FAILED\n";
		return;
	}

	std::cout << "[Produce] unit #" << (int)p->objNumber
		<< " (owner " << (int)p->ownerPlayer << ") created\n";
}

void DummyApp::CreateCommandCenterAt(int ownerPlayer, unsigned char buildNumber, const XMFLOAT3& pos)
{
	float y = mTerrain.GetHeight(pos.x, pos.z);

	GameObject* cc = new GameObject("CommandCenter",
		ObjectsType::ENVIRONMENT,
		XMMatrixTranslation(pos.x, y, pos.z),
		XMMatrixIdentity());

	cc->SetCBIndex(objCBIndex);
	cc->SetMesh(mMeshes["CommandCenter"]);
	cc->SetMaterial(mMaterials["commandCenter"].get());
	cc->AddSubmesh(cc->GetMesh()->GetSubmesh("commandcenter"));
	cc->SetBoundingBox(XMFLOAT3(0.0f, CC_EXTENT_Y, 0.0f),
		XMFLOAT3(CC_EXTENT_X, CC_EXTENT_Y, CC_EXTENT_Z));
	mPendingBoundingBuilds.push_back({ cc, BoundingShape::Box, 16 });

	mRenderLayer[(int)RenderLayer::Opaque].push_back(cc);
	mGameObjectLayer[(int)GameObjectLayer::Object].push_back(cc);
	mAllGameObjects.push_back(cc);

	//if (ownerPlayer == mMyPlayerNumber)
	//	mTeamObjects.push_back(cc);   // 내 건물 → 시야(암흑) 시스템에 포함
	RegisterVisionObject(cc, ownerPlayer);

	// 클라 물리에도 즉시 반영: 월드 BB를 정적 충돌체로 등록
	BoundingBox worldBB;
	cc->GetBoundingBox().Transform(worldBB, XMLoadFloat4x4(&cc->GetWorld()));
	mStaticColliders.push_back(worldBB);

	// 네트워크 매핑 등록 (건물 번호는 유닛과 겹치지 않게 +100 오프셋)
	mNetworkObjects[NetKey(ownerPlayer, buildNumber + 100)] = cc;

	std::cout << "[Build] CommandCenter #" << (int)buildNumber
		<< " (owner " << ownerPlayer << ") at ("
		<< pos.x << ", " << pos.z << ")\n";
}

Player* DummyApp::CreateKnightAt(int ownerPlayer, unsigned char objNumber, const XMFLOAT3& pos)
{
	XMFLOAT3 spawnPos = {pos.x, mTerrain.GetHeight(pos.x,pos.z), pos.z};

	Weapon* swordGameObject = new Weapon("sword", ObjectsType::WEAPON, XMMatrixIdentity(), XMMatrixIdentity());
	swordGameObject->SetCBIndex(objCBIndex);
	swordGameObject->SetMesh(mMeshes["Sword"]);
	swordGameObject->SetMaterial(mMaterials["sword"].get());
	swordGameObject->AddSubmesh(swordGameObject->GetMesh()->GetSubmesh("sword"));
	swordGameObject->SetBoundingBox(XMFLOAT3(0.0f, 0.0f, 60.0f), XMFLOAT3(1.0f, 8.0f, 65.0f));
	mPendingBoundingBuilds.push_back({ swordGameObject, BoundingShape::Box, 16 });

	mRenderLayer[(int)RenderLayer::Opaque].push_back(swordGameObject);
	mGameObjectLayer[(int)GameObjectLayer::Environment].push_back(swordGameObject);
	mAllGameObjects.push_back(swordGameObject);



	Player* playerGameObject1 = new Player("skinned", ObjectsType::CHARACTER, XMMatrixTranslation(spawnPos.x,spawnPos.y,spawnPos.z), XMMatrixIdentity());
	playerGameObject1->SetMaxHP(GetDefaultMaxHp(ObjType::Knight));
	playerGameObject1->SetMesh(mMeshes["Vanguard"]);
	playerGameObject1->SetCBIndex(2, objCBIndex, skinnedCBIndex);
	playerGameObject1->SetMaterials(2, { mMaterials["vanguard"].get(),  mMaterials["vanguard"].get() });
	playerGameObject1->AddSubmesh(playerGameObject1->GetMesh()->mSubmeshes[0]);
	playerGameObject1->AddSubmesh(playerGameObject1->GetMesh()->mSubmeshes[1]);
	playerGameObject1->SetBoundingBox(XMFLOAT3(0.0f, 85.0f, 0.0f), XMFLOAT3(40.0f, 85.0f, 40.0f));
	mPendingBoundingBuilds.push_back({ playerGameObject1, BoundingShape::Cylinder, 16 });

	mRenderLayer[(int)RenderLayer::SkinnedOpaque].push_back(playerGameObject1);
	mGameObjectLayer[(int)GameObjectLayer::Object].push_back(playerGameObject1);
	mAllGameObjects.push_back(playerGameObject1);
	//mTeamObjects.push_back(playerGameObject1);
	RegisterVisionObject(playerGameObject1, ownerPlayer);

	playerGameObject1->SetWeapon(swordGameObject);
	swordGameObject->SetOwner(playerGameObject1);

	if (mDebugMode) std::cout << "[Debug] Summoned Knight at (" << spawnPos.x << ", " << spawnPos.z << ")" << std::endl;


	mNetworkObjects[NetKey(ownerPlayer, objNumber)] = playerGameObject1;

	return playerGameObject1;
}

Player* DummyApp::CreateHunterAt(int ownerPlayer, unsigned char objNumber, const XMFLOAT3& pos)
{
	XMFLOAT3 spawnPos = { pos.x, mTerrain.GetHeight(pos.x,pos.z), pos.z };
	
	Weapon* bowGameObject = new Weapon("bow", ObjectsType::WEAPON, XMMatrixIdentity(), XMMatrixIdentity());
	bowGameObject->SetCBIndex(objCBIndex);
	bowGameObject->SetMesh(mMeshes["Bow"]);
	bowGameObject->SetMaterial(mMaterials["bow"].get());
	bowGameObject->AddSubmesh(bowGameObject->GetMesh()->GetSubmesh("bow"));
	bowGameObject->SetBoundingBox(XMFLOAT3(0.0f, 0.0f, 60.0f), XMFLOAT3(1.0f, 8.0f, 65.0f));
	mPendingBoundingBuilds.push_back({ bowGameObject, BoundingShape::Box, 16 });

	mRenderLayer[(int)RenderLayer::Opaque].push_back(bowGameObject);
	mGameObjectLayer[(int)GameObjectLayer::Environment].push_back(bowGameObject);
	mAllGameObjects.push_back(bowGameObject);



	Player* playerGameObject2 = new Player("Hunter", ObjectsType::CHARACTER, XMMatrixTranslation(spawnPos.x,spawnPos.y,spawnPos.z), XMMatrixIdentity());
	playerGameObject2->SetMaxHP(GetDefaultMaxHp(ObjType::Hunter));
	playerGameObject2->SetCBIndex(2, objCBIndex, skinnedCBIndex);
	playerGameObject2->SetMesh(mMeshes["Hunter"]);
	playerGameObject2->SetMaterials(2, { mMaterials["hunter"].get(),  mMaterials["hunter"].get() });
	playerGameObject2->AddSubmesh(playerGameObject2->GetMesh()->mSubmeshes[0]);
	playerGameObject2->AddSubmesh(playerGameObject2->GetMesh()->mSubmeshes[1]);
	playerGameObject2->SetBoundingBox(XMFLOAT3(0.0f, 85.0f, 0.0f), XMFLOAT3(40.0f, 85.0f, 40.0f));
	mPendingBoundingBuilds.push_back({ playerGameObject2, BoundingShape::Cylinder, 16 });

	mRenderLayer[(int)RenderLayer::SkinnedOpaque].push_back(playerGameObject2);
	mGameObjectLayer[(int)GameObjectLayer::Object].push_back(playerGameObject2);

	mAllGameObjects.push_back(playerGameObject2);
	//mTeamObjects.push_back(playerGameObject2);
	RegisterVisionObject(playerGameObject2, ownerPlayer);

	playerGameObject2->SetWeapon(bowGameObject);
	bowGameObject->SetOwner(playerGameObject2);



	if (mDebugMode) std::cout << "[Debug] Summoned Hunter at (" << spawnPos.x << ", " << spawnPos.z << ")" << std::endl;


	// (4) 네트워크 매핑 등록 — 이게 핵심
	mNetworkObjects[NetKey(ownerPlayer, objNumber)] = playerGameObject2;

	return playerGameObject2;
}

void DummyApp::ReleseMemory()
{
	// clear mesh
	for (auto& pair : mMeshes) {
		Mesh* meshPtr = pair.second;
		if (meshPtr != nullptr) {
			delete meshPtr;
			pair.second = nullptr;
		}
	}
	mMeshes.clear();

	// clear gameObj
	for (auto& gameObj : mAllGameObjects)
	{
		GameObject* meshPtr = gameObj;
		if (meshPtr != nullptr) {
			delete meshPtr;
			gameObj = nullptr;
		}
	}
	mAllGameObjects.clear();

	/*if (mPlayer) {
		delete mPlayer;
		mPlayer = nullptr;
	}*/

	if (mMainCamera) {
		delete mMainCamera;
		mMainCamera = nullptr;
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> DummyApp::GetStaticSamplers()
{
	// 그래픽 응용 프로그램이 사용하는 표본추출기의 수는 그리 많지 않으므로,
	// 미리 만들어서 루트 서명에 포함시켜 둔다.

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

void DummyApp::SendLinkRequest()
{
	if (!mNetworkBridge) return;

	CGLinkInfo info{};
	info.size = sizeof(CGLinkInfo);
	info.type = CG_LINKGAMESERVER;
	strncpy_s(info.RoomCode, "DEMO", RoomCodeLen - 1);  // 데모용 하드코딩
	info.userID = 0;                                    // TODO: 로비 연동 시 실제 ID

	mNetworkBridge->EnqueueSend(info);
	std::cout << "[Net] link request queued\n";
}

void DummyApp::OnLinkResult(const SCLinkResult* p)
{
	if (p->playerNumber == 0) {
		std::cout << "[Net] link FAILED (room full / not found)\n";
		return;
	}
	mMyPlayerNumber = p->playerNumber;
	std::cout << "[Net] linked as player " << mMyPlayerNumber << "\n";
}

void DummyApp::OnGameStart(const SCGameStart* p)
{
	mTeamObjects.clear();
	mEnemyObjects.clear();

	int count = (p->unitCount < MAX_TOTAL_START_UNITS)
		? p->unitCount : MAX_TOTAL_START_UNITS;

	for (int i = 0; i < count; ++i)
		BindNetworkUnit(p->units[i]);

	mGameStarted = true;
	std::cout << "[Net] GAME START, units: " << count << "\n";
}

void DummyApp::BindNetworkUnit(const SCStartUnit& u)
{
	// 내 유닛 / 상대 유닛을 로컬 캐릭터 풀에서 순서대로 할당하는 예시.
	// mAllGameObjects에서 CHARACTER 타입을 순회하며 아직 매핑 안 된
	// 오브젝트를 하나 집어 바인딩한다.
	for (auto& obj : mAllGameObjects) {
		if (obj->GetObjType() != ObjectsType::CHARACTER) continue;

		// 이미 매핑된 오브젝트인지 검사
		bool taken = false;
		for (auto& [k, v] : mNetworkObjects)
			if (v == obj) { taken = true; break; }
		if (taken) continue;

		// 바인딩: 서버 스폰 위치로 이동시키고 매핑 등록
		float y = mTerrain.GetHeight(u.position.x, u.position.z);
		obj->SetMaxHP(GetDefaultMaxHp((ObjType)u.objType));
		obj->SetPosition(u.position.x, y, u.position.z);

		mNetworkObjects[NetKey(u.ownerPlayer, u.objNumber)] = obj;
		RegisterVisionObject(obj, u.ownerPlayer);

		std::cout << "  bind: owner " << (int)u.ownerPlayer
			<< " obj " << (int)u.objNumber << "\n";
		return;
	}

	std::cout << "[Net] WARN: no free local object for owner "
		<< (int)u.ownerPlayer << " obj " << (int)u.objNumber << "\n";
}

void DummyApp::ProcessReceivedPackets()
{
	if (!mNetworkBridge) return;

	auto packets = mNetworkBridge->DequeueRecvAll();
	for (auto& pkt : packets)
	{
		if (pkt.length < 2) continue;
		unsigned char packetType = pkt.data[1]; // [0]=size, [1]=type

		switch (packetType)
		{
		case SC_MOVE_OBJ_RESULT:
			OnMoveObjResult(
				reinterpret_cast<const SCMoveObjResult*>(pkt.data));
			break;

		case SC_MOVE_MULTI_RESULT:
			OnMoveMultiResult(
				reinterpret_cast<const SCMoveMultiResult*>(pkt.data));
			break;

		case SC_OBJ_POSITION_SYNC:
			OnPositionSync(
				reinterpret_cast<const SCPositionSync*>(pkt.data));
			break;

		case SC_MOVE_REJECTED:
			OnMoveRejected(
				reinterpret_cast<const SCMoveRejected*>(pkt.data));
			break;

		case SC_HACK_WARNING:
			OnHackWarning(
				reinterpret_cast<const SCHackWarning*>(pkt.data));
			break;

		case SC_LINK_RESULT:
			OnLinkResult(reinterpret_cast<const SCLinkResult*>(pkt.data));
			break;
		case SC_GAME_START:
			OnGameStart(reinterpret_cast<const SCGameStart*>(pkt.data));
			break;

		case SC_BUILD_RESULT:
			OnBuildResult(reinterpret_cast<const SCBuildResult*>(pkt.data));
			break;
		case SC_UNIT_PRODUCED:
			OnUnitProduced(
				reinterpret_cast<
				const SCUnitProduced*>(
					pkt.data));
			break;

		case SC_PLAYER_LEFT:
			OnPlayerLeft(reinterpret_cast<const SCPlayerLeft*>(pkt.data));
			break;
		case SC_ATTACK_RESULT:
			OnAttackResult(reinterpret_cast<const SCAttackResult*>(pkt.data));
			break;
		case SC_OBJ_DEAD:
			OnObjDead(reinterpret_cast<const SCObjDead*>(pkt.data));
			break;
		default:
			std::cout << "[Net] unknown packet type: "
				<< (int)packetType << std::endl;
			break;
		}
	}
}

//-----------------------------------------------------------------
// 서버가 검증 완료한 단일 이동 결과 (내 것 + 남의 것 모두 수신)
//-----------------------------------------------------------------
void DummyApp::OnMoveObjResult(const SCMoveObjResult* p)
{
	GameObject* obj = FindNetworkObject(p->ownerPlayer, p->objNumber);
	if (!obj) return;

	Player* unit = dynamic_cast<Player*>(obj);
	if (!unit) return;

	float destY = mTerrain.GetHeight(p->destination.x, p->destination.z);
	XMFLOAT3 serverDest(p->destination.x, destY, p->destination.z);

	//--- 내 유닛: 이미 A*로 이동 중 (예측). 서버가 목적지를
	//    보정했을 때만 서버 확정 목적지로 갈아탄다.
	if (p->ownerPlayer == mMyPlayerNumber)
	{
		XMFLOAT3 fin = unit->GetFinalDestination();  // ※ getter 없으면 Player에 추가
		float dx = fin.x - serverDest.x;
		float dz = fin.z - serverDest.z;

		if (dx * dx + dz * dz > 25.0f) {   // 5유닛 이상 보정됨 → 서버 우선
			unit->SetFinalDestination(serverDest);
			unit->ClearPath();
			unit->SetDestination(serverDest);
			unit->SetFollowerKeyInput(FollowerKeyInput::Move);
			unit->FollowerEvent();
			std::cout << "[Net] my move corrected by server\n";
		}
		return;   // 보정 없으면 아무것도 안 함 (예측 신뢰)
	}

	//--- 상대 유닛: 서버 결과로 이동 시작 (기존 로직)
	XMFLOAT3 cur = unit->GetPosition();
	float dx = cur.x - p->currentPos.x;
	float dz = cur.z - p->currentPos.z;
	if (dx * dx + dz * dz > 100.0f) {
		float y = mTerrain.GetHeight(p->currentPos.x, p->currentPos.z);
		unit->SetPosition(p->currentPos.x, y, p->currentPos.z);
	}

	unit->ClearPath();
	unit->SetDestination(serverDest);
	unit->SetFollowerKeyInput(FollowerKeyInput::Move);
	unit->FollowerEvent();
}

//-----------------------------------------------------------------
// 다중 이동 결과
//-----------------------------------------------------------------
void DummyApp::OnMoveMultiResult(const SCMoveMultiResult* p)
{
	//--- 내 유닛 묶음: 예측 신뢰, 개별 보정은 위치동기화가 처리
	if (p->ownerPlayer == mMyPlayerNumber)
		return;

	//--- 상대 유닛: 기존 로직 그대로
	int count = (p->objCount < MAX_MULTI_MOVE) ? p->objCount : MAX_MULTI_MOVE;

	for (int i = 0; i < count; ++i)
	{
		GameObject* obj = FindNetworkObject(p->ownerPlayer, p->objNumbers[i]);
		if (!obj) continue;

		Player* unit = dynamic_cast<Player*>(obj);
		if (!unit) continue;

		const FXYZ& d = p->destinations[i];
		float destY = mTerrain.GetHeight(d.x, d.z);

		unit->ClearPath();
		unit->SetDestination(XMFLOAT3(d.x, destY, d.z));
		unit->SetFollowerKeyInput(FollowerKeyInput::Move);
		unit->FollowerEvent();
	}
}

//-----------------------------------------------------------------
// 주기적 위치 동기화 (서버 → 전원, POSITION_SYNC_INTERVAL 주기)
//  ※ 내 소유 오브젝트는 스냅하지 않고 임계값 초과 시에만 보정
//-----------------------------------------------------------------
void DummyApp::OnPositionSync(const SCPositionSync* p)
{
	int count = (p->objCount < MAX_SYNC_OBJECTS) ? p->objCount : MAX_SYNC_OBJECTS;

	for (int i = 0; i < count; ++i)
	{
		const SCSyncEntry& e = p->entries[i];
		GameObject* obj = FindNetworkObject(e.ownerPlayer, e.objNumber);
		if (!obj) continue;

		XMFLOAT3 cur = obj->GetPosition();
		float dx = cur.x - e.position.x;
		float dz = cur.z - e.position.z;
		float errSq = dx * dx + dz * dz;

		bool isMine = (e.ownerPlayer == mMyPlayerNumber);
		float snapThresholdSq = isMine ? 400.0f : 25.0f; // 내 것 20u, 남의 것 5u

		if (errSq > snapThresholdSq) {
			float y = mTerrain.GetHeight(e.position.x, e.position.z);
			obj->SetPosition(e.position.x, y, e.position.z);
		}
	}
}

//-----------------------------------------------------------------
// 이동 거부 → 서버가 지정한 위치로 강제 보정
//-----------------------------------------------------------------
void DummyApp::OnMoveRejected(const SCMoveRejected* p)
{
	static const char* reasons[] = { "collision", "speed", "no-auth", "out-of-map" };
	std::cout << "[Net] move rejected, obj " << (int)p->objNumber
		<< " reason=" << reasons[p->reason % 4] << std::endl;

	GameObject* obj = FindNetworkObject(mMyPlayerNumber, p->objNumber);
	if (!obj) return;

	Player* unit = dynamic_cast<Player*>(obj);
	if (unit) {
		unit->ClearPath();
		float y = mTerrain.GetHeight(p->correctedPos.x, p->correctedPos.z);
		unit->SetPosition(p->correctedPos.x, y, p->correctedPos.z);
	}
}

void DummyApp::OnHackWarning(const SCHackWarning* p)
{
	std::cout << "[Net] HACK WARNING " << (int)p->warningCount
		<< "/" << (int)p->maxWarnings << std::endl;
	// TODO: UI 경고 표시
}

//-----------------------------------------------------------------
// objNumber → GameObject 매핑
//  TODO: 오브젝트 생성 시 (ownerPlayer, objNumber)를 부여하고
//        unordered_map<int, GameObject*> 로 관리하는 게 좋음.
//        임시로 mAllGameObjects 선형 탐색 예시:
//-----------------------------------------------------------------
GameObject* DummyApp::FindNetworkObject(int ownerPlayer, unsigned char objNumber)
{
	auto iter = mNetworkObjects.find(NetKey(ownerPlayer, objNumber));
	
	if (iter != mNetworkObjects.end())
		return iter->second;

	return nullptr;
}


//=================================================================
// [4] DummyApp.cpp — 송신 헬퍼
//     PickingMove()에서 로컬 A* 대신(또는 A*와 병행해서) 호출.
//     서버 권위 구조이므로: 요청만 보내고, 실제 이동 시작은
//     SC_MOVE_*_RESULT 수신 시점에 한다.
//=================================================================

void DummyApp::SendMoveRequest(unsigned char objNumber, const XMFLOAT3& dest)
{
	if (!mNetworkBridge) return;

	CSMoveObjRequest pkt;
	pkt.playerNumber = (unsigned char)mMyPlayerNumber;
	pkt.objNumber = objNumber;
	pkt.destination = { dest.x, dest.y, dest.z };

	mNetworkBridge->EnqueueSend(pkt);
}

void DummyApp::SendMultiMoveRequest(const std::vector<unsigned char>& objNumbers,
	const XMFLOAT3& dest)
{
	if (!mNetworkBridge || objNumbers.empty()) return;

	CSMoveMultiRequest pkt;
	pkt.playerNumber = (unsigned char)mMyPlayerNumber;
	pkt.objCount = (unsigned char)std::min<size_t>(objNumbers.size(), MAX_MULTI_MOVE);
	for (int i = 0; i < pkt.objCount; ++i)
		pkt.objNumbers[i] = objNumbers[i];
	pkt.destination = { dest.x, dest.y, dest.z };

	mNetworkBridge->EnqueueSend(pkt);
}

void DummyApp::SendStopRequest(unsigned char objNumber)
{
	if (!mNetworkBridge) return;

	CSStopObjRequest pkt;
	pkt.playerNumber = (unsigned char)mMyPlayerNumber;
	pkt.objNumber = objNumber;

	mNetworkBridge->EnqueueSend(pkt);
}