//=====================================================================================
// ShaderManager.h
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#ifndef _SHADERMANAGER_H
#define _SHADERMANAGER_H

#include <map>
#include <string>
#include <d3d11.h>

class ShaderManager {	//ƒVƒ“ƒOƒ‹ƒgƒ“
private:
	std::map < std::string, ID3D11VertexShader*> _vertexShaders;
	std::map < std::string, ID3D11InputLayout*> _vertexLayouts;
	std::map < std::string, ID3D11PixelShader*> _pixelShaders;
public:
	static ShaderManager& Instance() {
		static ShaderManager instance;
		return instance;
	}

	void LoadVertexShader(const std::string& id, const char* fileName);
	void LoadPixelShader(const std::string& id, const char* fileName);

	ID3D11VertexShader* GetVertexShader(const std::string& id);
	ID3D11InputLayout* GetInputLayout(const std::string& id);
	ID3D11PixelShader* GetPixelShader(const std::string& id);
};

#endif // !_SHADERMANAGER_H