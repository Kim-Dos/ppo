#include "Pathfinder.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>

Pathfinder::Pathfinder() {}
Pathfinder::~Pathfinder() {}


// ============================================================
// 초기화
// ============================================================
void Pathfinder::Initialize(float mapWidth, float mapLength, float cellSize)
{
    mMapWidth = mapWidth;
    mMapLength = mapLength;
    mCellSize = cellSize;

    mGridX = (int)(mapWidth / cellSize);
    mGridZ = (int)(mapLength / cellSize);

    mGrid.assign(mGridX * mGridZ, 0); // 모두 walkable
}


// ============================================================
// 정적 충돌체 → 그리드에 bake
// ============================================================
void Pathfinder::BakeStaticObstacles(const std::vector<BoundingBox>& colliders)
{
    // 먼저 모든 셀을 walkable로 초기화
    std::fill(mGrid.begin(), mGrid.end(), 0);

    for (const auto& box : colliders)
    {
        // AABB의 월드 범위 → 그리드 범위
        float minX = box.Center.x - box.Extents.x;
        float maxX = box.Center.x + box.Extents.x;
        float minZ = box.Center.z - box.Extents.z;
        float maxZ = box.Center.z + box.Extents.z;

        int gMinX, gMinZ, gMaxX, gMaxZ;
        WorldToGrid(minX, minZ, gMinX, gMinZ);
        WorldToGrid(maxX, maxZ, gMaxX, gMaxZ);

        // min/max 정렬 (WorldToGrid의 Z축 방향에 따라 뒤집힐 수 있음)
        if (gMinX > gMaxX) std::swap(gMinX, gMaxX);
        if (gMinZ > gMaxZ) std::swap(gMinZ, gMaxZ);

        // 약간 여유 추가
        gMinX = max(0, gMinX - 1);
        gMinZ = max(0, gMinZ - 1);
        gMaxX = min(mGridX - 1, gMaxX + 1);
        gMaxZ = min(mGridZ - 1, gMaxZ + 1);

        for (int gz = gMinZ; gz <= gMaxZ; ++gz)
        {
            for (int gx = gMinX; gx <= gMaxX; ++gx)
            {
                // 셀의 월드 중심이 실제로 AABB와 겹치는지 정밀 체크
                float wx, wz;
                GridToWorld(gx, gz, wx, wz);

                // 셀의 AABB
                BoundingBox cellBB;
                cellBB.Center = DirectX::XMFLOAT3(wx, box.Center.y, wz);
                cellBB.Extents = DirectX::XMFLOAT3(mCellSize * 0.5f, box.Extents.y, mCellSize * 0.5f);

                if (cellBB.Intersects(box))
                {
                    mGrid[Index(gx, gz)] = 1; // static obstacle
                }
            }
        }
    }
}


// ============================================================
// 동적 장애물 마킹
// ============================================================
void Pathfinder::SetDynamicObstacle(int gx, int gz)
{
    if (!InBounds(gx, gz)) return;
    int idx = Index(gx, gz);
    if (mGrid[idx] == 0) // 정적 장애물이 아닌 셀만
    {
        mGrid[idx] = 2;
        mDynamicMarked.push_back(idx);
    }
}

void Pathfinder::ClearDynamicObstacles()
{
    for (int idx : mDynamicMarked)
    {
        if (mGrid[idx] == 2)
            mGrid[idx] = 0;
    }
    mDynamicMarked.clear();
}


// ============================================================
// 좌표 변환 (Fog의 WorldToFog과 동일한 매핑)
// ============================================================
void Pathfinder::WorldToGrid(float wx, float wz, int& gx, int& gz) const
{
    float nx = (wx + mMapWidth * 0.5f) / mMapWidth;
    float nz = (mMapLength - (wz + mMapLength * 0.5f)) / mMapLength;

    gx = (int)(nx * mGridX);
    gz = (int)(nz * mGridZ);

    gx = std::clamp(gx, 0, mGridX - 1);
    gz = std::clamp(gz, 0, mGridZ - 1);
}

void Pathfinder::GridToWorld(int gx, int gz, float& wx, float& wz) const
{
    float nx = (gx + 0.5f) / mGridX;
    float nz = (gz + 0.5f) / mGridZ;

    wx = nx * mMapWidth - mMapWidth * 0.5f;
    wz = mMapLength - nz * mMapLength - mMapLength * 0.5f;
}


