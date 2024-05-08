#include "SkinnedMesh.h"

SkinnedMesh::~SkinnedMesh()
{
    Clear();
}

bool SkinnedMesh::LoadMesh(const std::string& Filename)
{
    Clear();

    /*
        aiProcess_JoinIdenticalVertices |        // ������ ������ ����, �ε��� ����ȭ
        aiProcess_ValidateDataStructure |        // �δ��� ����� ����
        aiProcess_ImproveCacheLocality |        // ��� ������ ĳ����ġ�� ����
        aiProcess_RemoveRedundantMaterials |    // �ߺ��� ���͸��� ����
        aiProcess_GenUVCoords |                    // ����, ������, ���� �� ��� ������ ������ UV�� ��ȯ
        aiProcess_TransformUVCoords |            // UV ��ȯ ó���� (�����ϸ�, ��ȯ...)
        aiProcess_FindInstances |                // �ν��Ͻ��� �Ž��� �˻��Ͽ� �ϳ��� �����Ϳ� ���� ������ ����
        aiProcess_LimitBoneWeights |            // ������ ���� ����ġ�� �ִ� 4���� ����
        aiProcess_OptimizeMeshes |                // ������ ��� ���� �Ž��� ����
        aiProcess_GenSmoothNormals |            // �ε巯�� �븻����(��������) ����
        aiProcess_SplitLargeMeshes |            // �Ŵ��� �ϳ��� �Ž��� �����Ž���� ��Ȱ(����)
        aiProcess_Triangulate |                    // 3�� �̻��� �𼭸��� ���� �ٰ��� ���� �ﰢ������ ����
        aiProcess_ConvertToLeftHanded |            // D3D�� �޼���ǥ��� ��ȯ
    */
    Assimp::Importer importer;
    const aiScene* pScene = importer.ReadFile(Filename.c_str(),
        aiProcess_JoinIdenticalVertices |
        aiProcess_ValidateDataStructure |
        aiProcess_Triangulate |
        aiProcess_LimitBoneWeights |
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

bool SkinnedMesh::LoadAnimation(const std::string& Filename, const string animationName)
{
    //Clear();

    Assimp::Importer importer;
    const aiScene* pScene = importer.ReadFile(Filename.c_str(),
        aiProcess_JoinIdenticalVertices |
        aiProcess_ValidateDataStructure |
        aiProcess_Triangulate |
        aiProcess_LimitBoneWeights |
        aiProcess_ConvertToLeftHanded);

    if (pScene) {
        InitAnimation(pScene, animationName);
        importer.FreeScene();
        return true;
    }
    else {
        return false;
    }

    return false;
}

void SkinnedMesh::SetOffsetMatrix(XMFLOAT3 axis1, float degree1)
{
    XMStoreFloat4x4(&mOffsetMatrix, XMMatrixRotationAxis(XMLoadFloat3(&axis1), XMConvertToRadians(degree1)));
}

void SkinnedMesh::SetOffsetMatrix(XMFLOAT3 axis1, float degree1, XMFLOAT3 axis2, float degree2)
{
    XMStoreFloat4x4(&mOffsetMatrix, XMMatrixRotationAxis(XMLoadFloat3(&axis1), XMConvertToRadians(degree1)) * XMMatrixRotationAxis(XMLoadFloat3(&axis2), XMConvertToRadians(degree2)));
}

void SkinnedMesh::GetBoneTransforms(float timeInSeconds, vector<XMFLOAT4X4>& transforms, string animationName)
{
    if (mAnimations.empty())
        return;

    XMMATRIX Identity = XMMatrixIdentity();
    
    float ticksPerSecond = mAnimations[animationName].tickPerSecond;
    float timeInTicks = timeInSeconds * ticksPerSecond;
    float animationTimeTicks = fmod(timeInTicks, mAnimations[animationName].duration);
    
    ReadBoneHierarchy(animationTimeTicks, rootNodeName, animationName, XMLoadFloat4x4(&mOffsetMatrix));
    transforms.resize(mBoneInfo.size());

    //mBoneInfo[GetBoneId("root")].FinalTransformation = Matrix4x4::Identity();

    for (int i = 0; i < mBoneInfo.size(); i++) {
        transforms[i] = mBoneInfo[i].FinalTransformation;
    }
}

void SkinnedMesh::GetBoneTransforms(float timeInSeconds, vector<XMFLOAT4X4>& transforms, const vector<string> animationNames, float animationWeight, bool durationInterpolation)
{
    if (mAnimations.empty() || animationNames.size() == 0)
        return;

    if (animationNames.size() == 1) {
        GetBoneTransforms(timeInSeconds, transforms, animationNames[0]);
        return;
    }

    XMMATRIX Identity = XMMatrixIdentity();

    float animationTimeTicks[2];
    float animationDuration = mAnimations[animationNames[1]].duration;

    if (durationInterpolation) {
        float dAnimationDuration = mAnimations[animationNames[1]].duration / mAnimations[animationNames[0]].duration;

        float ticksPerSecond = mAnimations[animationNames[0]].tickPerSecond;
        float timeInTicks = timeInSeconds * ticksPerSecond;
        animationTimeTicks[0] = fmod(timeInTicks, mAnimations[animationNames[0]].duration);
        animationTimeTicks[1] = animationTimeTicks[0] * dAnimationDuration;
    }
    else {
        for (int i = 0; i < 2; i++)
        {
            float ticksPerSecond = mAnimations[animationNames[i]].tickPerSecond;
            float timeInTicks = timeInSeconds * ticksPerSecond;
            animationTimeTicks[i] = fmod(timeInTicks, mAnimations[animationNames[i]].duration);
        }
    }
    

    ReadBoneHierarchy(animationTimeTicks, rootNodeName, animationNames, animationWeight, XMLoadFloat4x4(&mOffsetMatrix));
    transforms.resize(mBoneInfo.size());

    //mBoneInfo[GetBoneId("root")].FinalTransformation = Matrix4x4::Identity();

    for (int i = 0; i < mBoneInfo.size(); i++) {
        transforms[i] = mBoneInfo[i].FinalTransformation;
    }
}

void SkinnedMesh::CreateBlob(const vector<SkinnedVertex>& vertices, const vector<UINT>& indices)
{
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(SkinnedVertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(UINT);

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mVertexBufferCPU));
    CopyMemory(mVertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mIndexBufferCPU));
    CopyMemory(mIndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
}

void SkinnedMesh::UploadBuffer(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* commandList, vector<SkinnedVertex> vertices, vector<UINT> indices)
{
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(SkinnedVertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(UINT);

    mVertexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice, commandList,
        vertices.data(), vbByteSize, mVertexBufferUploader);

    mIndexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice, commandList,
        indices.data(), ibByteSize, mIndexBufferUploader);

    mVertexByteStride = sizeof(SkinnedVertex);
    mVertexBufferByteSize = vbByteSize;
    mIndexFormat = DXGI_FORMAT_R32_UINT;
    mIndexBufferByteSize = ibByteSize;
}

