#pragma once
#include "d3dUtil.h"

#define VERTEXT_POSITION				0x01
#define VERTEXT_COLOR					0x02
#define VERTEXT_NORMAL					0x04
#define VERTEXT_TANGENT					0x08
#define VERTEXT_TEXTURE_COORD0			0x10
#define VERTEXT_TEXTURE_COORD1			0x20

#define VERTEXT_TEXTURE					(VERTEXT_POSITION | VERTEXT_TEXTURE_COORD0)
#define VERTEXT_DETAIL					(VERTEXT_POSITION | VERTEXT_TEXTURE_COORD0 | VERTEXT_TEXTURE_COORD1)
#define VERTEXT_NORMAL_TEXTURE			(VERTEXT_POSITION | VERTEXT_NORMAL | VERTEXT_TEXTURE_COORD0)
#define VERTEXT_NORMAL_TANGENT_TEXTURE	(VERTEXT_POSITION | VERTEXT_NORMAL | VERTEXT_TANGENT | VERTEXT_TEXTURE_COORD0)
#define VERTEXT_NORMAL_DETAIL			(VERTEXT_POSITION | VERTEXT_NORMAL | VERTEXT_TEXTURE_COORD0 | VERTEXT_TEXTURE_COORD1)
#define VERTEXT_NORMAL_TANGENT__DETAIL	(VERTEXT_POSITION | VERTEXT_NORMAL | VERTEXT_TANGENT | VERTEXT_TEXTURE_COORD0 | VERTEXT_TEXTURE_COORD1)

struct SubmeshGeometry
{
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	INT BaseVertexLocation = 0;

	// 서브메쉬의 바운딩 박스
	DirectX::BoundingBox Bounds;
};

struct MeshGeometry
{
	// 메시를 이름으로 조회
	std::string Name;

	UINT							mType = 0x00;

	XMFLOAT3						AABBCenter = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3						AABBExtents = XMFLOAT3(0.0f, 0.0f, 0.0f);

	D3D12_PRIMITIVE_TOPOLOGY		mPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	int vertexCnt = 0;
	int indexCnt = 0;

	int vertexMemberCnt = 0;

	// 시스템 메모리 복사본.
	// 정점/색인 형식이 범용적일 수 있으므로 ID3DBlob을 사용한다.
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	// 버퍼들에 관한 자료
	UINT VertexByteStride = 0;
	UINT VertexBufferByteSize = 0;

	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R32_UINT;
	UINT IndexBufferByteSize = 0;

	// 한 MeshGeometry 인스터스의 한 정점/색인 버퍼에 여러개의 기하구조를 담을 수 있다.
	// 부분 메시들을 개별적으로 그릴 수 있도록, 부분 메시 기하구조들을 컨테이너에 담아둔다.
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const;
	int VertexBufferViewMemberCnt()const;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView()const;

	void DisposeUploaders();

	void LoadMeshFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, FILE* file);
};