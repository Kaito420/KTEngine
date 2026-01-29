//=====================================================================================
// ShaderManager.cpp
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#include "ShaderManager.h"
#include "RendererDX11.h"
#include <io.h>

void ShaderManager::LoadVertexShader(const std::string& id, const char* fileName){
	ID3D11VertexShader* vertexShader = nullptr;
	ID3D11InputLayout* vertexLayout = nullptr;
    FILE* file;
    long int fsize;

    file = fopen(fileName, "rb");
    assert(file);

    fsize = _filelength(_fileno(file));
    unsigned char* buffer = new unsigned char[fsize];

    fread(buffer, fsize, 1, file);
    fclose(file);
    RendererDX11::GetDevice()->CreateVertexShader(buffer, fsize, NULL, &vertexShader);

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 4 * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 4 * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 4 * 10, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT numElements = ARRAYSIZE(layout);

    RendererDX11::GetDevice()->CreateInputLayout(layout, numElements, buffer, fsize, &vertexLayout);

    delete[] buffer;

	_vertexLayouts[id] = vertexLayout;
	_vertexShaders[id] = vertexShader;
}

void ShaderManager::LoadPixelShader(const std::string& id, const char* fileName){
    ID3D11PixelShader* pixelShader = nullptr;

    FILE* file;
    long int fsize;

    file = fopen(fileName, "rb");
    assert(file);

    fsize = _filelength(_fileno(file));
    unsigned char* buffer = new unsigned char[fsize];

    fread(buffer, fsize, 1, file);
    fclose(file);

    RendererDX11::GetDevice()->CreatePixelShader(buffer, fsize, NULL, &pixelShader);
    
    delete[] buffer;

	_pixelShaders[id] = pixelShader;
}

ID3D11VertexShader* ShaderManager::GetVertexShader(const std::string& id){
    if(_vertexShaders.find(id) != _vertexShaders.end()){
        return _vertexShaders[id];
	}
    return nullptr;
}

ID3D11InputLayout* ShaderManager::GetInputLayout(const std::string& id) {
    if (_vertexLayouts.find(id) != _vertexLayouts.end())
        return _vertexLayouts[id];
    return nullptr;
}

ID3D11PixelShader* ShaderManager::GetPixelShader(const std::string& id){
    if(_pixelShaders.find(id) != _pixelShaders.end())
		return _pixelShaders[id];
    return nullptr;
}