void SkinnedMesh::Clear()
{
}

bool SkinnedMesh::InitFromScene(const aiScene* pScene, const std::string& Filename)
{
    int numSubmeshes = pScene->mNumMeshes;
    //mSubmeshes.resize(numSubmeshes);
    //m_Materials.resize(pScene->mNumMaterials);

    rootNodeName = pScene->mRootNode->mName.C_Str();

    // ��� ������ �ε��� ���� ���ϱ�
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
    mBones.resize(NumVertices);

    InitAllMeshes(pScene);
    //InitAllAnimations(pScene);

    LoadNodeHierarchy(pScene->mRootNode);
    mBoneHierarchy.resize(NumBones());
    LoadBoneHierarchy(pScene->mRootNode);

    if (!InitMaterials(pScene, Filename)) {
        return false;
    }

    return true;
}

void SkinnedMesh::InitAllMeshes(const aiScene* pScene)
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

/*
void SkinnedMesh::InitAllAnimations(const aiScene* pScene)
{
    if (!pScene->HasAnimations())
        return;

    int numAnimations = pScene->mNumAnimations;

    for (unsigned int i = 0; i < numAnimations; i++)
    {
        aiAnimation* pAnimation = pScene->mAnimations[i]; 
        AnimationClip animationClip;
        animationClip.name = pAnimation->mName.C_Str();
        animationClip.duration = pAnimation->mDuration;
        animationClip.tickPerSecond = pAnimation->mTicksPerSecond != 0 ? pAnimation->mTicksPerSecond : 25.0f;

        int numChannels = pAnimation->mNumChannels;
        for (unsigned int j = 0; j < numChannels; j++)
        {
            aiNodeAnim* pChannel = pAnimation->mChannels[j];
            BoneAnimation boneAnimation;
            string boneName = pChannel->mNodeName.C_Str();

            // PositionKeyframe ä���
            int numPositionKeys = pChannel->mNumPositionKeys;
            boneAnimation.translation.resize(numPositionKeys);
            for (int k = 0; k < numPositionKeys; k++)
            {
                Keyframe<XMFLOAT3> translation;
                aiVectorKey positionKey = pChannel->mPositionKeys[k];
                translation.timePos = positionKey.mTime;
                translation.value = XMFLOAT3(positionKey.mValue.x, positionKey.mValue.y, positionKey.mValue.z);

                boneAnimation.translation[k] = translation;
            }

            // RotationKeyframe ä���
            int numRotationKeys = pChannel->mNumRotationKeys;
            boneAnimation.rotationQuat.resize(numRotationKeys);
            for (int k = 0; k < numRotationKeys; k++)
            {
                Keyframe<XMFLOAT4> rotationQuat;
                aiQuatKey rotateKey = pChannel->mRotationKeys[k];
                rotationQuat.timePos = rotateKey.mTime;
                rotationQuat.value = XMFLOAT4(rotateKey.mValue.x, rotateKey.mValue.y, rotateKey.mValue.z, rotateKey.mValue.w);

                boneAnimation.rotationQuat[k] = rotationQuat;
            }

            // ScaleKeyframe ä���
            int numScaleKeys = pChannel->mNumScalingKeys;
            boneAnimation.scale.resize(numScaleKeys);
            for (int k = 0; k < numScaleKeys; k++)
            {
                Keyframe<XMFLOAT3> scale;
                aiVectorKey scaleKey = pChannel->mScalingKeys[k];
                scale.timePos = scaleKey.mTime;
                scale.value = XMFLOAT3(scaleKey.mValue.x, scaleKey.mValue.y, scaleKey.mValue.z);
                boneAnimation.scale[k] = scale;
            }

            animationClip.boneAnimations[boneName] = boneAnimation;
        }

        mAnimations.push_back(animationClip);
    }
}
*/

