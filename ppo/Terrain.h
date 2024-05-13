#pragma once
#include "d3dUtil.h"
#include "HeightMapImage.h"
#include "FrameResource.h"

class Terrain
{
public:
	Terrain();
	~Terrain();

	void LoadHeightMap(const wchar_t* fileName, int width, int length, float scale);
	void CreateTerrain(float width, float length, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

	HeightMapImage GetHeightMapImage() { return mHeightImage; }
	float GetHeight(float x, float y);
private:
	HeightMapImage mHeightImage;

	std::vector<float> mHeight;
	std::vector<XMFLOAT3> mNormal;
	
	float mWidth = 0.0f;
	float mLength = 0.0f;

	int mImageWidth = 0;
	int mImageLength = 0;
};
