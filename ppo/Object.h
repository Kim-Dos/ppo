#pragma once

#include "d3dUtil.h"

class Object
{
};

// 하나의 물체를 그리는 데 필요한 매개변수들을 담는 가벼운 구조체
struct RenderItem
{
	RenderItem() = default;

	// 세계 공간을 기준으로 물체의 국소 공간을 서술하는 세계 행렬
	// 이 행렬은 세계 공간 안에서의 물체의 의치와 방행, 크기를 결정
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