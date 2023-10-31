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

GameObject* GameObject::LoadFrameHierarchyFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, GameObject* parent, FILE* file)
{
    char pstrToken[64] = { '\0' };

    BYTE nStrLength = 0;
    UINT nReads = 0;

    int nFrame = 0, nTextures = 0;

    GameObject* gameObject = nullptr;

    for (; ; )
    {
        nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, file);
        nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, file);
        pstrToken[nStrLength] = '\0';

        if (!strcmp(pstrToken, "<Frame>:")) {
            gameObject = new GameObject();

            nReads = (UINT)::fread(&nFrame, sizeof(int), 1, file);
            nReads = (UINT)::fread(&nTextures, sizeof(int), 1, file);

            char frameName[256];
            nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, file);
            nReads = (UINT)::fread(frameName, sizeof(char), nStrLength, file);
            frameName[nStrLength] = '\0';
            gameObject->SetName(frameName);
        }
        else if (!strcmp(pstrToken, "<Transform>:")) {
            XMFLOAT3 position, rotation, scale;
            XMFLOAT4 quaternion;
            nReads = (UINT)::fread(&position, sizeof(float), 3, file);
            nReads = (UINT)::fread(&rotation, sizeof(float), 3, file); //Euler Angle
            nReads = (UINT)::fread(&scale, sizeof(float), 3, file);
            nReads = (UINT)::fread(&quaternion, sizeof(float), 4, file); //Quaternion
        }
        else if (!strcmp(pstrToken, "<TransformMatrix>:")) {
            XMFLOAT4X4 transform;
            nReads = (UINT)::fread(&transform, sizeof(float), 16, file);
            gameObject->mRenderItem.World = transform;
        }
        else if (!strcmp(pstrToken, "<Mesh>:")) {
            /*
            CStandardMesh* pMesh = new CStandardMesh(pd3dDevice, pd3dCommandList);
            pMesh->LoadMeshFromFile(pd3dDevice, pd3dCommandList, file);
            pGameObject->SetMesh(pMesh);
            */
            MeshGeometry* meshGeometry = new MeshGeometry;
            meshGeometry->LoadMeshFromFile(device, commandList, file);
        }
        else if (!strcmp(pstrToken, "<Materials>:")) {
            //gameObject->LoadMaterialsFromFile(pd3dDevice, pd3dCommandList, pParent, file);
        }
        else if (!strcmp(pstrToken, "<Children>:")) {
            int nChilds = 0;
            nReads = (UINT)::fread(&nChilds, sizeof(int), 1, file);
            if (nChilds > 0) {
                for (int i = 0; i < nChilds; i++)
                {
                    GameObject* child = GameObject::LoadFrameHierarchyFromFile(device, commandList, gameObject, file);
                    if (child) {
                        gameObject->SetChild(child);
                    }   
#ifdef _WITH_DEBUG_FRAME_HIERARCHY
                    TCHAR pstrDebug[256] = { 0 };
                    _stprintf_s(pstrDebug, 256, _T("(Frame: %p) (Parent: %p)\n"), pChild, pGameObject);
                    OutputDebugString(pstrDebug);
#endif
                }
            }
        }
        else if (!strcmp(pstrToken, "</Frame>")) {
            break;
        }
    }

    return gameObject;
}

GameObject* GameObject::LoadGeometryFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, std::string fileName)
{
    FILE* file;
    ::fopen_s(&file, fileName.c_str(), "rb");
    if (!file) {
        std::string errorMsg = "Failed to open file. \n(" + fileName + ")";
        MessageBox(nullptr, (LPCWSTR)errorMsg.c_str(), L"HR Failed", MB_OK);
        return nullptr;
    }

    GameObject* gameObject = GameObject::LoadFrameHierarchyFromFile(device, commandList, nullptr, file);
    fclose(file);

    return gameObject;
}