void SkinnedMesh::InitAnimation(const aiScene* pScene, const string name)
{
    if (!pScene->HasAnimations())
        return;

    int numAnimations = pScene->mNumAnimations;

    aiAnimation* pAnimation = pScene->mAnimations[0];
    AnimationClip animationClip;
    animationClip.name = pAnimation->mName.C_Str();
    animationClip.duration = pAnimation->mDuration;
    animationClip.tickPerSecond = pAnimation->mTicksPerSecond != 0 ? pAnimation->mTicksPerSecond : 25.0f;

    int numChannels = pAnimation->mNumChannels;
    for (unsigned int j = 0; j < numChannels; j++)
    {
        aiNodeAnim* pChannel = pAnimation->mChannels[j];
        BoneAnimation boneAnimation;
        string boneName = pChannel->mNodeName.C_Str();

        // PositionKeyframe ä���
        int numPositionKeys = pChannel->mNumPositionKeys;
        boneAnimation.translation.resize(numPositionKeys);
        for (int k = 0; k < numPositionKeys; k++)
        {
            Keyframe<XMFLOAT3> translation;
            aiVectorKey positionKey = pChannel->mPositionKeys[k];
            translation.timePos = positionKey.mTime;
            translation.value = XMFLOAT3(positionKey.mValue.x, positionKey.mValue.y, positionKey.mValue.z);

            boneAnimation.translation[k] = translation;
        }

        // RotationKeyframe ä���
        int numRotationKeys = pChannel->mNumRotationKeys;
        boneAnimation.rotationQuat.resize(numRotationKeys);
        for (int k = 0; k < numRotationKeys; k++)
        {
            Keyframe<XMFLOAT4> rotationQuat;
            aiQuatKey rotateKey = pChannel->mRotationKeys[k];
            rotationQuat.timePos = rotateKey.mTime;
            rotationQuat.value = XMFLOAT4(rotateKey.mValue.x, rotateKey.mValue.y, rotateKey.mValue.z, rotateKey.mValue.w);

            boneAnimation.rotationQuat[k] = rotationQuat;
        }

        // ScaleKeyframe ä���
        int numScaleKeys = pChannel->mNumScalingKeys;
        boneAnimation.scale.resize(numScaleKeys);
        for (int k = 0; k < numScaleKeys; k++)
        {
            Keyframe<XMFLOAT3> scale;
            aiVectorKey scaleKey = pChannel->mScalingKeys[k];
            scale.timePos = scaleKey.mTime;
            scale.value = XMFLOAT3(scaleKey.mValue.x, scaleKey.mValue.y, scaleKey.mValue.z);
            boneAnimation.scale[k] = scale;
        }

        animationClip.boneAnimations[boneName] = boneAnimation;
    }

    mAnimations[name] = animationClip;
}

void SkinnedMesh::InitSingleMesh(int MeshIndex, const aiMesh* paiMesh, int baseVertex, int baseIndex)
{
    // ���� ���� ä���
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

    LoadMeshBones(MeshIndex, paiMesh);

    // �ε��� ���� ä���
    for (UINT i = 0; i < numFace; i++)
    {
        UINT base = baseIndex + 3 * i;
        mIndices[base] = paiMesh->mFaces[i].mIndices[0];
        mIndices[base + 1] = paiMesh->mFaces[i].mIndices[1];
        mIndices[base + 2] = paiMesh->mFaces[i].mIndices[2];
    }
}

bool SkinnedMesh::InitMaterials(const aiScene* pScene, const std::string& Filename)
{
    //string Dir = GetDirFromFilename(Filename);

    bool Ret = true;

    printf("Num materials: %d\n", pScene->mNumMaterials);

    // Initialize the materials
    for (unsigned int i = 0; i < pScene->mNumMaterials; i++) {
        const aiMaterial* pMaterial = pScene->mMaterials[i];

        //LoadTextures(Dir, pMaterial, i);

        LoadColors(pMaterial, i);
    }

    return Ret;
}

void SkinnedMesh::LoadTextures(const string& Dir, const aiMaterial* pMaterial, int index)
{
    LoadDiffuseTexture(Dir, pMaterial, index);
    LoadSpecularTexture(Dir, pMaterial, index);
}

void SkinnedMesh::LoadDiffuseTexture(const string& Dir, const aiMaterial* pMaterial, int index)
{
    /*
    m_Materials[index].pDiffuse = NULL;

    if (pMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString Path;

        if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &Path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
            string p(Path.data);

            if (p.substr(0, 2) == ".\\") {
                p = p.substr(2, p.size() - 2);
            }

            string FullPath = Dir + "/" + p;

            m_Materials[index].pDiffuse = new Texture(GL_TEXTURE_2D, FullPath.c_str());

            if (!m_Materials[index].pDiffuse->Load()) {
                printf("Error loading diffuse texture '%s'\n", FullPath.c_str());
                exit(0);
            }
            else {
                printf("Loaded diffuse texture '%s'\n", FullPath.c_str());
            }
        }
    }
    */
}

void SkinnedMesh::LoadSpecularTexture(const string& Dir, const aiMaterial* pMaterial, int index)
{
    /*
    m_Materials[index].pSpecularExponent = NULL;

    if (pMaterial->GetTextureCount(aiTextureType_SHININESS) > 0) {
        aiString Path;

        if (pMaterial->GetTexture(aiTextureType_SHININESS, 0, &Path, NULL, NULL, NULL, NULL, NULL) == AI_SUCCESS) {
            string p(Path.data);

            if (p == "C:\\\\") {
                p = "";
            }
            else if (p.substr(0, 2) == ".\\") {
                p = p.substr(2, p.size() - 2);
            }

            string FullPath = Dir + "/" + p;

            m_Materials[index].pSpecularExponent = new Texture(GL_TEXTURE_2D, FullPath.c_str());

            if (!m_Materials[index].pSpecularExponent->Load()) {
                printf("Error loading specular texture '%s'\n", FullPath.c_str());
                exit(0);
            }
            else {
                printf("Loaded specular texture '%s'\n", FullPath.c_str());
            }
        }
    }
    */
}

