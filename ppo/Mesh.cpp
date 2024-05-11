#include "Mesh.h"

Mesh::Mesh()
{
}

Mesh::~Mesh()
{
}

bool Mesh::LoadMesh(const std::string& Filename)
{
	Clear();

	/*
		aiProcess_JoinIdenticalVertices |        // 동일한 꼭지점 결합, 인덱싱 최적화
		aiProcess_ValidateDataStructure |        // 로더의 출력을 검증
		aiProcess_ImproveCacheLocality |        // 출력 정점의 캐쉬위치를 개선
		aiProcess_RemoveRedundantMaterials |    // 중복된 매터리얼 제거
		aiProcess_GenUVCoords |                    // 구형, 원통형, 상자 및 평면 매핑을 적절한 UV로 변환
		aiProcess_TransformUVCoords |            // UV 변환 처리기 (스케일링, 변환...)
		aiProcess_FindInstances |                // 인스턴스된 매쉬를 검색하여 하나의 마스터에 대한 참조로 제거
		aiProcess_LimitBoneWeights |            // 정점당 뼈의 가중치를 최대 4개로 제한
		aiProcess_OptimizeMeshes |                // 가능한 경우 작은 매쉬를 조인
		aiProcess_GenSmoothNormals |            // 부드러운 노말벡터(법선벡터) 생성
		aiProcess_SplitLargeMeshes |            // 거대한 하나의 매쉬를 하위매쉬들로 분활(나눔)
		aiProcess_Triangulate |                    // 3개 이상의 모서리를 가진 다각형 면을 삼각형으로 나눔
		aiProcess_ConvertToLeftHanded |            // D3D의 왼손좌표계로 변환
	*/
	Assimp::Importer importer;
	const aiScene* pScene = importer.ReadFile(Filename.c_str(),
		aiProcess_JoinIdenticalVertices |
		aiProcess_ValidateDataStructure |
		aiProcess_Triangulate |
		aiProcess_ConvertToLeftHanded);

	if (pScene) {
		bool be = InitFromScene(pScene, Filename);
		importer.FreeScene();
		return be;
	}
	else {
		return false;
	}

	return false;
}

void Mesh::Clear()
{
}

bool Mesh::InitFromScene(const aiScene* pScene, const std::string& Filename)
{
	int numSubmeshes = pScene->mNumMeshes;
	//mSubmeshes.resize(numSubmeshes);
	//m_Materials.resize(pScene->mNumMaterials);

	mRootNodeName = pScene->mRootNode->mName.C_Str();

	// 모든 정점과 인덱스 개수 구하기
	unsigned int NumVertices = 0;
	unsigned int NumIndices = 0;

	for (int i = 0; i < numSubmeshes; i++)
	{
		Submesh submesh;

		submesh.name = pScene->mMeshes[i]->mName.C_Str();

		submesh.materialIndex = pScene->mMeshes[i]->mMaterialIndex;
		submesh.numIndices = pScene->mMeshes[i]->mNumFaces * 3;
		submesh.baseVertex = NumVertices;
		submesh.baseIndex = NumIndices;

		NumVertices += pScene->mMeshes[i]->mNumVertices;
		NumIndices += submesh.numIndices;

		mSubmeshes.push_back(submesh);
	}

	mPositions.resize(NumVertices);
	mNormals.resize(NumVertices);
	mTexCoords.resize(NumVertices);
	mIndices.resize(NumIndices);

	InitAllMeshes(pScene);
	//InitAllAnimations(pScene);

	/*
	if (!InitMaterials(pScene, Filename)) {
		return false;
	}
	*/
	return true;
}

void Mesh::InitAllMeshes(const aiScene* pScene)
{
	int baseVertex = 0;
	int baseIndex = 0;

	for (unsigned int i = 0; i < mSubmeshes.size(); i++)
	{
		const aiMesh* paiMesh = pScene->mMeshes[i];
		InitSingleMesh(i, paiMesh, baseVertex, baseIndex);
		baseVertex += pScene->mMeshes[i]->mNumVertices;
		baseIndex += pScene->mMeshes[i]->mNumFaces * 3;
	}
}

void Mesh::InitSingleMesh(int MeshIndex, const aiMesh* paiMesh, int baseVertex, int baseIndex)
{
	// 정점 정보 채우기
	int numVertices = paiMesh->mNumVertices;
	int numFace = paiMesh->mNumFaces;

	for (UINT i = 0; i < numVertices; i++)
	{
		UINT base = baseVertex + i;
		mPositions[base] = XMFLOAT3(paiMesh->mVertices[i].x, paiMesh->mVertices[i].y, paiMesh->mVertices[i].z);

		if (paiMesh->mNormals)
			mNormals[base] = XMFLOAT3(paiMesh->mNormals[i].x, paiMesh->mNormals[i].y, paiMesh->mNormals[i].z);
		else
			mNormals[base] = XMFLOAT3(0.0f, 1.0f, 0.0f);

		if (paiMesh->HasTextureCoords(0))
			mTexCoords[base] = XMFLOAT2(paiMesh->mTextureCoords[0][i].x, paiMesh->mTextureCoords[0][i].y);
		else
			mTexCoords[base] = XMFLOAT2(0.0f, 0.0f);
	}

	// 인덱스 정보 채우기
	for (UINT i = 0; i < numFace; i++)
	{
		UINT base = baseIndex + 3 * i;
		mIndices[base] = paiMesh->mFaces[i].mIndices[0];
		mIndices[base + 1] = paiMesh->mFaces[i].mIndices[1];
		mIndices[base + 2] = paiMesh->mFaces[i].mIndices[2];
	}
}

Submesh Mesh::GetSubmesh(string name)
{
	for (int i = 0; i < mSubmeshes.size(); i++)
	{
		if (mSubmeshes[i].name == name)
			return mSubmeshes[i];
	}

	return Submesh();
}

void Mesh::AddSubmesh(const string name, UINT numIndices, UINT baseVertex, UINT baseIndex, UINT materialIndex)
{
	Submesh submesh;
	
	submesh.name = name;
	submesh.numIndices = numIndices;
	submesh.baseVertex = baseVertex;
	submesh.baseIndex = baseIndex;
	submesh.materialIndex = materialIndex;
	
	mSubmeshes.push_back(submesh);
}

void Mesh::CreateBlob(const vector<Vertex>& vertices, const vector<UINT>& indices)
{
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(UINT);

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mVertexBufferCPU));
	CopyMemory(mVertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mIndexBufferCPU));
	CopyMemory(mIndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
}

void Mesh::UploadBuffer(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* commandList,
	vector<Vertex> vertices, vector<UINT> indices)
{
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(UINT);

	mVertexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice, commandList, 
		vertices.data(), vbByteSize, mVertexBufferUploader);

	mIndexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice, commandList, 
		indices.data(), ibByteSize, mIndexBufferUploader);

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

void Mesh::SetOffsetMatrix(XMFLOAT3 axis1, float degree1)
{
	XMStoreFloat4x4(&mOffsetMatrix, XMMatrixRotationAxis(XMLoadFloat3(&axis1), XMConvertToRadians(degree1)));
}

void Mesh::SetOffsetMatrix(XMFLOAT3 axis1, float degree1, XMFLOAT3 axis2, float degree2)
{
	XMStoreFloat4x4(&mOffsetMatrix, XMMatrixRotationAxis(XMLoadFloat3(&axis1), XMConvertToRadians(degree1)) * XMMatrixRotationAxis(XMLoadFloat3(&axis2), XMConvertToRadians(degree2)));
}