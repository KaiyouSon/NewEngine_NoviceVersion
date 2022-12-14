#pragma once
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include <unordered_map>

class Mesh
{
private:
	VertexBuffer<VertexPosNormalUv> vertexBuffer;
	IndexBuffer indexBuffer;
	std::vector<VertexPosNormalUv> vertices;
	std::vector<unsigned short> indices;

	// 頂点法線スムージング用データ
	std::unordered_map<unsigned short, std::vector<unsigned short>> smoothData;

public:

	// スムージングデータの追加
	void AddSmoothData(const unsigned short& indexPos, const unsigned short& indexVertex);

	// Vertexの追加
	void AddVertex(const VertexPosNormalUv& vertex);

	// Indexの追加
	void AddIndex(const unsigned short& index);

	// バッファーの生成
	void CreateBuffer();

	// 平滑化された頂点法線の計算
	void CalculateSmoothedVertexNormals();

public:
	// ゲッター
	inline size_t GetVertexSize() { return vertices.size(); }
	inline size_t GetIndexSize() { return indices.size(); }

	inline VertexBuffer<VertexPosNormalUv> GetVertexBuffer() { return vertexBuffer; }
	inline IndexBuffer GetIndexBuffer() { return indexBuffer; }
};