void SkinnedMesh::LoadColors(const aiMaterial* pMaterial, int index)
{
    /*
    aiColor3D AmbientColor(0.0f, 0.0f, 0.0f);
    Vector3f AllOnes(1.0f, 1.0f, 1.0f);

    int ShadingModel = 0;
    if (pMaterial->Get(AI_MATKEY_SHADING_MODEL, ShadingModel) == AI_SUCCESS) {
        printf("Shading model %d\n", ShadingModel);
    }

    if (pMaterial->Get(AI_MATKEY_COLOR_AMBIENT, AmbientColor) == AI_SUCCESS) {
        printf("Loaded ambient color [%f %f %f]\n", AmbientColor.r, AmbientColor.g, AmbientColor.b);
        m_Materials[index].AmbientColor.r = AmbientColor.r;
        m_Materials[index].AmbientColor.g = AmbientColor.g;
        m_Materials[index].AmbientColor.b = AmbientColor.b;
    }
    else {
        m_Materials[index].AmbientColor = AllOnes;
    }

    aiColor3D DiffuseColor(0.0f, 0.0f, 0.0f);

    if (pMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, DiffuseColor) == AI_SUCCESS) {
        printf("Loaded diffuse color [%f %f %f]\n", DiffuseColor.r, DiffuseColor.g, DiffuseColor.b);
        m_Materials[index].DiffuseColor.r = DiffuseColor.r;
        m_Materials[index].DiffuseColor.g = DiffuseColor.g;
        m_Materials[index].DiffuseColor.b = DiffuseColor.b;
    }

    aiColor3D SpecularColor(0.0f, 0.0f, 0.0f);

    if (pMaterial->Get(AI_MATKEY_COLOR_SPECULAR, SpecularColor) == AI_SUCCESS) {
        printf("Loaded specular color [%f %f %f]\n", SpecularColor.r, SpecularColor.g, SpecularColor.b);
        m_Materials[index].SpecularColor.r = SpecularColor.r;
        m_Materials[index].SpecularColor.g = SpecularColor.g;
        m_Materials[index].SpecularColor.b = SpecularColor.b;
    }
    */
}

void SkinnedMesh::LoadMeshBones(UINT meshID, const aiMesh* pMesh)
{
    for (int i = 0; i < pMesh->mNumBones; i++) {
        LoadSingleBone(meshID, pMesh->mBones[i]);
    }
}

void SkinnedMesh::LoadSingleBone(UINT meshID, const aiBone* pBone)
{
    int BoneId = GetBoneId(pBone);

    if (BoneId == mBoneInfo.size()) {
        XMFLOAT4X4 offsetMat;
        XMStoreFloat4x4(&offsetMat, XMMatrixTranspose(XMMATRIX(&pBone->mOffsetMatrix.a1)));
        BoneInfo boneInfo(offsetMat);
        mBoneInfo.push_back(boneInfo);
    }

    for (int i = 0; i < pBone->mNumWeights; i++) {
        const aiVertexWeight& vw = pBone->mWeights[i];
        int GlobalVertexID = mSubmeshes[meshID].baseVertex + pBone->mWeights[i].mVertexId;
        mBones[GlobalVertexID].AddBoneData(BoneId, vw.mWeight);
    }
}

int SkinnedMesh::GetBoneId(const aiBone* pBone)
{
    int boneIndex = 0;
    string boneName(pBone->mName.C_Str());

    if (mBoneNameToIndexMap.find(boneName) == mBoneNameToIndexMap.end()) {
        // ���ο� �� ����
        boneIndex = (int)mBoneNameToIndexMap.size();
        mBoneNameToIndexMap[boneName] = boneIndex;
    }
    else {
        boneIndex = mBoneNameToIndexMap[boneName];
    }

    return boneIndex;
}

int SkinnedMesh::GetBoneId(const string boneName)
{
    if (mBoneNameToIndexMap.find(boneName) == mBoneNameToIndexMap.end()) {
        return -1;
    }
    else {
        return mBoneNameToIndexMap[boneName];
    }
}

int SkinnedMesh::GetNodeId(const string nodeName)
{
    if (mNodeNameToIndexMap.find(nodeName) == mNodeNameToIndexMap.end()) {
        return -1;
    }
    else {
        return mNodeNameToIndexMap[nodeName];
    }
}

void SkinnedMesh::CalcInterpolatedScaling(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    // ������ ���ؼ� ��� 2���� ���� �ʿ��ϴ�.
    if (pNodeAnim->mNumScalingKeys == 1) {
        Out = pNodeAnim->mScalingKeys[0].mValue;
        return;
    }

    UINT ScalingIndex = FindScaling(AnimationTime, pNodeAnim);
    UINT NextScalingIndex = ScalingIndex + 1;
    assert(NextScalingIndex < pNodeAnim->mNumScalingKeys);
    float t1 = (float)pNodeAnim->mScalingKeys[ScalingIndex].mTime;
    float t2 = (float)pNodeAnim->mScalingKeys[NextScalingIndex].mTime;
    float DeltaTime = t2 - t1;
    float Factor = (AnimationTime - (float)t1) / DeltaTime;
    assert(Factor >= 0.0f && Factor <= 1.0f);
    const aiVector3D& Start = pNodeAnim->mScalingKeys[ScalingIndex].mValue;
    const aiVector3D& End = pNodeAnim->mScalingKeys[NextScalingIndex].mValue;
    aiVector3D Delta = End - Start;
    Out = Start + Factor * Delta;
}

