#include "GameObject.h"

GameObject::GameObject()
{
}

GameObject::~GameObject()
{
	if (mSibling) 
        delete mSibling;
	if (mChild) 
        delete mChild;
}

void GameObject::SetPosition(float x, float y, float z)
{
    mRenderItem.World._41 = x;
    mRenderItem.World._42 = y;
    mRenderItem.World._43 = z;
}

void GameObject::SetPosition(XMFLOAT3 position)
{
    SetPosition(position.x, position.y, position.z);
}

void GameObject::SetScale(float x, float y, float z)
{
    XMMATRIX mtxScale = XMMatrixScaling(x, y, z);
    mRenderItem.World = Matrix4x4::Multiply(mtxScale, mRenderItem.World);
}

void GameObject::SetChild(GameObject* child)
{
    if (child) {
        if (mChild) {
            child->mSibling = mChild->mSibling;
            mChild->mSibling = child;
        }
        else {
            mChild = child;
        }
        child->mParent = this;
    }
}

XMFLOAT3 GameObject::GetPosition()
{
    return XMFLOAT3(mRenderItem.World._41, mRenderItem.World._42, mRenderItem.World._43);
}

XMFLOAT3 GameObject::GetLook()
{
    return Vector3::Normalize(XMFLOAT3(mRenderItem.World._31, mRenderItem.World._32, mRenderItem.World._33));
}

XMFLOAT3 GameObject::GetUp()
{
    return Vector3::Normalize(XMFLOAT3(mRenderItem.World._21, mRenderItem.World._22, mRenderItem.World._23));
}

XMFLOAT3 GameObject::GetRight()
{
    return Vector3::Normalize(XMFLOAT3(mRenderItem.World._11, mRenderItem.World._12, mRenderItem.World._13));
}

void GameObject::MoveStrafe(float distance)
{
    SetPosition(Vector3::Add(GetPosition(), GetRight(), distance));
}

void GameObject::MoveUp(float distance)
{
    SetPosition(Vector3::Add(GetPosition(), GetUp(), distance));
}

void GameObject::MoveForward(float distance)
{
    SetPosition(Vector3::Add(GetPosition(), GetLook(), distance));
}

void GameObject::Rotate(float pitch, float yaw, float roll)
{
    XMMATRIX rotateMat = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(pitch), XMConvertToRadians(yaw), XMConvertToRadians(roll));
    mRenderItem.World = Matrix4x4::Multiply(rotateMat, mRenderItem.World);
}

void GameObject::Rotate(XMFLOAT3* axis, float angle)
{
    XMMATRIX rotateMat = XMMatrixRotationAxis(
        XMLoadFloat3(axis), XMConvertToRadians(angle));
    mRenderItem.World = Matrix4x4::Multiply(rotateMat, mRenderItem.World);
}

void GameObject::Rotate(XMFLOAT4* quaternion)
{
    XMMATRIX rotateMat = XMMatrixRotationQuaternion(XMLoadFloat4(quaternion));
    mRenderItem.World = Matrix4x4::Multiply(rotateMat, mRenderItem.World);
}

GameObject* GameObject::FindFrame(const std::string frameName)
{
    if (frameName == mName) 
        return(this);

    GameObject* frameObject = nullptr;
    if (mSibling) 
        if (frameObject = mSibling->FindFrame(frameName)) 
            return(frameObject);

    if (mChild) 
        if (frameObject = mChild->FindFrame(frameName)) 
            return(frameObject);

    return nullptr;
}

