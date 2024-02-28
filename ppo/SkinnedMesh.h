#pragma once

#define MAX_NUM_BONES_PER_VERTEX 4

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp\cimport.h>
#include "d3dUtil.h"
#include <map>

using namespace DirectX;
using namespace std;

// 애니메이션의 한 프레임
template <typename TXMFLOAT>
struct Keyframe
{
    float timePos = 0.0f;

    TXMFLOAT value;
};

// Keyframe의 리스트
// 시간값을 이용해 2개의 keyframe을 보간한 행렬값을 구함.
struct BoneAnimation
{   
    string boneName;

    vector<Keyframe<XMFLOAT3>> translation;
    vector<Keyframe<XMFLOAT3>> scale;
    vector<Keyframe<XMFLOAT4>> rotationQuat;
};

// 하나의 애니메이션을 나타낸다. ("달리기", "대기" 등)
// 모든 뼈에 대한 BoneAnimation의 리스트
struct AnimationClip
{
    string name;
    float tickPerSecond = 0.0f;
    float duration = 0.0f;

    std::vector<BoneAnimation> boneAnimations;
};

// 각 정점이 뼈 애니메이션에 얼마나 영향을 받는지 가중치를 저장
// 모든 정점은 최대 MAX_NUM_BONES_PER_VERTEX 개의 뼈에 영향을 받는다.
struct VertexBoneData
{
    int BoneIDs[MAX_NUM_BONES_PER_VERTEX] = { 0 };
    float Weights[MAX_NUM_BONES_PER_VERTEX] = { 0.0f };

    VertexBoneData() {}

    void AddBoneData(int BoneID, float Weight)
    {
        for (int i = 0; i < MAX_NUM_BONES_PER_VERTEX; i++) {
            if (Weights[i] == 0.0) {
                BoneIDs[i] = BoneID;
                Weights[i] = Weight;
                return;
            }
        }

        //assert(0);
    }
};

struct BoneInfo
{
    XMFLOAT4X4 OffsetMatrix;
    XMFLOAT4X4 FinalTransformation;

    BoneInfo(const XMFLOAT4X4& Offset)
    {
        OffsetMatrix = Offset;
        FinalTransformation = XMFLOAT4X4();
    }
};

struct BasicMeshEntry {
    BasicMeshEntry()
    {
        NumIndices = 0;
        BaseVertex = 0;
        BaseIndex = 0;
        MaterialIndex = 0;
    }

    unsigned int NumIndices;
    unsigned int BaseVertex;
    unsigned int BaseIndex;
    unsigned int MaterialIndex;
};

class SkinnedMesh
{
public:
    SkinnedMesh() {};
    ~SkinnedMesh();

    bool LoadMesh(const std::string& Filename);

    int NumBones() const
    {
        return (int)mBoneNameToIndexMap.size();
    }

    //const Material& GetMaterial();

    void GetBoneTransforms(float animationTimeSec, vector<XMFLOAT4X4>& transforms, int animationIndex);

    vector<BasicMeshEntry> mMeshes;
    vector<Material> mMaterials;

    vector<XMFLOAT3> mPositions;
    vector<XMFLOAT3> mNormals;
    vector<XMFLOAT2> mTexCoords;
    vector<unsigned int> mIndices;
    vector<VertexBoneData> mBones;

    map<string, int> mBoneNameToIndexMap;
    vector<vector<int>> mBoneHierarchy;         // 부모는 자식의 인덱스를 가지고 있다. 루트는 0
    vector<BoneInfo> mBoneInfo;

    vector<AnimationClip> mAnimations;
private:
    void Clear();

    bool InitFromScene(const aiScene* pScene, const std::string& Filename);

    void InitAllMeshes(const aiScene* pScene);
    void InitAllAnimations(const aiScene* pScene);
    void InitSingleMesh(int MeshIndex, const aiMesh* paiMesh);
    bool InitMaterials(const aiScene* pScene, const std::string& Filename);

    void LoadTextures(const string& Dir, const aiMaterial* pMaterial, int index);
    void LoadDiffuseTexture(const string& Dir, const aiMaterial* pMaterial, int index);
    void LoadSpecularTexture(const string& Dir, const aiMaterial* pMaterial, int index);
    void LoadColors(const aiMaterial* pMaterial, int index);

    void LoadMeshBones(int MeshIndex, const aiMesh* pMesh);
    void LoadSingleBone(int MeshIndex, const aiBone* pBone);
    void LoadBoneHierarchy(const aiNode* pNode);
    int GetBoneId(const aiBone* pBone);     // 기존에 없는 값이 오면 새로운 index를 추가함.
    int GetBoneId(const string boneName);   // 기존에 없는 값이 오면 -1 반환
    //string GetBoneName(const int boneId);

    void CalcInterpolatedScaling(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim);
    void CalcInterpolatedRotation(aiQuaternion& Out, float AnimationTime, const aiNodeAnim* pNodeAnim);
    void CalcInterpolatedPosition(aiVector3D& Out, float AnimationTime, const aiNodeAnim* pNodeAnim);

    int FindScaling(float AnimationTime, const aiNodeAnim* pNodeAnim);
    int FindScaleIndex(float AnimationTime, BoneAnimation boneAnimation);
    int FindRotation(float AnimationTime, const aiNodeAnim* pNodeAnim);

    XMVECTOR CalcInterpolatedScaling(const float animationTime, const BoneAnimation boneAnimation);
    XMVECTOR CalcInterpolatedRotation(const float animationTime, const BoneAnimation boneAnimation);
    XMVECTOR CalcInterpolatedPosition(const float animationTime, const BoneAnimation boneAnimation);

    int FindRotationIndex(float AnimationTime, BoneAnimation boneAnimation);
    int FindPosition(float AnimationTime, const aiNodeAnim* pNodeAnim);
    int FindPositionIndex(float AnimationTime, BoneAnimation boneAnimation);
    const aiNodeAnim* FindNodeAnim(const aiAnimation* pAnimation, const string& NodeName);

    //void ReadNodeHierarchy(float AnimationTimeTicks, const aiNode* pNode, const XMMATRIX& ParentTransform);
    void ReadBoneHierarchy(float AnimationTimeTicks, const int boneId, const int animationId, const XMMATRIX& ParentTransform);
    
    XMFLOAT4X4 m_GlobalInverseTransform;
};