void SkinnedMesh::CalcInterpolatedRotation(aiQuaternion& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    // we need at least two values to interpolate...
    if (pNodeAnim->mNumRotationKeys == 1) {
        Out = pNodeAnim->mRotationKeys[0].mValue;
        return;
    }

    UINT RotationIndex = FindRotation(AnimationTime, pNodeAnim);
    UINT NextRotationIndex = RotationIndex + 1;
    assert(NextRotationIndex < pNodeAnim->mNumRotationKeys);
    float t1 = (float)pNodeAnim->mRotationKeys[RotationIndex].mTime;
    float t2 = (float)pNodeAnim->mRotationKeys[NextRotationIndex].mTime;
    float DeltaTime = t2 - t1;
    float Factor = (AnimationTime - t1) / DeltaTime;
    assert(Factor >= 0.0f && Factor <= 1.0f);
    const aiQuaternion& StartRotationQ = pNodeAnim->mRotationKeys[RotationIndex].mValue;
    const aiQuaternion& EndRotationQ = pNodeAnim->mRotationKeys[NextRotationIndex].mValue;
    aiQuaternion::Interpolate(Out, StartRotationQ, EndRotationQ, Factor);
    Out.Normalize();
}

void SkinnedMesh::CalcInterpolatedPosition(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    // we need at least two values to interpolate...
    if (pNodeAnim->mNumPositionKeys == 1) {
        Out = pNodeAnim->mPositionKeys[0].mValue;
        return;
    }

    UINT PositionIndex = FindPosition(AnimationTime, pNodeAnim);
    UINT NextPositionIndex = PositionIndex + 1;
    assert(NextPositionIndex < pNodeAnim->mNumPositionKeys);
    float t1 = (float)pNodeAnim->mPositionKeys[PositionIndex].mTime;
    float t2 = (float)pNodeAnim->mPositionKeys[NextPositionIndex].mTime;
    float DeltaTime = t2 - t1;
    float Factor = (AnimationTime - t1) / DeltaTime;
    assert(Factor >= 0.0f && Factor <= 1.0f);
    const aiVector3D& Start = pNodeAnim->mPositionKeys[PositionIndex].mValue;
    const aiVector3D& End = pNodeAnim->mPositionKeys[NextPositionIndex].mValue;
    aiVector3D Delta = End - Start;
    Out = Start + Factor * Delta;
}

XMVECTOR SkinnedMesh::CalcInterpolatedScaling(const float AnimationTime, const BoneAnimation boneAnimation)
{
    // ������ ���ؼ� ��� 2���� ���� �ʿ��ϴ�.
    if (boneAnimation.scale.size() == 1) {
        return XMLoadFloat3(&boneAnimation.scale[0].value);
    }

    int scalingIndex = FindScaleIndex(AnimationTime, boneAnimation);
    int nextScalingIndex = scalingIndex + 1;
    assert(nextScalingIndex < boneAnimation.scale.size());

    float time1 = boneAnimation.scale[scalingIndex].timePos;
    float time2 = boneAnimation.scale[nextScalingIndex].timePos;
    float DeltaTime = time2 - time1;
    float Factor = (AnimationTime - time1) / DeltaTime;
    assert(Factor >= 0.0f && Factor <= 1.0f);

    XMVECTOR start = XMLoadFloat3(&boneAnimation.scale[scalingIndex].value);
    XMVECTOR end = XMLoadFloat3(&boneAnimation.scale[nextScalingIndex].value);
    XMVECTOR delta = end - start;

    return (start + (delta * Factor));
}

XMVECTOR SkinnedMesh::CalcInterpolatedRotation(const float AnimationTime, const BoneAnimation boneAnimation)
{
    // ������ ���ؼ� ��� 2���� ���� �ʿ��ϴ�.
    if (boneAnimation.rotationQuat.size() == 1) {
        return XMLoadFloat4(&boneAnimation.rotationQuat[0].value);
    }
    
    int rotationIndex = FindRotationIndex(AnimationTime, boneAnimation);
    int nextRotationIndex = rotationIndex + 1;
    assert(nextRotationIndex < boneAnimation.rotationQuat.size());

    float time1 = boneAnimation.rotationQuat[rotationIndex].timePos;
    float time2 = boneAnimation.rotationQuat[nextRotationIndex].timePos;
    float deltaTime = time2 - time1;
    float factor = (AnimationTime - time1) / deltaTime;
    assert(factor >= 0.0f && factor <= 1.0f);
    /*
    char buf[256];
    sprintf_s(buf, sizeof(buf), "%d, %f\n", rotationIndex, factor);
    OutputDebugStringA(buf);
    */
    XMVECTOR start = XMLoadFloat4(&boneAnimation.rotationQuat[rotationIndex].value);
    XMVECTOR end = XMLoadFloat4(&boneAnimation.rotationQuat[nextRotationIndex].value);

    XMVECTOR rotationQuat = XMQuaternionSlerp(start, end, factor);
    return rotationQuat;
}

XMVECTOR SkinnedMesh::CalcInterpolatedPosition(const float AnimationTime, const BoneAnimation boneAnimation)
{
    // ������ ���ؼ� ��� 2���� ���� �ʿ��ϴ�.
    if (boneAnimation.translation.size() == 1) {
        return XMLoadFloat3(&boneAnimation.translation[0].value);
    }

    int translationIndex = FindScaleIndex(AnimationTime, boneAnimation);
    int nextTranslationIndex = translationIndex + 1;
    assert(nextTranslationIndex < boneAnimation.translation.size());

    float time1 = boneAnimation.translation[translationIndex].timePos;
    float time2 = boneAnimation.translation[nextTranslationIndex].timePos;
    float DeltaTime = time2 - time1;
    float Factor = (AnimationTime - time1) / DeltaTime;
    assert(Factor >= 0.0f && Factor <= 1.0f);

    XMVECTOR start = XMLoadFloat3(&boneAnimation.translation[translationIndex].value);
    XMVECTOR end = XMLoadFloat3(&boneAnimation.translation[nextTranslationIndex].value);
    XMVECTOR delta = end - start;

    return (start + (delta * Factor));
}