// ============================================================
// 걸을 수 있는지 체크
// ============================================================
bool Pathfinder::IsWalkable(int gx, int gz) const
{
    if (!InBounds(gx, gz)) return false;
    return mGrid[Index(gx, gz)] == 0;
}

bool Pathfinder::IsWalkableForUnit(int gx, int gz, int expandCells) const
{
    // 유닛 크기만큼 주변 셀도 walkable이어야 함 (Minkowski 팽창)
    for (int dz = -expandCells; dz <= expandCells; ++dz)
    {
        for (int dx = -expandCells; dx <= expandCells; ++dx)
        {
            int nx = gx + dx;
            int nz = gz + dz;
            if (!InBounds(nx, nz)) return false;
            if (mGrid[Index(nx, nz)] != 0) return false;
        }
    }
    return true;
}


// ============================================================
// 휴리스틱: 옥타일 거리 (8방향 이동)
// ============================================================
float Pathfinder::Heuristic(int ax, int az, int bx, int bz) const
{
    int dx = std::abs(ax - bx);
    int dz = std::abs(az - bz);
    // 직선 1.0, 대각선 1.414
    return 1.0f * max(dx, dz) + 0.414f * min(dx, dz);
}


// ============================================================
// A* 경로 탐색
// ============================================================
bool Pathfinder::FindPath(
    const DirectX::XMFLOAT3& startWorld,
    const DirectX::XMFLOAT3& goalWorld,
    const DirectX::XMFLOAT3& unitExtents,
    std::vector<DirectX::XMFLOAT3>& outPath)
{
    outPath.clear();

    int startGX, startGZ, goalGX, goalGZ;
    WorldToGrid(startWorld.x, startWorld.z, startGX, startGZ);
    WorldToGrid(goalWorld.x, goalWorld.z, goalGX, goalGZ);

    // 유닛 크기에 따른 팽창 셀 수
    float maxExtent = max(unitExtents.x, unitExtents.z);
    int expandCells = (int)(maxExtent / mCellSize);

    // 목표가 갈 수 없는 곳이면 가장 가까운 walkable 셀을 찾음
    if (!IsWalkableForUnit(goalGX, goalGZ, expandCells))
    {
        bool found = false;
        for (int r = 1; r <= 20; ++r)
        {
            for (int dz = -r; dz <= r && !found; ++dz)
            {
                for (int dx = -r; dx <= r && !found; ++dx)
                {
                    if (std::abs(dx) != r && std::abs(dz) != r) continue; // 테두리만
                    int nx = goalGX + dx;
                    int nz = goalGZ + dz;
                    if (IsWalkableForUnit(nx, nz, expandCells))
                    {
                        goalGX = nx;
                        goalGZ = nz;
                        found = true;
                    }
                }
            }
            if (found) break;
        }
        if (!found) return false; // 근처에 갈 수 있는 곳이 없음
    }

    // 시작점도 걸을 수 없으면 실패
    if (!IsWalkableForUnit(startGX, startGZ, expandCells))
    {
        // 시작 셀이 장애물 안에 있을 수 있으므로 expand 0으로 재시도
        expandCells = 0;
        if (!IsWalkable(startGX, startGZ))
            return false;
    }

    // 같은 셀이면 직접 이동
    if (startGX == goalGX && startGZ == goalGZ)
    {
        outPath.push_back(goalWorld);
        return true;
    }

    // ----- A* -----
    const int MAX_SEARCH = 10000; // 탐색 상한 (성능 보호)

    // 8방향 이동 (dx, dz, cost)
    static const int DX[8] = { 1, -1, 0, 0, 1, -1, 1, -1 };
    static const int DZ[8] = { 0, 0, 1, -1, 1, 1, -1, -1 };
    static const float COST[8] = { 1.f, 1.f, 1.f, 1.f, 1.414f, 1.414f, 1.414f, 1.414f };

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openSet;
    std::vector<float> gScore(mGridX * mGridZ, 1e18f);
    std::vector<int> cameFrom(mGridX * mGridZ, -1);
    std::vector<bool> closed(mGridX * mGridZ, false);

    int startIdx = Index(startGX, startGZ);
    int goalIdx = Index(goalGX, goalGZ);

    gScore[startIdx] = 0.0f;
    openSet.push({ startGX, startGZ, 0.0f, Heuristic(startGX, startGZ, goalGX, goalGZ) });

    int searched = 0;
    bool pathFound = false;

    while (!openSet.empty() && searched < MAX_SEARCH)
    {
        Node current = openSet.top();
        openSet.pop();

        int curIdx = Index(current.gx, current.gz);

        if (closed[curIdx])
            continue;
        closed[curIdx] = true;
        ++searched;

        if (current.gx == goalGX && current.gz == goalGZ)
        {
            pathFound = true;
            break;
        }

        for (int dir = 0; dir < 8; ++dir)
        {
            int nx = current.gx + DX[dir];
            int nz = current.gz + DZ[dir];

            if (!IsWalkableForUnit(nx, nz, expandCells))
                continue;

            // 대각선 이동 시 인접 직선 셀도 walkable이어야 함 (코너 커팅 방지)
            if (dir >= 4)
            {
                if (!IsWalkableForUnit(current.gx + DX[dir], current.gz, expandCells) ||
                    !IsWalkableForUnit(current.gx, current.gz + DZ[dir], expandCells))
                    continue;
            }

            int nIdx = Index(nx, nz);
            if (closed[nIdx]) continue;

            float tentativeG = gScore[curIdx] + COST[dir];

            if (tentativeG < gScore[nIdx])
            {
                gScore[nIdx] = tentativeG;
                cameFrom[nIdx] = curIdx;

                float h = Heuristic(nx, nz, goalGX, goalGZ);
                openSet.push({ nx, nz, tentativeG, tentativeG + h });
            }
        }
    }

    if (!pathFound)
        return false;

    // ----- 경로 역추적 -----
    std::vector<std::pair<int, int>> gridPath;
    int cur = goalIdx;
    while (cur != -1)
    {
        int gz = cur / mGridX;
        int gx = cur % mGridX;
        gridPath.push_back({ gx, gz });
        cur = cameFrom[cur];
    }
    std::reverse(gridPath.begin(), gridPath.end());

    // ----- 그리드 좌표 → 월드 좌표 -----
    for (auto& [gx, gz] : gridPath)
    {
        float wx, wz;
        GridToWorld(gx, gz, wx, wz);
        outPath.push_back(DirectX::XMFLOAT3(wx, 0.0f, wz));
        // Y는 나중에 Terrain.GetHeight로 보정
    }

    // 마지막 waypoint를 정확한 목표 위치로 교체
    if (!outPath.empty())
    {
        outPath.back().x = goalWorld.x;
        outPath.back().z = goalWorld.z;
    }

    // ----- 경로 스무딩 (불필요한 중간 waypoint 제거) -----
    SmoothPath(outPath, expandCells);

    return true;
}


