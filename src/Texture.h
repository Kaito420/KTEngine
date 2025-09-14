//=====================================================================================
// Texture.h
// Author:Kaito Aoki
// Date:2025/09/14
//=====================================================================================

#ifndef _TEXTURE_H_
#define _TEXTURE_H_

#include <string>
#include <d3d11.h>
#include <unordered_map>

//ƒNƒ‰ƒX‚É‚·‚éˆÓ–¡‚ª‚ ‚é‚Ì‚©ŒŸ“¢’†

class Texture {

private:
	static std::unordered_map<std::string, ID3D11ShaderResourceView*> _texturePool;

public:
	static ID3D11ShaderResourceView* Load(const char* FileName);
};

#endif // !_TEXTURE_H_