int SkinnedMesh::FindScaling(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    assert(pNodeAnim->mNumScalingKeys > 0);

    for (UINT i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++) {
        float t = (float)pNodeAnim->mScalingKeys[i + 1].mTime;
        if (AnimationTime < t) {
            return i;
        }
    }

    return 0;
}

int SkinnedMesh::FindScaleIndex(float animationTime, BoneAnimation boneAnimation)
{
    int numScaleKeyframes = boneAnimation.scale.size();
    assert(numScaleKeyframes > 0);
    
    for (int i = 0; i < numScaleKeyframes - 1; i++)
    {
        float time = boneAnimation.scale[i + 1].timePos;
        if (animationTime < time)
            return i;
    }

    return 0;
}

int SkinnedMesh::FindRotation(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    assert(pNodeAnim->mNumRotationKeys > 0);

    for (UINT i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++) {
        float t = (float)pNodeAnim->mRotationKeys[i + 1].mTime;
        if (AnimationTime < t) {
            return i;
        }
    }

    return 0;
}

int SkinnedMesh::FindRotationIndex(float animationTime, BoneAnimation boneAnimation)
{
    int numRotationKeyframes = boneAnimation.rotationQuat.size();
    assert(numRotationKeyframes > 0);

    for (int i = 0; i < numRotationKeyframes - 1; i++)
    {
        float time = boneAnimation.rotationQuat[i + 1].timePos;
        if (animationTime < time)
            return i;
    }

    return 0;
}

int SkinnedMesh::FindPosition(float AnimationTime, const aiNodeAnim* pNodeAnim)
{
    for (UINT i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++) {
        float t = (float)pNodeAnim->mPositionKeys[i + 1].mTime;
        if (AnimationTime < t) {
            return i;
        }
    }

    return 0;
}

int SkinnedMesh::FindPositionIndex(float animationTime, BoneAnimation boneAnimation)
{
    int numPositionKeyframes = boneAnimation.translation.size();
    assert(numPositionKeyframes > 0);

    for (int i = 0; i < numPositionKeyframes - 1; i++)
    {
        float time = boneAnimation.translation[i + 1].timePos;
        if (animationTime < time)
            return i;
    }

    return 0;
}

const aiNodeAnim* SkinnedMesh::FindNodeAnim(const aiAnimation* pAnimation, const string& NodeName)
{
    for (int i = 0; i < pAnimation->mNumChannels; i++) {
        const aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];

        if (string(pNodeAnim->mNodeName.data) == NodeName) {
            return pNodeAnim;
        }
    }

    return nullptr;
}

void SkinnedMesh::LoadNodeHierarchy(const aiNode* pNode)
{
    string nodeName(pNode->mName.data);
    vector<string> children;
    // bone�� �ִ� ���� ����
    for (int i = 0; i < pNode->mNumChildren; i++)
    {
        children.push_back(pNode->mChildren[i]->mName.C_Str());
    }
    mNodeNameToIndexMap[nodeName] = mNodeHierarchy.size();
    mNodeHierarchy.push_back(make_pair(nodeName, children));
    // ��� �ڽ� ��忡 ���� ��� ȣ��
    for (int i = 0; i < pNode->mNumChildren; i++) {
        LoadNodeHierarchy(pNode->mChildren[i]);
    }
}

void SkinnedMesh::LoadBoneHierarchy(const aiNode* pNode)
{
    string nodeName(pNode->mName.data);

    // bone�� �ִ� ���� ����
    if (mBoneNameToIndexMap.find(nodeName) != mBoneNameToIndexMap.end()) {
        vector<int> children;
        LoadChildren(children, pNode);
        mBoneHierarchy[GetBoneId(nodeName)] = make_pair(nodeName, children);
    }

    // ��� �ڽ� ��忡 ���� ��� ȣ��
    for (int i = 0; i < pNode->mNumChildren; i++) {
        LoadBoneHierarchy(pNode->mChildren[i]);
    }
}

void SkinnedMesh::LoadChildren(vector<int>& children, const aiNode* pNode)
{
    for (int i = 0; i < pNode->mNumChildren; i++)
    {
        string childName = pNode->mChildren[i]->mName.C_Str();
        // �ڽ� ��尡 Bone ����ϰ�� �ڽ����� ����
        if (mBoneNameToIndexMap.find(childName) != mBoneNameToIndexMap.end())
            children.push_back(GetBoneId(childName));
        // �ڽ� ��尡 Bone ��尡 �ƴ� ��� ��� Ž��
        else {
            LoadChildren(children, pNode->mChildren[i]);
        }
    }
}

