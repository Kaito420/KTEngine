//=====================================================================================
// Texture.h
// Author:Kaito Aoki
// Date:2025/09/14
//=====================================================================================

#ifndef _TEXTURE_H_
#define _TEXTURE_H_

#include <string>
#include <unordered_map>
#include <memory>
#include "Renderer.h"

//クラスにする意味があるのか検討中

class Texture {

private:
	static std::unordered_map<std::string, std::unique_ptr<TEXTURE>> _texturePool;

public:
	static const TEXTURE* Load(const char* FileName);
	static const TEXTURE* LoadFromMemory(const std::string& key, const void* data, size_t size);
	static const TEXTURE* CreateFromRGBA(const std::string& key, const unsigned char* rgbaData, unsigned int width, unsigned int height);
};

#endif // !_TEXTURE_H_