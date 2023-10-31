#include "Mesh.h"
#include "d3dUtil.h"

D3D12_VERTEX_BUFFER_VIEW MeshGeometry::VertexBufferView() const
{
	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
	vbv.StrideInBytes = VertexByteStride;
	vbv.SizeInBytes = VertexBufferByteSize;

	return vbv;
}

D3D12_INDEX_BUFFER_VIEW MeshGeometry::IndexBufferView() const
{
	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
	ibv.Format = IndexFormat;
	ibv.SizeInBytes = IndexBufferByteSize;

	return ibv;
}

// 자료를 GPU에 모두 올린 후에는 메모리를 해제해도 된다.
void MeshGeometry::DisposeUploaders()
{
	VertexBufferUploader = nullptr;
	IndexBufferUploader = nullptr;
}

void MeshGeometry::LoadMeshFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, FILE* file)
{
	char pstrToken[64] = { '\0' };
	BYTE nStrLength = 0;

	int nPositions = 0, nColors = 0, nNormals = 0, 
		nTangents = 0, nBiTangents = 0, nTextureCoords = 0, 
		nIndices = 0, nSubMeshes = 0, nSubIndices = 0;

	UINT nReads = (UINT)::fread(&nPositions, sizeof(int), 1, file);
	nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, file);
	char meshName[256];
	nReads = (UINT)::fread(meshName, sizeof(char), nStrLength, file);
	meshName[nStrLength] = '\0';
	//Name = meshName;

	for (; ; )
	{
		nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, file);
		nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, file);
		pstrToken[nStrLength] = '\0';

		if (!strcmp(pstrToken, "<Bounds>:")) {
			nReads = (UINT)::fread(&mAABBCenter, sizeof(XMFLOAT3), 1, file);
			nReads = (UINT)::fread(&mAABBExtents, sizeof(XMFLOAT3), 1, file);
		}
		else if (!strcmp(pstrToken, "<Positions>:")) {
			nReads = (UINT)::fread(&nPositions, sizeof(int), 1, file);

			if (nPositions > 0) {
				mType |= VERTEXT_POSITION;
				mPositions = new XMFLOAT3[nPositions];
				nReads = (UINT)::fread(mPositions, sizeof(XMFLOAT3), nPositions, file);

				VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, commandList, 
					mPositions, nPositions * sizeof(XMFLOAT3), VertexBufferUploader);
				/*
				m_pd3dPositionBuffer = ::CreateBufferResource(device, commandList, m_pxmf3Positions, sizeof(XMFLOAT3) * mVertexCnt, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dPositionUploadBuffer);

				m_d3dPositionBufferView.BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
				m_d3dPositionBufferView.StrideInBytes = sizeof(XMFLOAT3);
				m_d3dPositionBufferView.SizeInBytes = sizeof(XMFLOAT3) * mVertexCnt;
				*/
			}
		}
		else if (!strcmp(pstrToken, "<Colors>:")) {
			nReads = (UINT)::fread(&nColors, sizeof(int), 1, file);

			if (nColors > 0) {
				mType |= VERTEXT_COLOR;
				mColors = new XMFLOAT4[nColors];
				nReads = (UINT)::fread(mColors, sizeof(XMFLOAT4), nColors, file);
			}
		}
		else if (!strcmp(pstrToken, "<TextureCoords0>:")) {
			nReads = (UINT)::fread(&nTextureCoords, sizeof(int), 1, file);

			if (nTextureCoords > 0)
			{
				mType |= VERTEXT_TEXTURE_COORD0;
				mTextureCoords0 = new XMFLOAT2[nTextureCoords];
				nReads = (UINT)::fread(mTextureCoords0, sizeof(XMFLOAT2), nTextureCoords, file);
				/*
				m_pd3dTextureCoord0Buffer = ::CreateBufferResource(device, commandList, mTextureCoords0, sizeof(XMFLOAT2) * mVertexCnt, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dTextureCoord0UploadBuffer);

				m_d3dTextureCoord0BufferView.BufferLocation = m_pd3dTextureCoord0Buffer->GetGPUVirtualAddress();
				m_d3dTextureCoord0BufferView.StrideInBytes = sizeof(XMFLOAT2);
				m_d3dTextureCoord0BufferView.SizeInBytes = sizeof(XMFLOAT2) * mVertexCnt;
				*/
			}
		}
		else if (!strcmp(pstrToken, "<TextureCoords1>:")) {
			nReads = (UINT)::fread(&nTextureCoords, sizeof(int), 1, file);

			if (nTextureCoords > 0) {
				mType |= VERTEXT_TEXTURE_COORD1;
				mTextureCoords1 = new XMFLOAT2[nTextureCoords];
				nReads = (UINT)::fread(mTextureCoords1, sizeof(XMFLOAT2), nTextureCoords, file);

				/*
				m_pd3dTextureCoord1Buffer = ::CreateBufferResource(device, commandList, mTextureCoords1, sizeof(XMFLOAT2) * mVertexCnt, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dTextureCoord1UploadBuffer);

				m_d3dTextureCoord1BufferView.BufferLocation = m_pd3dTextureCoord1Buffer->GetGPUVirtualAddress();
				m_d3dTextureCoord1BufferView.StrideInBytes = sizeof(XMFLOAT2);
				m_d3dTextureCoord1BufferView.SizeInBytes = sizeof(XMFLOAT2) * mVertexCnt;
				*/
			}
		}
		else if (!strcmp(pstrToken, "<Normals>:")) {
			nReads = (UINT)::fread(&nNormals, sizeof(int), 1, file);

			if (nNormals > 0) {
				mType |= VERTEXT_NORMAL;
				mNormals = new XMFLOAT3[nNormals];
				nReads = (UINT)::fread(mNormals, sizeof(XMFLOAT3), nNormals, file);
				/*
				m_pd3dNormalBuffer = ::CreateBufferResource(device, commandList, mNormals, sizeof(XMFLOAT3) * mVertexCnt, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dNormalUploadBuffer);

				m_d3dNormalBufferView.BufferLocation = m_pd3dNormalBuffer->GetGPUVirtualAddress();
				m_d3dNormalBufferView.StrideInBytes = sizeof(XMFLOAT3);
				m_d3dNormalBufferView.SizeInBytes = sizeof(XMFLOAT3) * mVertexCnt;
				*/
			}
		}
		else if (!strcmp(pstrToken, "<Tangents>:")) {
			nReads = (UINT)::fread(&nTangents, sizeof(int), 1, file);

			if (nTangents > 0) {
				mType |= VERTEXT_TANGENT;
				mTangents = new XMFLOAT3[nTangents];
				nReads = (UINT)::fread(mTangents, sizeof(XMFLOAT3), nTangents, file);
				/*
				m_pd3dTangentBuffer = ::CreateBufferResource(device, commandList, mTangents, sizeof(XMFLOAT3) * mVertexCnt, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dTangentUploadBuffer);

				m_d3dTangentBufferView.BufferLocation = m_pd3dTangentBuffer->GetGPUVirtualAddress();
				m_d3dTangentBufferView.StrideInBytes = sizeof(XMFLOAT3);
				m_d3dTangentBufferView.SizeInBytes = sizeof(XMFLOAT3) * mVertexCnt;
				*/
			}
		}
		else if (!strcmp(pstrToken, "<BiTangents>:")) {
			nReads = (UINT)::fread(&nBiTangents, sizeof(int), 1, file);

			if (nBiTangents > 0) {
				mBiTangents = new XMFLOAT3[nBiTangents];
				nReads = (UINT)::fread(mBiTangents, sizeof(XMFLOAT3), nBiTangents, file);
				/*
				m_pd3dBiTangentBuffer = ::CreateBufferResource(device, commandList, mBiTangents, sizeof(XMFLOAT3) * mVertexCnt, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dBiTangentUploadBuffer);

				m_d3dBiTangentBufferView.BufferLocation = m_pd3dBiTangentBuffer->GetGPUVirtualAddress();
				m_d3dBiTangentBufferView.StrideInBytes = sizeof(XMFLOAT3);
				m_d3dBiTangentBufferView.SizeInBytes = sizeof(XMFLOAT3) * mVertexCnt;
				*/
			}
		}
		else if (!strcmp(pstrToken, "<Indices>:")) {
			nReads = (UINT)::fread(&nIndices, sizeof(int), 1, file);
			
			if (nIndices > 0) {
				mIndices = new UINT[nIndices];
				nReads = (UINT)::fread(mIndices, sizeof(UINT), nIndices, file);

				IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, commandList,
					mIndices, nIndices * sizeof(XMFLOAT3), VertexBufferUploader);
				
				ThrowIfFailed(D3DCreateBlob(sizeof(UINT) * nIndices, &IndexBufferCPU));
				CopyMemory(IndexBufferCPU->GetBufferPointer(), mIndices, sizeof(UINT) * nIndices);
				/*
				m_pd3dPositionBuffer = ::CreateBufferResource(device, commandList, m_pxmf3Positions, sizeof(XMFLOAT3) * mVertexCnt, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dPositionUploadBuffer);

				m_d3dPositionBufferView.BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
				m_d3dPositionBufferView.StrideInBytes = sizeof(XMFLOAT3);
				m_d3dPositionBufferView.SizeInBytes = sizeof(XMFLOAT3) * mVertexCnt;
				*/
			}
		}
		else if (!strcmp(pstrToken, "<SubMeshes>:")) {
			nReads = (UINT)::fread(&nSubMeshes, sizeof(int), 1, file);

			if (nSubMeshes > 0) {
				SubmeshGeometry subMesh;
				/*
				m_pnSubSetIndices = new int[m_nSubMeshes];
				m_ppnSubSetIndices = new UINT * [m_nSubMeshes];

				m_ppd3dSubSetIndexBuffers = new ID3D12Resource * [m_nSubMeshes];
				m_ppd3dSubSetIndexUploadBuffers = new ID3D12Resource * [m_nSubMeshes];
				m_pd3dSubSetIndexBufferViews = new D3D12_INDEX_BUFFER_VIEW[m_nSubMeshes];
				*/
				for (int i = 0; i < nSubMeshes; i++)
				{
					nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, file);
					nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, file);
					pstrToken[nStrLength] = '\0';

					if (!strcmp(pstrToken, "<SubMesh>:")) {
						nReads = (UINT)::fread(&subMesh.StartIndexLocation, sizeof(int), 1, file);
						nReads = (UINT)::fread(&subMesh.IndexCount, sizeof(int), 1, file);
						/*
						if (nIndex > 0) {
							UINT* subMeshIndices = new UINT[nIndex];
							nReads = (UINT)::fread(subMeshIndices, sizeof(UINT) * nIndex, 1, file);

							m_ppd3dSubSetIndexBuffers[i] = ::CreateBufferResource(device, commandList, m_ppnSubSetIndices[i], sizeof(UINT) * m_pnSubSetIndices[i], D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_ppd3dSubSetIndexUploadBuffers[i]);

							m_pd3dSubSetIndexBufferViews[i].BufferLocation = m_ppd3dSubSetIndexBuffers[i]->GetGPUVirtualAddress();
							m_pd3dSubSetIndexBufferViews[i].Format = DXGI_FORMAT_R32_UINT;
							m_pd3dSubSetIndexBufferViews[i].SizeInBytes = sizeof(UINT) * m_pnSubSetIndices[i];
							
						}
					*/
					}
				}
			}
		}
		else if (!strcmp(pstrToken, "</Mesh>"))
		{
			break;
		}
	}
}