// ============================================================
// 경로 스무딩: Line of Sight가 있는 waypoint 건너뛰기
// ============================================================
void Pathfinder::SmoothPath(std::vector<DirectX::XMFLOAT3>& path, int expandCells)
{
    if (path.size() <= 2) return;

    std::vector<DirectX::XMFLOAT3> smoothed;
    smoothed.push_back(path.front());

    size_t current = 0;

    while (current < path.size() - 1)
    {
        size_t farthest = current + 1;

        // 가장 먼 LOS 가능한 waypoint를 찾음
        for (size_t test = current + 2; test < path.size(); ++test)
        {
            int x0, z0, x1, z1;
            WorldToGrid(path[current].x, path[current].z, x0, z0);
            WorldToGrid(path[test].x, path[test].z, x1, z1);

            if (HasLineOfSight(x0, z0, x1, z1, expandCells))
            {
                farthest = test;
            }
            else
            {
                break; // LOS 끊기면 중단
            }
        }

        smoothed.push_back(path[farthest]);
        current = farthest;
    }

    path = smoothed;
}


// ============================================================
// Bresenham Line: 두 그리드 셀 사이 장애물 검사
// ============================================================
bool Pathfinder::HasLineOfSight(int x0, int z0, int x1, int z1, int expandCells) const
{
    int dx = std::abs(x1 - x0);
    int dz = std::abs(z1 - z0);
    int sx = (x0 < x1) ? 1 : -1;
    int sz = (z0 < z1) ? 1 : -1;
    int err = dx - dz;

    while (true)
    {
        if (!IsWalkableForUnit(x0, z0, expandCells))
            return false;

        if (x0 == x1 && z0 == z1)
            break;

        int e2 = 2 * err;
        if (e2 > -dz) { err -= dz; x0 += sx; }
        if (e2 < dx) { err += dx; z0 += sz; }
    }

    return true;
}