//GameObject* GameObject::LoadFrameHierarchyFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, GameObject* parent, FILE* file)
//{
//    char pstrToken[64] = { '\0' };
//
//    BYTE strLength = 0;
//    UINT nReads = 0;
//
//    int nFrame = 0, nTextures = 0;
//
//    GameObject* gameObject = nullptr;
//
//    for (; ; )
//    {
//        nReads = (UINT)::fread(&strLength, sizeof(BYTE), 1, file);
//        nReads = (UINT)::fread(pstrToken, sizeof(char), strLength, file);
//        pstrToken[strLength] = '\0';
//
//        if (!strcmp(pstrToken, "<Frame>:")) {
//            gameObject = new GameObject();
//
//            nReads = (UINT)::fread(&nFrame, sizeof(int), 1, file);
//            nReads = (UINT)::fread(&nTextures, sizeof(int), 1, file);
//
//            char frameName[256];
//            nReads = (UINT)::fread(&strLength, sizeof(BYTE), 1, file);
//            nReads = (UINT)::fread(frameName, sizeof(char), strLength, file);
//            frameName[strLength] = '\0';
//            gameObject->SetName(frameName);
//        }
//        else if (!strcmp(pstrToken, "<Transform>:")) {
//            XMFLOAT3 position, rotation, scale;
//            XMFLOAT4 quaternion;
//            nReads = (UINT)::fread(&position, sizeof(float), 3, file);
//            nReads = (UINT)::fread(&rotation, sizeof(float), 3, file); //Euler Angle
//            nReads = (UINT)::fread(&scale, sizeof(float), 3, file);
//            nReads = (UINT)::fread(&quaternion, sizeof(float), 4, file); //Quaternion
//        }
//        else if (!strcmp(pstrToken, "<TransformMatrix>:")) {
//            XMFLOAT4X4 transform;
//            nReads = (UINT)::fread(&transform, sizeof(float), 16, file);
//            if (gameObject != nullptr)
//                gameObject->mRenderItem.World = transform;
//        }
//        else if (!strcmp(pstrToken, "<Mesh>:")) {
//            MeshGeometry* meshGeometry = new MeshGeometry;
//            meshGeometry->LoadMeshFromFile(device, commandList, file);
//            gameObject->mRenderItem.Geo = meshGeometry;
//            break;
//        }
//        else if (!strcmp(pstrToken, "<Materials>:")) {
//            gameObject->LoadMaterialsFromFile(device, commandList, parent, file);
//        }
//        else if (!strcmp(pstrToken, "<Children>:")) {
//            /*
//            int nChilds = 0;
//            nReads = (UINT)::fread(&nChilds, sizeof(int), 1, file);
//            if (nChilds > 0) {
//                for (int i = 0; i < nChilds; i++)
//                {
//                    GameObject* child = GameObject::LoadFrameHierarchyFromFile(device, commandList, gameObject, file);
//                    if (child) {
//                        gameObject->SetChild(child);
//                    }
//                }
//            }
//            */
//        }
//        else if (!strcmp(pstrToken, "</Frame>")) {
//            break;
//        }
//    }
//
//    return gameObject;
//}
//
//GameObject* GameObject::LoadGeometryFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, std::string fileName)
//{
//    FILE* file;
//    ::fopen_s(&file, fileName.c_str(), "rb");
//    if (!file) {
//        std::string errorMsg = "Failed to open file. \n(" + fileName + ")";
//        MessageBox(nullptr, (LPCWSTR)errorMsg.c_str(), L"HR Failed", MB_OK);
//        return nullptr;
//    }
//
//    GameObject* gameObject = GameObject::LoadFrameHierarchyFromFile(device, commandList, nullptr, file);
//    fclose(file);
//
//    return gameObject;
//}
//
//void GameObject::LoadMaterialsFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, GameObject* parent, FILE* file)
//{
//    char pstrToken[64] = { '\0' };
//
//    int materialCnt = 0;
//    BYTE strLength = 0;
//
//    UINT nReads = (UINT)::fread(&materialCnt, sizeof(int), 1, file);
//
//    //m_ppMaterials = new CMaterial * [m_nMaterials];
//    //for (int i = 0; i < m_nMaterials; i++) m_ppMaterials[i] = NULL;
//
//    //CMaterial* pMaterial = NULL;
//
//    for (; ; )
//    {
//        nReads = (UINT)::fread(&strLength, sizeof(BYTE), 1, file);
//        nReads = (UINT)::fread(pstrToken, sizeof(char), strLength, file);
//        pstrToken[strLength] = '\0';
//
//        if (!strcmp(pstrToken, "<Material>:"))
//        {
//            nReads = (UINT)::fread(&materialCnt, sizeof(int), 1, file);
//
//            //pMaterial = new CMaterial(7); //0:Albedo, 1:Specular, 2:Metallic, 3:Normal, 4:Emission, 5:DetailAlbedo, 6:DetailNormal
//            /*
//            UINT nMeshType = GetMeshType();
//            if (nMeshType & VERTEXT_NORMAL_TEXTURE)
//                pMaterial->SetStandardShader();
//            SetMaterial(nMaterial, pMaterial); //m_ppMaterials[nMaterial] = pMaterial;
//            */
//        }
//        else if (!strcmp(pstrToken, "<AlbedoColor>:"))
//        {
//            //nReads = (UINT)::fread(&(pMaterial->m_xmf4AlbedoColor), sizeof(float), 4, file);
//            nReads = (UINT)::fseek(file, sizeof(XMFLOAT4), SEEK_CUR);
//        }
//        else if (!strcmp(pstrToken, "<EmissiveColor>:"))
//        {
//            //nReads = (UINT)::fread(&(pMaterial->m_xmf4EmissiveColor), sizeof(float), 4, file);
//            nReads = (UINT)::fseek(file, sizeof(XMFLOAT4), SEEK_CUR);
//        }
//        else if (!strcmp(pstrToken, "<SpecularColor>:"))
//        {
//            //nReads = (UINT)::fread(&(pMaterial->m_xmf4SpecularColor), sizeof(float), 4, file);
//            nReads = (UINT)::fseek(file, sizeof(XMFLOAT4), SEEK_CUR);
//        }
//        else if (!strcmp(pstrToken, "<Glossiness>:"))
//        {
//            //nReads = (UINT)::fread(&(pMaterial->m_fGlossiness), sizeof(float), 1, file);
//            nReads = (UINT)::fseek(file, sizeof(float), SEEK_CUR);
//        }
//        else if (!strcmp(pstrToken, "<Smoothness>:"))
//        {
//            //nReads = (UINT)::fread(&(pMaterial->m_fSmoothness), sizeof(float), 1, file);
//            nReads = (UINT)::fseek(file, sizeof(float), SEEK_CUR);
//        }
//        else if (!strcmp(pstrToken, "<Metallic>:"))
//        {
//            //nReads = (UINT)::fread(&(pMaterial->m_fSpecularHighlight), sizeof(float), 1, file);
//            nReads = (UINT)::fseek(file, sizeof(float), SEEK_CUR);
//        }
//        else if (!strcmp(pstrToken, "<SpecularHighlight>:"))
//        {
//            //nReads = (UINT)::fread(&(pMaterial->m_fMetallic), sizeof(float), 1, file);
//            nReads = (UINT)::fseek(file, sizeof(float), SEEK_CUR);
//        }
//        else if (!strcmp(pstrToken, "<GlossyReflection>:"))
//        {
//            //nReads = (UINT)::fread(&(pMaterial->m_fGlossyReflection), sizeof(float), 1, file);
//            nReads = (UINT)::fseek(file, sizeof(float), SEEK_CUR);
//        }
//        else if (!strcmp(pstrToken, "<AlbedoMap>:"))
//        {
//            //pMaterial->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_ALBEDO_MAP, 3, pMaterial->m_ppstrTextureNames[0], &(pMaterial->m_ppTextures[0]), pParent, file);
//        }
//        else if (!strcmp(pstrToken, "<SpecularMap>:"))
//        {
//            //m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_SPECULAR_MAP, 4, pMaterial->m_ppstrTextureNames[1], &(pMaterial->m_ppTextures[1]), pParent, file);
//        }
//        else if (!strcmp(pstrToken, "<NormalMap>:"))
//        {
//            //m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_NORMAL_MAP, 5, pMaterial->m_ppstrTextureNames[2], &(pMaterial->m_ppTextures[2]), pParent, file);
//        }
//        else if (!strcmp(pstrToken, "<MetallicMap>:"))
//        {
//            //m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_METALLIC_MAP, 6, pMaterial->m_ppstrTextureNames[3], &(pMaterial->m_ppTextures[3]), pParent, file);
//        }
//        else if (!strcmp(pstrToken, "<EmissionMap>:"))
//        {
//            //m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_EMISSION_MAP, 7, pMaterial->m_ppstrTextureNames[4], &(pMaterial->m_ppTextures[4]), pParent, file);
//        }
//        else if (!strcmp(pstrToken, "<DetailAlbedoMap>:"))
//        {
//            //m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_DETAIL_ALBEDO_MAP, 8, pMaterial->m_ppstrTextureNames[5], &(pMaterial->m_ppTextures[5]), pParent, file);
//        }
//        else if (!strcmp(pstrToken, "<DetailNormalMap>:"))
//        {
//            //m_ppMaterials[nMaterial]->LoadTextureFromFile(pd3dDevice, pd3dCommandList, MATERIAL_DETAIL_NORMAL_MAP, 9, pMaterial->m_ppstrTextureNames[6], &(pMaterial->m_ppTextures[6]), pParent, file);
//        }
//        else if (!strcmp(pstrToken, "</Materials>"))
//        {
//            break;
//        }
//    }
//}
