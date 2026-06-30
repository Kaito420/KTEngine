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
};

#endif // !_TEXTURE_H_