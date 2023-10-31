
#include "DummyApp.h"

const int gNumFrameResources = 3;

DummyApp::DummyApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

DummyApp::~DummyApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
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

	mCamera.SetPosition(0.0f, 0.0f, -10.0f);

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildSkullGeometry();
	BuildSkullGeometryTest();
	LoadTerrain();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// 초기화 명령 실행
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 초기화 명령들이 모두 처리되기 기다린다.
	FlushCommandQueue();

	return true;
}

void DummyApp::OnResize()
{
	D3DApp::OnResize();

	// 창의 크기가 변경되었기 때문에 종횡비를 갱신하고
	// 투영행렬을 다시 계산한다.
	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), mCamera.GetNearZ(), mCamera.GetFarZ());
}

void DummyApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

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
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
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
	if (mIsWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
	}
	else if (mIsToonShading)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_toonShading"].Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
	}

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
	mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	// 이 장면에 쓰이는 모든 재질을 묶는다.
	// 구조적 버퍼는 힙을 생략하고 그냥 하나의 루트 서술자로 묶을 수 있다.
	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	skyTexDescriptor.Offset(mSkyTexHeapIndex, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(3, skyTexDescriptor);

	// 이 장면에 쓰이는 모든 텍스처를 묶는다. 
	// 테이블의 첫 서술자만 지정하면 된다.
	// 테이블에 몇 개의 서술자가 있는지는 루트 서명에 설정되어 있다.
	mCommandList->SetGraphicsRootDescriptorTable(4, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);



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

void DummyApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void DummyApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void DummyApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

bool DummyApp::OnKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case '1':
			mIsWireframe = false;
			mIsToonShading = false;
			return(false);
		case '2':
			mIsWireframe = true;
			mIsToonShading = false;
			return(false);
		case '3':
			mIsWireframe = false;
			mIsToonShading = true;
			return(false);
		}
		return(false);
	}

	return(false);
}

void DummyApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(50.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-50.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-50.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(50.0f * dt);

	if (GetAsyncKeyState(VK_SPACE) & 0x8000)
		mCamera.Up(50.0f * dt);

	if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
		mCamera.Up(-50.0f * dt);

	mCamera.UpdateViewMatrix();
}

void DummyApp::AnimateMaterials(const GameTimer& gt)
{
}

void DummyApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		// 상수들이 바뀌었을 때에만 cbuffer 자료를 갱신한다.
		// 이러한 갱신을 프레임 자원마다 수행해야한다.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			objConstants.MaterialIndex = e->Mat->MatCBIndex;

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// 다음 프레임 자원으로 넘어간다.
			e->NumFramesDirty--;
		}
	}
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
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

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
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = mCamera.GetNearZ();
	mMainPassCB.FarZ = mCamera.GetFarZ();
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	mMainPassCB.AmbientLight = { 0.2f, 0.2f, 0.2f, 1.0f };

	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };
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
		"bricksDiffuseMap",
		"stoneDiffuseMap",
		"tileDiffuseMap",
		"terrainDiffuseMap",
		"skyCubeMap"
	};

	std::vector<std::wstring> texFilenames =
	{
		L"Textures/bricks.dds",
		L"Textures/stone.dds",
		L"Textures/tile.dds",
		L"Textures/terrainColorMap.dds",
		L"Textures/grasscube1024.dds"
	};

	for (int i = 0; i < (int)texNames.size(); ++i)
	{
		auto texMap = std::make_unique<Texture>();
		texMap->Name = texNames[i];
		texMap->Filename = texFilenames[i];
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), texMap->Filename.c_str(),
			texMap->Resource, texMap->UploadHeap));

		mTextures[texMap->Name] = std::move(texMap);
	}
}