/*
void SkinnedMesh::ReadNodeHierarchy(float AnimationTimeTicks, const aiNode* pNode, const XMMATRIX& ParentTransform)
{
    int animationIndex = 0;
    string NodeName(pNode->mName.data);

    const aiAnimation* pAnimation = pScene->mAnimations[animationIndex];

    //XMMATRIX NodeTransformation = XMMatrixTranspose(XMMATRIX(&pNode->mTransformation.a1));
    XMMATRIX NodeTransformation = XMMatrixIdentity();

    const aiNodeAnim* pNodeAnim = FindNodeAnim(pAnimation, NodeName);

    // �ִϸ��̼� ������ �ִٸ�
    if (pNodeAnim) {
        // �ִϸ��̼� �ð��� �̿��� ��ȯ�� �����Ѵ�
        aiVector3D Scaling;
        CalcInterpolatedScaling(Scaling, AnimationTimeTicks, pNodeAnim);
        XMMATRIX ScalingM = DirectX::XMMatrixScaling(Scaling.x, Scaling.y, Scaling.z);

        aiQuaternion RotationQ;
        CalcInterpolatedRotation(RotationQ, AnimationTimeTicks, pNodeAnim);
        XMVECTOR quaternion = { RotationQ.x, RotationQ.y, RotationQ.z, RotationQ.w };
        XMMATRIX RotationM = DirectX::XMMatrixRotationQuaternion(quaternion);

        aiVector3D Translation;
        CalcInterpolatedPosition(Translation, AnimationTimeTicks, pNodeAnim);
        XMMATRIX TranslationM = DirectX::XMMatrixTranslation(Translation.x, Translation.y, Translation.z);

        // Combine the above transformations
        NodeTransformation = ScalingM * RotationM * TranslationM;
    }

    XMMATRIX GlobalTransformation = NodeTransformation * ParentTransform;

    // bone�� �ִ� ��忡 ���ؼ��� bone Transform�� ����
    if (mBoneNameToIndexMap.find(NodeName) != mBoneNameToIndexMap.end()) {
        int BoneIndex = mBoneNameToIndexMap[NodeName];
        XMMATRIX finalTransformation = XMLoadFloat4x4(&mBoneInfo[BoneIndex].OffsetMatrix) * GlobalTransformation * XMLoadFloat4x4(&m_GlobalInverseTransform);
        XMStoreFloat4x4(&mBoneInfo[BoneIndex].FinalTransformation, XMMatrixTranspose(finalTransformation));
    }

    // ��� �ڽ� ��忡 ���� ��� ȣ��
    for (int i = 0; i < pNode->mNumChildren; i++) {
        ReadNodeHierarchy(AnimationTimeTicks, pNode->mChildren[i], GlobalTransformation);
    }
}
*/
void SkinnedMesh::ReadBoneHierarchy(float AnimationTimeTicks, const int boneId, const string animationName, const XMMATRIX& ParentTransform)
{
    string boneName = mBoneHierarchy[boneId].first;
    BoneAnimation boneAnimation = mAnimations[animationName].boneAnimations[boneName];
    
    XMMATRIX NodeTransformation = XMMatrixIdentity();
    XMMATRIX GlobalTransformation = XMMatrixIdentity();

    if (boneId == GetBoneId(boneName)) {
        // �ִϸ��̼� �ð��� �̿��� ��ȯ�� �����Ѵ�
        XMVECTOR scaling = CalcInterpolatedScaling(AnimationTimeTicks, boneAnimation);
        XMVECTOR rotationQuat = CalcInterpolatedRotation(AnimationTimeTicks, boneAnimation);
        XMVECTOR translation = CalcInterpolatedPosition(AnimationTimeTicks, boneAnimation);

        // Combine the above transformations
        NodeTransformation = XMMatrixAffineTransformation(scaling, XMQuaternionIdentity(), rotationQuat, translation);
        /*
        int BoneIndex = mBoneNameToIndexMap[boneName];
        XMMATRIX finalTransformation = XMLoadFloat4x4(&mBoneInfo[BoneIndex].OffsetMatrix) * GlobalTransformation * XMLoadFloat4x4(&m_GlobalInverseTransform);
        XMStoreFloat4x4(&mBoneInfo[BoneIndex].FinalTransformation, XMMatrixTranspose(finalTransformation));
        */
    }
   
    GlobalTransformation = NodeTransformation * ParentTransform;

    if (mBoneNameToIndexMap.find(boneName) != mBoneNameToIndexMap.end()) {
        int BoneIndex = mBoneNameToIndexMap[boneName];
        XMMATRIX finalTransformation = XMLoadFloat4x4(&mBoneInfo[BoneIndex].OffsetMatrix) * GlobalTransformation;
        XMStoreFloat4x4(&mBoneInfo[BoneIndex].FinalTransformation, XMMatrixTranspose(finalTransformation));
    }

    // ��� �ڽ� ��忡 ���� ��� ȣ��
    int numChildren = mBoneHierarchy[boneId].second.size();
    for (int i = 0; i < numChildren; i++) {
        ReadBoneHierarchy(AnimationTimeTicks, mBoneHierarchy[boneId].second[i], animationName, GlobalTransformation);
    }
}

void SkinnedMesh::ReadBoneHierarchy(float AnimationTimeTicks, const string name, const string animationName, const XMMATRIX& ParentTransform)
{
    string nodeName = name;
    int nodeId = GetNodeId(nodeName);
    int numChildren = mNodeHierarchy[nodeId].second.size();

    // ��Ʈ ����� ������� ���� ���
    if (!mAnimations[animationName].enableRootMotion && nodeName == "root") {
        for (int i = 0; i < numChildren; i++)
            ReadBoneHierarchy(AnimationTimeTicks, mNodeHierarchy[nodeId].second[i], animationName, ParentTransform);
        return;
    }

    // �ִϸ��̼ǿ� ��ϵ��� ���� ����� ���
    if (mAnimations[animationName].boneAnimations.find(nodeName) == mAnimations[animationName].boneAnimations.end()) {
        // ĳ���Ϳ� ���� ���������� �ִϸ��̼ǿ� �ش� ���� ��ġ�� ������� �ʴ� ��� �θ� ��带 ���󰣴�.
        if (GetBoneId(nodeName) != -1) {
            int BoneIndex = mBoneNameToIndexMap[nodeName];
            XMMATRIX finalTransformation = XMLoadFloat4x4(&mBoneInfo[BoneIndex].OffsetMatrix) * ParentTransform;
            XMStoreFloat4x4(&mBoneInfo[BoneIndex].FinalTransformation, XMMatrixTranspose(finalTransformation));
        }
        for (int i = 0; i < numChildren; i++)
            ReadBoneHierarchy(AnimationTimeTicks, mNodeHierarchy[nodeId].second[i], animationName, ParentTransform);
        return;
    }

    XMMATRIX GlobalTransformation = XMMatrixIdentity();
    XMMATRIX NodeTransformation = XMMatrixIdentity();

    BoneAnimation boneAnimation = mAnimations[animationName].boneAnimations[nodeName];

    // �ִϸ��̼� �ð��� �̿��� ��ȯ�� �����Ѵ�
    XMVECTOR scaling = CalcInterpolatedScaling(AnimationTimeTicks, boneAnimation);
    XMVECTOR rotationQuat = CalcInterpolatedRotation(AnimationTimeTicks, boneAnimation);
    XMVECTOR translation = CalcInterpolatedPosition(AnimationTimeTicks, boneAnimation);

    // Combine the above transformations
    NodeTransformation = XMMatrixAffineTransformation(scaling, XMQuaternionIdentity(), rotationQuat, translation);

    GlobalTransformation = NodeTransformation * ParentTransform;

    // node�� bone�� ����Ǿ��ִٸ� finalTransformation�� �����Ѵ�.
    if (GetBoneId(nodeName) != -1) {
        int BoneIndex = mBoneNameToIndexMap[nodeName];
        XMMATRIX finalTransformation = XMLoadFloat4x4(&mBoneInfo[BoneIndex].OffsetMatrix) * GlobalTransformation;
        XMStoreFloat4x4(&mBoneInfo[BoneIndex].FinalTransformation, XMMatrixTranspose(finalTransformation));
    }

    // ��� �ڽ� ��忡 ���� ��� ȣ��
    for (int i = 0; i < numChildren; i++) {
        ReadBoneHierarchy(AnimationTimeTicks, mNodeHierarchy[nodeId].second[i], animationName, GlobalTransformation);
    }
}

