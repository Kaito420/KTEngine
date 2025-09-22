//=====================================================================================
// Texture.cpp
// Author:Kaito Aoki
// Date:2025/09/14
//=====================================================================================

#include "Texture.h"
#include "Manager.h"
#include "RendererDX11.h"

std::unordered_map<std::string, ID3D11ShaderResourceView*> Texture::_texturePool;

ID3D11ShaderResourceView* Texture::Load(const char* FileName) {

	if (_texturePool.count(FileName) > 0)
	{
		return _texturePool[FileName];
	}

	wchar_t wFileName[512];
	mbstowcs(wFileName, FileName, strlen(FileName) + 1);

	//テクスチャ読み込み
	TexMetadata metadata;
	ScratchImage image;
	ID3D11ShaderResourceView* texture;
	LoadFromWICFile(wFileName, WIC_FLAGS_NONE, &metadata, image);
	CreateShaderResourceView(RendererDX11::GetDevice(), image.GetImages(),
		image.GetImageCount(), metadata, &texture);
	assert(texture);

	_texturePool[FileName] = texture;

	return texture;
}