#include <iostream>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <map>

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace std;

bool operator<(const XMFLOAT3& lhs, const XMFLOAT3& rhs) {
    if (lhs.x != rhs.x) return lhs.x < rhs.x;
    if (lhs.y != rhs.y) return lhs.y < rhs.y;
    return lhs.z < rhs.z;
}

vector<XMFLOAT3> vertices = {
    XMFLOAT3(-1.0f, -1.0f, -1.0f),
    XMFLOAT3(-1.0f, +1.0f, -1.0f),
    XMFLOAT3(+1.0f, +1.0f, -1.0f),
    XMFLOAT3(+1.0f, -1.0f, -1.0f),
    XMFLOAT3(-1.0f, -1.0f, +1.0f),
    XMFLOAT3(-1.0f, +1.0f, +1.0f),
    XMFLOAT3(+1.0f, +1.0f, +1.0f),
    XMFLOAT3(+1.0f, -1.0f, +1.0f)
};

vector<UINT> indices = {
    // 앞면
    0, 1, 2,
    0, 2, 3,

    // 뒷면
    4, 6, 5,
    4, 7, 6,

    // 왼쪽 면
    4, 5, 1,
    4, 1, 0,

    // 오른쪽 면
    3, 2, 6,
    3, 6, 7,

    // 윗면
    1, 5, 6,
    1, 6, 2,

    // 아랫면
    4, 0, 3,
    4, 3, 7
};

XMFLOAT4 p = { 0.0f, 1.0f ,0.0f, 0.0f };    // xz 평면

struct NewVertex
{
    NewVertex(XMFLOAT3 v, UINT i1, UINT i2) {
        vertex = v;
        index[0] = i1;
        index[1] = i2;
    }
    bool operator==(const NewVertex& nv) const;
    bool operator==(const XMFLOAT3& nv) const;

    XMFLOAT3 vertex;
    UINT index[2]; // space
};

bool NewVertex::operator==(const NewVertex& nv) const {
    return (nv.vertex.x == this->vertex.x && nv.vertex.y == this->vertex.y && nv.vertex.z == this->vertex.z);
}
bool NewVertex::operator==(const XMFLOAT3& nv) const {
    return (nv.x == this->vertex.x && nv.y == this->vertex.y && nv.z == this->vertex.z);
}

