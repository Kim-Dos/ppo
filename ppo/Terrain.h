#pragma once
#include "d3dUtil.h"
#include "HeightMapImage.h"
#include "FrameResource.h"

class Terrain
{
public:
	Terrain();
	~Terrain();

	void LoadHeightMap(const wchar_t* fileName, int width, int length, XMFLOAT3 scale);
	void CreateTerrain(float width, float length, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

	HeightMapImage GetHeightMapImage() { return mHeightImage; }
private:
	HeightMapImage mHeightImage;
};
