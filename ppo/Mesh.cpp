#include "Mesh.h"

Mesh::Mesh()
{
}

Mesh::~Mesh()
{
}

void Mesh::AddSubmesh(const string name, UINT numIndices, UINT baseVertex, UINT baseIndex, UINT materialIndex)
{
	Submesh submesh;
	
	submesh.numIndices = numIndices;
	submesh.baseVertex = baseVertex;
	submesh.baseIndex = baseIndex;
	submesh.materialIndex = materialIndex;

	mSubmeshes[name] = submesh;
}

void Mesh::UploadBuffer(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, 
	vector<Vertex> vertices, vector<UINT> indices)
{
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(UINT);

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mVertexBufferCPU));
	CopyMemory(mVertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mIndexBufferCPU));
	CopyMemory(mIndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mVertexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice.Get(),
		commandList.Get(), vertices.data(), vbByteSize, mVertexBufferUploader);

	mIndexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice.Get(),
		commandList.Get(), indices.data(), ibByteSize, mIndexBufferUploader);

	mVertexByteStride = sizeof(Vertex);
	mVertexBufferByteSize = vbByteSize;
	mIndexFormat = DXGI_FORMAT_R32_UINT;
	mIndexBufferByteSize = ibByteSize;
}

D3D12_VERTEX_BUFFER_VIEW Mesh::VertexBufferView() const
{
	if (mVertexBufferGPU != nullptr) {
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = mVertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = mVertexByteStride;
		vbv.SizeInBytes = mVertexBufferByteSize;

		return vbv;
	}
}

D3D12_INDEX_BUFFER_VIEW Mesh::IndexBufferView() const
{
	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = mIndexBufferGPU->GetGPUVirtualAddress();
	ibv.Format = mIndexFormat;
	ibv.SizeInBytes = mIndexBufferByteSize;

	return ibv;
}

// 자료를 GPU에 모두 올린 후에는 메모리를 해제해도 된다.
void Mesh::DisposeUploaders()
{
	mVertexBufferUploader = nullptr;
	mIndexBufferUploader = nullptr;
}

