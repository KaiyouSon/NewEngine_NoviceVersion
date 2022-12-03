#include "Texture.h"
#include "RenderBase.h"
#include <DirectXTex.h>
using namespace std;
using namespace DirectX;

void TextureManager::Add(unique_ptr<Texture>&& texture, const string& tag)
{
	list.push_back(move(texture));
}

void TextureManager::Add(std::unique_ptr<Texture>&& texture, const uint32_t& index)
{
	list.push_back(move(texture));
}

Texture* TextureManager::GetBackTexture()
{
	if (list.size() > 0)
	{
		return list.back().get();
	}
}

Texture* TextureManager::GetTexture(const string& tag)
{

	return nullptr;
}

Texture* TextureManager::GetTexture(const uint32_t& index)
{

	return nullptr;
}

void TextureManager::DestroyTexture(const string& tag)
{

}

unique_ptr<TextureManager> gameTextureList(new TextureManager);
unique_ptr<TextureManager> materialTextureList(new TextureManager);

Texture::Texture(std::string filePath)
{
	std::string path = "Application/Resources/Texture/" + filePath;

	HRESULT result;
	TexMetadata metadata{};
	ScratchImage scratchImg{};
	wstring wfilePath(path.begin(), path.end());

	// WICテクスチャのロード
	result = LoadFromWICFile(
		wfilePath.c_str(),
		WIC_FLAGS_NONE,
		&metadata, scratchImg);
	if (result != S_OK)
	{
		assert(SUCCEEDED(result));
	}

	// ミップマップ生成
	ScratchImage mipChain{};
	result = GenerateMipMaps(
		scratchImg.GetImages(),
		scratchImg.GetImageCount(),
		scratchImg.GetMetadata(),
		TEX_FILTER_DEFAULT, 0, mipChain);
	if (SUCCEEDED(result))
	{
		scratchImg = std::move(mipChain);
		metadata = scratchImg.GetMetadata();
	}

	// 読み込んだディフューズテクスチャをSRGBとして扱う
	metadata.format = MakeSRGB(metadata.format);

	// ヒープの設定
	D3D12_HEAP_PROPERTIES textureHeapProp{};
	textureHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	textureHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	textureHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

	// リソース設定
	D3D12_RESOURCE_DESC textureResourceDesc{};
	textureResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureResourceDesc.Format = metadata.format;
	textureResourceDesc.Width = metadata.width; // 幅
	textureResourceDesc.Height = (UINT)metadata.height; // 高さ
	textureResourceDesc.DepthOrArraySize = (UINT16)metadata.arraySize;
	textureResourceDesc.MipLevels = (UINT16)metadata.mipLevels;
	textureResourceDesc.SampleDesc.Count = 1;

	// テクスチャのサイズをセット
	//texture->SetTextureSize(Vec2(textureResourceDesc.Width, textureResourceDesc.Height));

	// テクスチャバッファの生成
	result = RenderBase::GetInstance()->GetDevice()->
		CreateCommittedResource(
			&textureHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&textureResourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&buffer));
	assert(SUCCEEDED(result));

	// 全ミップマップについて
	for (size_t i = 0; i < metadata.mipLevels; i++)
	{
		// 全ミップマップレベルを指定してイメージを取得
		const Image* img = scratchImg.GetImage(i, 0, 0);
		// テクスチャバッファにデータ転送
		result = buffer->WriteToSubresource(
			(UINT)i,
			nullptr,				// 全領域へコピー
			img->pixels,			// 元データアドレス
			(UINT)img->rowPitch,	// １ラインサイズ
			(UINT)img->slicePitch	// １枚サイズ
		);
		assert(SUCCEEDED(result));
	}

	RenderBase::GetInstance()->CreateSrv(*this, textureResourceDesc);
}
