#pragma once

#pragma once
#include "d3dUtil.h"
#include <vector>
#include <queue>
#include <unordered_set>
#include <functional>

// ============================================================
// Grid 기반 A* Pathfinder
// ============================================================
// Fog 시스템과 동일한 Grid 해상도를 사용하여
// 정적/동적 충돌체를 고려한 경로 탐색 수행
// ============================================================

class Pathfinder
{
public:
    Pathfinder();
    ~Pathfinder();

    // 그리드 초기화 (Fog 시스템과 동일한 셀 크기 사용 권장)
    // mapWidth, mapLength: 월드 단위 맵 크기
    // cellSize: 셀 하나의 월드 단위 크기
    void Initialize(float mapWidth, float mapLength, float cellSize);

    // 정적 충돌체로 그리드 장애물 마킹
    // 
    void SetStaticObstacle(int gx, int gz);
    // colliders: 월드 공간 AABB 목록
    void BakeStaticObstacles(const std::vector<BoundingBox>& colliders);

    // 동적 장애물 임시 마킹 (매 경로 탐색 전 호출)
    // 탐색 후 자동 해제됨
    void SetDynamicObstacle(int gx, int gz);
    void ClearDynamicObstacles();

    // A* 경로 탐색
    // startWorld, goalWorld: 월드 좌표
    // unitExtents: 유닛 BB의 XZ Extents (크기 고려)
    // outPath: [out] 월드 좌표 waypoint 목록 (start → goal)
    // return: 경로를 찾았는지 여부
    bool FindPath(
        const DirectX::XMFLOAT3& startWorld,
        const DirectX::XMFLOAT3& goalWorld,
        const DirectX::XMFLOAT3& unitExtents,
        std::vector<DirectX::XMFLOAT3>& outPath);

    // 월드 좌표 ↔ 그리드 좌표 변환
    void WorldToGrid(float wx, float wz, int& gx, int& gz) const;
    void GridToWorld(int gx, int gz, float& wx, float& wz) const;

    // 그리드 정보 조회
    bool IsWalkable(int gx, int gz) const;
    int GetGridX() const { return mGridX; }
    int GetGridZ() const { return mGridZ; }
    float GetCellSize() const { return mCellSize; }



private:
    // 그리드 데이터
    int mGridX = 0;
    int mGridZ = 0;
    float mCellSize = 1.0f;
    float mMapWidth = 0.0f;
    float mMapLength = 0.0f;

    // 0 = walkable, 1 = static obstacle, 2 = dynamic obstacle (임시)
    std::vector<uint8_t> mGrid;

    // 동적 장애물로 마킹된 셀 목록 (ClearDynamicObstacles에서 복원)
    std::vector<int> mDynamicMarked;

    // 유닛 크기를 고려한 장애물 팽창 (Minkowski sum)
    bool IsWalkableForUnit(int gx, int gz, int expandCells) const;

    // A* 내부 구조
    struct Node
    {
        int gx, gz;
        float gCost;    // start까지의 실제 비용
        float fCost;    // gCost + heuristic

        bool operator>(const Node& other) const { return fCost > other.fCost; }
    };

    int Index(int gx, int gz) const { return gz * mGridX + gx; }
    bool InBounds(int gx, int gz) const { return gx >= 0 && gx < mGridX && gz >= 0 && gz < mGridZ; }

    // 휴리스틱: 옥타일 거리 (8방향 이동)
    float Heuristic(int ax, int az, int bx, int bz) const;

    // 경로 스무딩 (불필요한 waypoint 제거)
    void SmoothPath(std::vector<DirectX::XMFLOAT3>& path, int expandCells);

    // 두 그리드 셀 사이에 장애물이 없는지 (Bresenham line)
    bool HasLineOfSight(int x0, int z0, int x1, int z1, int expandCells) const;
};