void SkinnedMesh::ReadBoneHierarchy(float AnimationTimeTicks[2], const string name, const vector<string> animationNames, float animationWeight, const XMMATRIX& ParentTransform)
{
    string nodeName = name;
    int nodeId = GetNodeId(nodeName);
    int numChildren = mNodeHierarchy[nodeId].second.size();

    // ��Ʈ ����� ������� ���� ���
    if (mAnimations[animationNames[0]].enableRootMotion && nodeName == "root") {
        for (int i = 0; i < numChildren; i++)
            ReadBoneHierarchy(AnimationTimeTicks, mNodeHierarchy[nodeId].second[i], animationNames, animationWeight, ParentTransform);
        return;
    }

    // �ִϸ��̼ǿ� ��ϵ��� ���� ����� ���
    if (mAnimations[animationNames[0]].boneAnimations.find(nodeName) == mAnimations[animationNames[0]].boneAnimations.end()) {
        // ĳ���Ϳ� ���� ���������� �ִϸ��̼ǿ� �ش� ���� ��ġ�� ������� �ʴ� ��� �θ� ��带 ���󰣴�.
        if (GetBoneId(nodeName) != -1) {
            int BoneIndex = mBoneNameToIndexMap[nodeName];
            XMMATRIX finalTransformation = XMLoadFloat4x4(&mBoneInfo[BoneIndex].OffsetMatrix) * ParentTransform;
            XMStoreFloat4x4(&mBoneInfo[BoneIndex].FinalTransformation, XMMatrixTranspose(finalTransformation));
        }
        for (int i = 0; i < numChildren; i++)
            ReadBoneHierarchy(AnimationTimeTicks, mNodeHierarchy[nodeId].second[i], animationNames, animationWeight, ParentTransform);
        return;
    }

    XMMATRIX GlobalTransformation = XMMatrixIdentity();
    XMMATRIX NodeTransformation = XMMatrixIdentity();

    XMVECTOR scaling[2];
    XMVECTOR rotationQuat[2];
    XMVECTOR translation[2];
    for (int i = 0; i < 2; i++)
    {
        BoneAnimation boneAnimation = mAnimations[animationNames[i]].boneAnimations[nodeName];

        // �ִϸ��̼� �ð��� �̿��� ��ȯ�� �����Ѵ�
        scaling[i] = CalcInterpolatedScaling(AnimationTimeTicks[i], boneAnimation);
        rotationQuat[i] = CalcInterpolatedRotation(AnimationTimeTicks[i], boneAnimation);
        translation[i] = CalcInterpolatedPosition(AnimationTimeTicks[i], boneAnimation);
    }

    // 2���� �ִϸ��̼� ����
    XMVECTOR finalScaling = XMVectorLerp(scaling[0], scaling[1], animationWeight);
    XMVECTOR finalRotationQuat = XMQuaternionSlerp(rotationQuat[0], rotationQuat[1], animationWeight);
    XMVECTOR finalTranslation = XMVectorLerp(translation[0], translation[1], animationWeight);

    NodeTransformation = XMMatrixAffineTransformation(finalScaling, XMQuaternionIdentity(), finalRotationQuat, finalTranslation);

    GlobalTransformation = NodeTransformation * ParentTransform;

    // node�� bone�� ����Ǿ��ִٸ� finalTransformation�� �����Ѵ�.
    if (GetBoneId(nodeName) != -1) {
        int BoneIndex = mBoneNameToIndexMap[nodeName];
        XMMATRIX finalTransformation = XMLoadFloat4x4(&mBoneInfo[BoneIndex].OffsetMatrix) * GlobalTransformation;
        XMStoreFloat4x4(&mBoneInfo[BoneIndex].FinalTransformation, XMMatrixTranspose(finalTransformation));
    }

    // ��� �ڽ� ��忡 ���� ��� ȣ��
    for (int i = 0; i < numChildren; i++) {
        ReadBoneHierarchy(AnimationTimeTicks, mNodeHierarchy[nodeId].second[i], animationNames, animationWeight, GlobalTransformation);
    }
}