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

// �ִϸ��̼��� �� ������
template <typename TXMFLOAT>
struct Keyframe
{
    float timePos = 0.0f;

    TXMFLOAT value;
};

// Keyframe�� ����Ʈ
// �ð����� �̿��� 2���� keyframe�� ������ ��İ��� ����.
struct BoneAnimation
{   
    string boneName;

    vector<Keyframe<XMFLOAT3>> translation;
    vector<Keyframe<XMFLOAT3>> scale;
    vector<Keyframe<XMFLOAT4>> rotationQuat;
};

// �ϳ��� �ִϸ��̼��� ��Ÿ����. ("�޸���", "���" ��)
// ��� ���� ���� BoneAnimation�� ����Ʈ
struct AnimationClip
{
    string name;
    float tickPerSecond = 0.0f;
    float duration = 0.0f;

    std::vector<BoneAnimation> boneAnimations;
};

// �� ������ �� �ִϸ��̼ǿ� �󸶳� ������ �޴��� ����ġ�� ����
// ��� ������ �ִ� MAX_NUM_BONES_PER_VERTEX ���� ���� ������ �޴´�.
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
    vector<vector<int>> mBoneHierarchy;         // �θ�� �ڽ��� �ε����� ������ �ִ�. ��Ʈ�� 0
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
    int GetBoneId(const aiBone* pBone);     // ������ ���� ���� ���� ���ο� index�� �߰���.
    int GetBoneId(const string boneName);   // ������ ���� ���� ���� -1 ��ȯ
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