int main()
{
    vector<XMFLOAT3> separatedVertices[2]; // half space, oppoiste half space
    vector<UINT> separatedIndices[2]; // half space, oppoiste half space
    vector<NewVertex> middleVertices;
    XMVECTOR plane = XMLoadFloat4(&p);

    int numVertices = vertices.size(), numIndices = indices.size();
    vector<int> newIndices[2]; // half space, oppoiste half space
    vector<int> vertexPos;
    newIndices[0].resize(numVertices, -1);  // half space
    newIndices[1].resize(numVertices, -1);  // oppoiste half space
    vertexPos.resize(numVertices, 0);   // 0 = 절단면 위에 존재, 1 = half space, -1 = oppoiste half space

    // 정점 분리
    for (int i = 0; i < numVertices; i++)
    {
        XMFLOAT3 vertex = vertices[i];

        float dp = vertex.x * p.x + vertex.y * p.y + vertex.z * p.z + p.w;
        if (dp > 0) { // half space
            newIndices[0][i] = separatedVertices[0].size();
            separatedVertices[0].push_back(vertex);
            vertexPos[i] = 1;
        }
        else if (dp < 0) { // oppoiste half space
            newIndices[1][i] = separatedVertices[1].size();
            separatedVertices[1].push_back(vertex);
            vertexPos[i] = -1;
        }
        else {  // 절단면 위에 존재
            newIndices[0][i] = separatedVertices[0].size();
            newIndices[1][i] = separatedVertices[1].size();
            middleVertices.push_back(NewVertex(vertex, newIndices[0][i], newIndices[1][i]));
            separatedVertices[0].push_back(vertex);
            separatedVertices[1].push_back(vertex);
            vertexPos[i] = 0;
        }
    }

    int numFace = numIndices / 3;
    for (int i = 0; i < numFace; i++)
    {
        UINT index1 = indices[i * 3];
        UINT index2 = indices[i * 3 + 1];
        UINT index3 = indices[i * 3 + 2];

        int vertexPos1 = vertexPos[index1];
        int vertexPos2 = vertexPos[index2];
        int vertexPos3 = vertexPos[index3];

        // 3점이 같은 공간에 존재
        if (vertexPos1 == vertexPos2 && vertexPos2 == vertexPos3) {
            if (vertexPos1 > 0) {  // half space
                separatedIndices[0].push_back(newIndices[0][index1]);
                separatedIndices[0].push_back(newIndices[0][index2]);
                separatedIndices[0].push_back(newIndices[0][index3]);
            }
            else if (vertexPos1 < 0) {  // oppoiste half space
                separatedIndices[1].push_back(newIndices[1][index1]);
                separatedIndices[1].push_back(newIndices[1][index2]);
                separatedIndices[1].push_back(newIndices[1][index3]);
            }
            else {
                for (int space = 0; space < 2; space++)
                {
                    separatedIndices[space].push_back(newIndices[space][index1]);
                    separatedIndices[space].push_back(newIndices[space][index2]);
                    separatedIndices[space].push_back(newIndices[space][index3]);
                }
            }
        }
        // 2점이 같은 공간에 존재
        else if (vertexPos1 == vertexPos2 || vertexPos1 == vertexPos3 || vertexPos2 == vertexPos3) {
            // 한점이라도 평면 위에 존재하는 경우 면 절단이 일어나지 않는다.
            if (vertexPos1 == 0 || vertexPos2 == 0 || vertexPos3 == 0) {
                if (vertexPos1 > 0 || vertexPos2 > 0 || vertexPos3 > 0) {
                    separatedIndices[0].push_back(newIndices[0][index1]);
                    separatedIndices[0].push_back(newIndices[0][index2]);
                    separatedIndices[0].push_back(newIndices[0][index3]);
                }
                else {
                    separatedIndices[1].push_back(newIndices[1][index1]);
                    separatedIndices[1].push_back(newIndices[1][index2]);
                    separatedIndices[1].push_back(newIndices[1][index3]);
                }
            }
            // 모든 점이 평면 위에 존재하지 않으며 한 점이 다른 두점과 서로 다른 공간에 존재
            else {
                int sameIndex[2], otherIndex;
                (vertexPos1 == vertexPos2) ?
                    (otherIndex = index3, sameIndex[0] = index1, sameIndex[1] = index2) :
                    (vertexPos1 == vertexPos3) ?
                    (otherIndex = index2, sameIndex[0] = index3, sameIndex[1] = index1) : // otherIndex = index2 일때 나머지 중 더 작은 인덱스가 sameIndex2가 된다.
                    (otherIndex = index1, sameIndex[0] = index2, sameIndex[1] = index3);

                XMFLOAT3 newVertex[2];
                for (int vi = 0; vi < 2; vi++)
                    XMStoreFloat3(&newVertex[vi], XMPlaneIntersectLine(plane, XMLoadFloat3(&vertices[otherIndex]), XMLoadFloat3(&vertices[sameIndex[vi]])));

                UINT newVertexIndices[2][2]; // space, vertexId
                for (int vi = 0; vi < 2; vi++)
                {
                    auto it = find(middleVertices.begin(), middleVertices.end(), newVertex[vi]);
                    // 새로운 정점이 추가되있지 않았다면? 정점을 추가하고 인덱스를 부여한다.
                    if (it == middleVertices.end()) {
                        for (int space = 0; space < 2; space++)
                        {
                            newVertexIndices[space][vi] = separatedVertices[space].size();
                            separatedVertices[space].push_back(newVertex[vi]);
                        }
                        //--
                        middleVertices.push_back(NewVertex(newVertex[vi], newVertexIndices[0][vi], newVertexIndices[1][vi]));
                    }
                    // 새로운 정점이 이미 추가되어 있다면? 해당 정점의 인덱스만 가져온다.
                    else {
                        for (int space = 0; space < 2; space++)
                            newVertexIndices[space][vi] = middleVertices[it - middleVertices.begin()].index[space];
                    }
                }

                int space = vertexPos[otherIndex] > 0 ? 0 : 1;  // ohter vertex 가 half space에 있을겨우 space = 0
                int otherSpace = 1 - space;

                separatedIndices[space].push_back(newVertexIndices[space][0]); // newVertex1
                separatedIndices[space].push_back(newVertexIndices[space][1]); // newVertex2
                separatedIndices[space].push_back(newIndices[space][otherIndex]); // ohterVertex

                separatedIndices[otherSpace].push_back(newVertexIndices[otherSpace][1]);
                separatedIndices[otherSpace].push_back(newVertexIndices[otherSpace][0]);
                separatedIndices[otherSpace].push_back(newIndices[otherSpace][sameIndex[0]]);

                separatedIndices[otherSpace].push_back(newIndices[otherSpace][sameIndex[0]]);
                separatedIndices[otherSpace].push_back(newIndices[otherSpace][sameIndex[1]]);
                separatedIndices[otherSpace].push_back(newVertexIndices[otherSpace][1]);
            }
        }
        // 3점이 다른 공간에 존재
        else {
            int index_1, index_2, middleIndex;
            (vertexPos1 == 0) ?
                middleIndex = index1, index_1 = index2, index_2 = index3 :
                (vertexPos2 == 0) ?
                middleIndex = index2, index_1 = index3, index_2 = index1 : // otherIndex = index2 일때 작은 인덱스가 index_2가 된다.
                middleIndex = index3, index_1 = index1, index_2 = index2;

            XMFLOAT3 newVertex;
            XMStoreFloat3(&newVertex, XMPlaneIntersectLine(plane, XMLoadFloat3(&vertices[index_1]), XMLoadFloat3(&vertices[index_2])));

            UINT newVertexIndices[2]; // space
            auto it = find(middleVertices.begin(), middleVertices.end(), newVertex);
            //새로운 정점이 추가되있지 않았다면? 정점을 추가하고 인덱스를 부여한다.
            if (it == middleVertices.end()) {
                for (int space = 0; space < 2; space++)
                {
                    newVertexIndices[space] = separatedVertices[space].size();
                    separatedVertices[space].push_back(newVertex);
                }
                middleVertices.push_back(NewVertex(newVertex, newVertexIndices[0], newVertexIndices[1]));
            }
            // 새로운 정점이 이미 추가되어 있다면? 해당 정점의 인덱스만 가져온다.
            else {
                for (int space = 0; space < 2; space++)
                    newVertexIndices[space] = middleVertices[it - middleVertices.begin()].index[space];
            }

            int space = vertexPos[index_1] > 0 ? 0 : 1;  // ohter vertex 가 half space에 있을겨우 space = 0
            int otherSpace = 1 - space;

            separatedIndices[space].push_back(newVertexIndices[space]); // newVertex
            separatedIndices[space].push_back(newIndices[space][middleIndex]); // middleIndex
            separatedIndices[space].push_back(newIndices[space][index_1]); // index_1

            separatedIndices[otherSpace].push_back(newVertexIndices[otherSpace]); // newVertex
            separatedIndices[otherSpace].push_back(newIndices[otherSpace][middleIndex]); // middleIndex
            separatedIndices[otherSpace].push_back(newIndices[otherSpace][index_2]); // index_2
        }
    }

    for (int i = 0; i < separatedVertices[0].size(); i++)
    {
        cout << "half space vertex: (" << separatedVertices[0][i].x << ", " << separatedVertices[0][i].y << ", " << separatedVertices[0][i].z << ")\n";
    }

    for (int i = 0; i < separatedIndices[0].size() / 3; i++)
    {
        cout << "half space index: (" << separatedIndices[0][i] << ", " << separatedIndices[0][i + 1] << ", " << separatedIndices[0][i + 2] << ")\n";
    }
    /*
    for (int i = 0; i < separatedVertices[1].size(); i++)
        cout << "oppoiste half space: (" << separatedVertices[1][i].x << ", " << separatedVertices[1][i].y << ", " << separatedVertices[1][i].x << ")\n";
    */
    return 0;
}