void DummyApp::BuildRootSignature()
{
	// 일반적으로 셰이더 프로그램은 특정 자원들(상수 버퍼, 텍스처, 표본추출기 등)이 입력된다고 기대한다.
	// 루트 서명은 셰이더 프로그램이 기대하는 자원들을 정의한다.
	// 셰이더 프로그램은 본질적으로 하나의 함수이고 셰이더에 입력되는 자원들은 
	// 함수의 매개변수들에 해당하므로, 루트 서명은 곧 함수 서명을 정의하는 수단이라 할 수 있다.

	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 1, 0);

	// 루트 매개변수는 서술자 테이블이거나 루트 서술자 또는 루트 상수이다.
	CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	// 루트 CBV 생성한다.
	// 성능 팁: 사용빈도가 높은것에서 낮은것 순서대로 배열한다.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstantBufferView(1);
	slotRootParameter[2].InitAsShaderResourceView(0, 1);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();

	// 루트 서명은 루트 매개변수들의 배열이다.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter, 
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// 상수 버퍼 하나로 구성된 서술자 구간을 가리키는
	// 슬롯 하나로 이루어진 루트 서명을 생성한다.
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
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
	srvHeapDesc.NumDescriptors = 5;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	// 텍스처 자원이 이미 로드되어 있을때

	// 힙의 시작을 가리키는 포인터를 얻는다.
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto bricksTex = mTextures["bricksDiffuseMap"]->Resource;
	auto stoneTex = mTextures["stoneDiffuseMap"]->Resource;
	auto tileTex = mTextures["tileDiffuseMap"]->Resource;
	auto terrainTex = mTextures["terrainDiffuseMap"]->Resource;
	auto skyTex = mTextures["skyCubeMap"]->Resource;

	// 텍스처에 대한 실제 서술자들을 앞에서 생성한 힙에 생성한다.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = bricksTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
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

	// 다음 서술자로 넘어간다. skyCubeMap
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = skyTex->GetDesc().MipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = skyTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(skyTex.Get(), &srvDesc, hDescriptor);

	mSkyTexHeapIndex = 4;
}

void DummyApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", 
		nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", 
		nullptr, "PS", "ps_5_1");
	mShaders["toonLightingOpaquePS"] = d3dUtil::CompileShader(L"Shaders\\ToonLighting.hlsl", 
		nullptr, "PS", "ps_5_1");

	mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", 
		nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", 
		nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, 
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, 
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void DummyApp::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
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

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

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

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void DummyApp::BuildSkullGeometry()
{
	std::ifstream fin("Models/skinnedMeshData.txt");

	if (!fin)
	{
		MessageBox(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;
	std::vector<Vertex> vertices;
	std::vector<std::uint32_t> indices;
	uint32_t index;
	uint32_t dindex = 0;
	for (int k = 0; k < 5; k++)
	{
		fin >> ignore >> vcount;
		fin >> ignore >> tcount;
		fin >> ignore >> ignore >> ignore >> ignore >> ignore;


		for (UINT i = 0; i < vcount; ++i)
		{
			Vertex vertex;
			fin >> vertex.Pos.x >> vertex.Pos.y >> vertex.Pos.z;
			fin >> vertex.Normal.x >> vertex.Normal.y >> vertex.Normal.z;
			fin >> vertex.TexC.x >> vertex.TexC.y;

			vertices.push_back(vertex);
		}

		fin >> ignore;
		fin >> ignore;
		fin >> ignore;


		for (UINT i = 0; i < tcount; ++i)
		{
			fin >> index;
			indices.push_back(index + dindex);
			fin >> index;
			indices.push_back(index + dindex);
			fin >> index;
			indices.push_back(index + dindex);
			//fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
		}
		fin >> ignore;

		dindex += vcount;
	}
	
	fin.close();

	//
	// Pack the indices of all the meshes into one index buffer.
	//
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skullGeo";

	//ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	//CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	//ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	//CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["skull"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}

void DummyApp::BuildSkullGeometryTest()
{
	/*
	GameObject *gameObject = GameObject::LoadGeometryFromFile(md3dDevice.Get(), mCommandList.Get(), 
		"Models/Gunship.bin");
	*/
}

void DummyApp::LoadTerrain()
{
	mTerrain.LoadHeightMap(L"HeightMap/heightmap.r16", 1025, 1025, XMFLOAT3(1.0f, 0.01f, 1.0f));
	
	UINT vcount = 1025 * 1025;
	UINT tcount = 1024 * 1024 * 2 * 3;
	
	//
	// Pack the indices of all the meshes into one index buffer.
	//
	
	std::vector<Vertex> vertices(vcount);
	std::vector<uint32_t> indices(tcount);

	mTerrain.CreateTerrain(2000.0f, 2000.f, vertices, indices);

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "terrain";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["terrain"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
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
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
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
	skyPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));
}

void DummyApp::BuildFrameResources()
{
	// gNumFrameResources개의 FrameResources를 담는 mFrameResources 벡터를 생성한다.
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
	}
}

void DummyApp::BuildMaterials()
{
	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	auto stone0 = std::make_unique<Material>();
	stone0->Name = "stone0";
	stone0->MatCBIndex = 1;
	stone0->DiffuseSrvHeapIndex = 1;
	stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	stone0->Roughness = 0.3f;

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	tile0->MatCBIndex = 2;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.3f;

	auto skullMat = std::make_unique<Material>();
	skullMat->Name = "skullMat";
	skullMat->MatCBIndex = 3;
	skullMat->DiffuseSrvHeapIndex = 0;
	skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	skullMat->Roughness = 0.3f;

	auto terrainMat = std::make_unique<Material>();
	terrainMat->Name = "terrainMat";
	terrainMat->MatCBIndex = 4;
	terrainMat->DiffuseSrvHeapIndex = 3;
	terrainMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	terrainMat->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	terrainMat->Roughness = 0.05f;

	auto sky = std::make_unique<Material>();
	sky->Name = "sky";
	sky->MatCBIndex = 5;
	sky->DiffuseSrvHeapIndex = 4;
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["stone0"] = std::move(stone0);
	mMaterials["tile0"] = std::move(tile0);
	mMaterials["skullMat"] = std::move(skullMat);
	mMaterials["terrainMat"] = std::move(terrainMat);
	mMaterials["sky"] = std::move(sky);
}

void DummyApp::BuildRenderItems()
{
	UINT objCBIndex = 0;

	auto skyRitem = std::make_unique<RenderItem>();
	skyRitem->TexTransform = MathHelper::Identity4x4();
	skyRitem->ObjCBIndex = objCBIndex++;
	skyRitem->Mat = mMaterials["sky"].get();
	skyRitem->Geo = mGeometries["shapeGeo"].get();
	skyRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(std::move(skyRitem));

	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f));
	XMStoreFloat4x4(&boxRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem->ObjCBIndex = objCBIndex++;
	boxRitem->Mat = mMaterials["bricks0"].get();
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());
	mAllRitems.push_back(std::move(boxRitem));

	/*
	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	gridRitem->ObjCBIndex = objCBIndex++;
	gridRitem->Mat = mMaterials["tile0"].get();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mAllRitems.push_back(std::move(gridRitem));
	*/
	
	auto skullRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&skullRitem->World, XMMatrixScaling(5.0f, 5.0f, 5.0f) * XMMatrixTranslation(0.0f, 20.0f, 0.0f));
	skullRitem->TexTransform = MathHelper::Identity4x4();
	skullRitem->ObjCBIndex = objCBIndex++;
	skullRitem->Mat = mMaterials["bricks0"].get();
	skullRitem->Geo = mGeometries["skullGeo"].get();
	skullRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(skullRitem.get());
	mAllRitems.push_back(std::move(skullRitem));
	
	auto terrainRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&terrainRitem->World, XMMatrixTranslation(0.0f, -200.0f, 0.0f));
	terrainRitem->TexTransform = MathHelper::Identity4x4();
	terrainRitem->ObjCBIndex = objCBIndex++;
	terrainRitem->Mat = mMaterials["terrainMat"].get();
	terrainRitem->Geo = mGeometries["terrain"].get();
	terrainRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	terrainRitem->IndexCount = terrainRitem->Geo->DrawArgs["terrain"].IndexCount;
	terrainRitem->StartIndexLocation = terrainRitem->Geo->DrawArgs["terrain"].StartIndexLocation;
	terrainRitem->BaseVertexLocation = terrainRitem->Geo->DrawArgs["terrain"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(terrainRitem.get());
	mAllRitems.push_back(std::move(terrainRitem));
	
	XMMATRIX brickTexTransform = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
		XMStoreFloat4x4(&leftCylRitem->TexTransform, brickTexTransform);
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Mat = mMaterials["bricks0"].get();
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();
		leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
		XMStoreFloat4x4(&rightCylRitem->TexTransform, brickTexTransform);
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Mat = mMaterials["bricks0"].get();
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->TexTransform = MathHelper::Identity4x4();
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Mat = mMaterials["stone0"].get();
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->TexTransform = MathHelper::Identity4x4();
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Mat = mMaterials["stone0"].get();
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftCylRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightCylRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftSphereRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());

		mAllRitems.push_back(std::move(leftCylRitem));
		mAllRitems.push_back(std::move(rightCylRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));
	}
}

void DummyApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();

	// 각 렌더항목에 대해:
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// 현재 프레임 자원에 대한 이 물체를 위한 CBV의 오프셋을 구한다.
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() 
			+ ri->ObjCBIndex * objCBByteSize;
		
		// CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		// tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, 
			ri->StartIndexLocation, ri->BaseVertexLocation, 0);
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


