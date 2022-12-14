
// 3D変換行列
cbuffer ConstBufferDataTransform : register(b0)
{
	matrix mat; // 3D変換行列
}

// マテリアル
cbuffer ConstBufferDataMaterial : register(b2)
{
	float4 color; // 色(RGBA)
};

// 頂点シェーダーの出力構造体
// （頂点シェーダーからピクセルシェーダーヘのやり取りに使用する）
struct VSOutput
{
	float4 svpos : SV_POSITION;	// システム用頂点座標
	float2 uv : TEXCOORD;		// uv値
};