//void Mesh::LoadMeshFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, FILE* file)
//{
//	char pstrToken[64] = { '\0' };
//	BYTE strLength = 0;
//
//	int positionCnt = 0, colorCnt = 0, normalCnt = 0,
//		tangentCnt = 0, biTangentCnt = 0, textureCoordCnt = 0,
//		indexCnt = 0, subMesheCnt = 0, subIndexCnt = 0;
//
//	XMFLOAT3* positions = nullptr;
//	XMFLOAT3* normals = nullptr;
//	XMFLOAT2* textureCoords0 = nullptr;
//
//	UINT nReads = (UINT)::fread(&positionCnt, sizeof(int), 1, file);
//	
//	nReads = (UINT)::fread(&strLength, sizeof(BYTE), 1, file);
//	char meshName[256];
//	nReads = (UINT)::fread(meshName, sizeof(char), strLength, file);
//	meshName[strLength] = '\0';
//	//Name = meshName;
//
//	for (; ; )
//	{
//		nReads = (UINT)::fread(&strLength, sizeof(BYTE), 1, file);
//		nReads = (UINT)::fread(pstrToken, sizeof(char), strLength, file);
//		pstrToken[strLength] = '\0';
//
//		if (!strcmp(pstrToken, "<Bounds>:")) {
//			nReads = (UINT)::fread(&AABBCenter, sizeof(XMFLOAT3), 1, file);
//			nReads = (UINT)::fread(&AABBExtents, sizeof(XMFLOAT3), 1, file);
//		}
//		else if (!strcmp(pstrToken, "<Positions>:")) {
//			nReads = (UINT)::fread(&positionCnt, sizeof(int), 1, file);
//
//			if (positionCnt > 0) {
//				mType |= VERTEXT_POSITION;
//				positions = new XMFLOAT3[positionCnt];
//				nReads = (UINT)::fread(positions, sizeof(XMFLOAT3), positionCnt, file);
//			}
//		}
//		else if (!strcmp(pstrToken, "<Colors>:")) {
//			nReads = (UINT)::fread(&colorCnt, sizeof(int), 1, file);
//
//			if (colorCnt > 0) {
//				mType |= VERTEXT_COLOR;
//				XMFLOAT4* colors = new XMFLOAT4[colorCnt];
//				nReads = (UINT)::fread(colors, sizeof(XMFLOAT4), colorCnt, file);
//
//				delete[] colors;
//			}
//		}
//		else if (!strcmp(pstrToken, "<TextureCoords0>:")) {
//			nReads = (UINT)::fread(&textureCoordCnt, sizeof(int), 1, file);
//
//			if (textureCoordCnt > 0) {
//				mType |= VERTEXT_TEXTURE_COORD0;
//				textureCoords0 = new XMFLOAT2[textureCoordCnt];
//				nReads = (UINT)::fread(textureCoords0, sizeof(XMFLOAT2), textureCoordCnt, file);
//			}
//		}
//		else if (!strcmp(pstrToken, "<TextureCoords1>:")) {
//			nReads = (UINT)::fread(&textureCoordCnt, sizeof(int), 1, file);
//
//			if (textureCoordCnt > 0) {	// 무시
//				/*
//				mType |= VERTEXT_TEXTURE_COORD1;
//				XMFLOAT2* textureCoords1 = new XMFLOAT2[textureCoordCnt];
//				nReads = (UINT)::fread(textureCoords1, sizeof(XMFLOAT2), textureCoordCnt, file);
//
//				ThrowIfFailed(D3DCreateBlob(1, &TextureCoord1BufferCPU));
//				CopyMemory(TextureCoord1BufferCPU->GetBufferPointer(), textureCoords1, sizeof(XMFLOAT2) * textureCoordCnt);
//
//				TextureCoord0BufferGPU = d3dUtil::CreateDefaultBuffer(device, commandList,
//					textureCoords1, sizeof(XMFLOAT2) * textureCoordCnt, TextureCoord1BufferUploader);
//
//				delete[] textureCoords1;
//				*/
//				nReads = (UINT)::fseek(file, sizeof(XMFLOAT2) * textureCoordCnt, SEEK_CUR);
//			}
//		}
//		else if (!strcmp(pstrToken, "<Normals>:")) {
//			nReads = (UINT)::fread(&normalCnt, sizeof(int), 1, file);
//
//			if (normalCnt > 0) {
//				mType |= VERTEXT_NORMAL;
//				normals = new XMFLOAT3[normalCnt];
//				nReads = (UINT)::fread(normals, sizeof(XMFLOAT3), normalCnt, file);
//			}
//		}
//		else if (!strcmp(pstrToken, "<Tangents>:")) {
//			nReads = (UINT)::fread(&tangentCnt, sizeof(int), 1, file);
//
//			if (tangentCnt > 0) {
//				/*
//				mType |= VERTEXT_TANGENT;
//				XMFLOAT3* tangents = new XMFLOAT3[tangentCnt];
//				nReads = (UINT)::fread(tangents, sizeof(XMFLOAT3), tangentCnt, file);
//
//				delete[] tangents;
//				*/
//				nReads = (UINT)::fseek(file, sizeof(XMFLOAT3) * tangentCnt, SEEK_CUR);
//			}
//		}
//		else if (!strcmp(pstrToken, "<BiTangents>:")) {
//			nReads = (UINT)::fread(&biTangentCnt, sizeof(int), 1, file);
//
//			if (biTangentCnt > 0) {
//				/*
//				XMFLOAT3* biTangents = new XMFLOAT3[biTangentCnt];
//				nReads = (UINT)::fread(biTangents, sizeof(XMFLOAT3), biTangentCnt, file);
//				
//				delete[] biTangents;
//				*/
//				nReads = (UINT)::fseek(file, sizeof(XMFLOAT3) * biTangentCnt, SEEK_CUR);
//			}
//		}
//		else if (!strcmp(pstrToken, "<Indices>:")) {
//			nReads = (UINT)::fread(&indexCnt, sizeof(int), 1, file);
//			
//			if (indexCnt > 0) {
//				UINT* indices = new UINT[indexCnt];
//				nReads = (UINT)::fread(indices, sizeof(UINT), indexCnt, file);
//
//				IndexBufferByteSize = sizeof(UINT) * indexCnt;
//
//				ThrowIfFailed(D3DCreateBlob(sizeof(UINT) * indexCnt, &IndexBufferCPU));
//				CopyMemory(IndexBufferCPU->GetBufferPointer(), indices, sizeof(UINT)* indexCnt);
//
//				IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, commandList,
//					indices, sizeof(UINT) * indexCnt, IndexBufferUploader);
//
//				delete[] indices;
//			}
//		}
//		else if (!strcmp(pstrToken, "<SubMeshes>:")) {
//			nReads = (UINT)::fread(&subMesheCnt, sizeof(int), 1, file);
//
//			if (subMesheCnt > 0) {
//				for (int i = 0; i < subMesheCnt; i++)
//				{
//					nReads = (UINT)::fread(&strLength, sizeof(BYTE), 1, file);
//					nReads = (UINT)::fread(pstrToken, sizeof(char), strLength, file);
//					pstrToken[strLength] = '\0';
//
//					if (!strcmp(pstrToken, "<SubMesh>:")) {
//						SubmeshGeometry subMesh;
//
//						subMesh.BaseVertexLocation = 0;
//						nReads = (UINT)::fread(&subMesh.StartIndexLocation, sizeof(int), 1, file);
//						nReads = (UINT)::fread(&subMesh.IndexCount, sizeof(int), 1, file);
//
//						mSubmeshes["subMeshName"] = subMesh;
//					}
//				}
//			}
//		}
//		else if (!strcmp(pstrToken, "</Mesh>"))
//		{
//			// vertex..
//			struct Vertex {
//				XMFLOAT3 position;
//				XMFLOAT3 normal;
//				XMFLOAT2 textureCoord0;
//			};
//
//			Vertex* vertices = new Vertex[positionCnt];
//
//			for (int i = 0; i < positionCnt; i++)
//			{
//				Vertex vertex;
//				vertex.position = positions[i];
//				vertex.normal = normals[i];
//				vertex.textureCoord0 = textureCoords0[i];
//
//				vertices[i] = vertex;
//			}
//
//			VertexByteStride = sizeof(Vertex);
//			VertexBufferByteSize = sizeof(Vertex) * positionCnt;
//
//			ThrowIfFailed(D3DCreateBlob(sizeof(Vertex) * positionCnt, &VertexBufferCPU));
//			CopyMemory(VertexBufferCPU->GetBufferPointer(), vertices, sizeof(Vertex)* positionCnt);
//
//			VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, commandList,
//				vertices, sizeof(Vertex) * positionCnt, VertexBufferUploader);
//
//			delete[] vertices;
//			delete[] positions;
//			delete[] normals;
//			delete[] textureCoords0;
//			break;
//		}
//	}
//}