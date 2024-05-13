#include "Terrain.h"

Terrain::Terrain()
{
}

Terrain::~Terrain()
{
}

void Terrain::LoadHeightMap(const wchar_t* filepath, int width, int length, float yScale)
{
	mImageWidth = width;
	mImageLength = length;

	mHeightImage.LoadHeightMapImage(filepath, width, length, yScale);
}

void Terrain::CreateTerrain(float width, float length, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
{
	mWidth = width;
	mLength = length;

	int imageWidth = mHeightImage.GetHeightMapWidth();
	int imageLength = mHeightImage.GetHeightMapLength();

	uint32_t vertexCount =  imageWidth * imageLength;

	mHeight = std::vector<float>(vertexCount);
	mNormal = std::vector<XMFLOAT3>(vertexCount);
	//
	// Create the vertices.
	//

	float halfWidth = 0.5f * width;
	float halflength = 0.5f * length;

	float dx = width / (imageWidth - 1);
	float dz = length / (imageLength - 1);

	float du = 1.0f / (imageWidth - 1);
	float dv = 1.0f / (imageLength - 1);

	for (uint32_t i = 0; i < imageLength; ++i)
	{
		float z = halflength - i * dz;
		for (uint32_t j = 0; j < imageWidth; ++j)
		{
			float x = -halfWidth + j * dx;
			float y = mHeightImage.GetHeight(j, i);

			vertices[i * imageWidth + j].Pos = XMFLOAT3(x, y, z);
			vertices[i * imageWidth + j].Normal = mHeightImage.GetHeightMapNormal(j, i, dx, dz);
			//vertices[i * imageWidth + j].TangentU = XMFLOAT3(1.0f, 0.0f, 0.0f);

			// Stretch texture over grid.
			vertices[i * imageWidth + j].TexC.x = j * du;
			vertices[i * imageWidth + j].TexC.y = i * dv;
		}
	}

	// terrain y pos 값 평탄화, normal 값 평탄화
	int numFlattening = 2;
	// 상하좌우로 numFlattening만큼의 정점을 기준으로 평탄화를 진행한다.
	// numFlattening = 2일 경우 하나의 정점 평탄화에 총 25((1 + 2 * numFlattening)^2) 칸의 정점이 사용된다.

	std::vector<Vertex> newVertices(vertices.size());
	for (uint32_t i = 0; i < imageLength; ++i)
	{
		for (uint32_t j = 0; j < imageWidth; ++j)
		{
			int index = i * imageWidth + j;
			float newY = 0.0f;
			XMFLOAT3 newNormal = XMFLOAT3(0.f, 0.f, 0.f);
			int numAddedVertices = 0;

			for (int di = -numFlattening; di <= numFlattening; di++)
			{
				if ((i + di) < 0 || (i + di) >= imageLength)
					continue;

				for (int dj = -numFlattening; dj <= numFlattening; dj++)
				{
					if ((j + dj) < 0 || (j + dj) >= imageLength)
						continue;

					int dIndex = (i + di) * imageWidth + (j + dj);

					newY += vertices[dIndex].Pos.y;
					newNormal.x += vertices[dIndex].Normal.x;
					newNormal.y += vertices[dIndex].Normal.y;
					newNormal.z += vertices[dIndex].Normal.z;
					numAddedVertices++;
				}
			}

			newVertices[index].Pos = XMFLOAT3(vertices[index].Pos.x, newY / numAddedVertices, vertices[index].Pos.z);
			XMStoreFloat3(&newVertices[index].Normal, XMVector3Normalize(XMLoadFloat3(&newNormal)));
			newVertices[index].TexC = vertices[index].TexC;

			mHeight[index] = newVertices[index].Pos.y;
			mNormal[index] = newVertices[index].Normal;
		}
	}
	vertices = newVertices;
	//
	// Create the indices.
	//

	uint32_t faceCount = (imageWidth - 1) * (imageLength - 1) * 2;

	// Iterate over each quad and compute indices.
	uint32_t k = 0;
	for (uint32_t i = 0; i < imageLength - 1; ++i)
	{
		for (uint32_t j = 0; j < imageWidth - 1; ++j)
		{
			indices[k] = i * imageWidth + j;
			indices[k + 1] = i * imageWidth + j + 1;
			indices[k + 2] = (i + 1) * imageWidth + j;

			indices[k + 3] = (i + 1) * imageWidth + j;
			indices[k + 4] = i * imageWidth + j + 1;
			indices[k + 5] = (i + 1) * imageWidth + j + 1;

			k += 6; // next quad
		}
	}

	return;
}

float Terrain::GetHeight(float x, float z)
{
	float baseX = (x + mWidth / 2);
	float baseZ = mLength - (z + mLength / 2);

	float dWidth = baseX / mWidth;
	float dLength = baseZ / mLength;

	int indexX = int(mImageWidth * dWidth);
	int indexZ = int(mImageLength * dLength);

	float dIndexX = (mImageWidth * dWidth) - indexX;
	float dIndexZ = (mImageWidth * dLength) - indexZ;

	// 높이 보간
	float height = mHeight[indexX + indexZ * mImageWidth];
	float finalheight = height;

	float heightX = mHeight[(indexX + 1) + indexZ * mImageWidth];
	float heightY = mHeight[indexX + (indexZ - 1) * mImageWidth];

	finalheight += (heightX - height) * dIndexX;
	finalheight += (heightY - height) * dIndexZ;
	/*
	if (dIndexX + dIndexZ >= 1.0f) {
		
	}
	else {
		height = mHeight[(indexX + 1) + (indexZ - 1) * mImageWidth];
		float heightX = mHeight[(indexX + 1) + indexZ * mImageWidth];
		float heightY = mHeight[indexX + (indexZ - 1) * mImageWidth];

		finalheight += (heightX - height) * (1.0f - dIndexX);
		finalheight += (heightY - height) * (1.0f - dIndexZ);
	}
	*/
	return finalheight;
}