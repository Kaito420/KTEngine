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

    fopen_s(&file, fileName, "rb");
    if (!file) {
        assert(false && "Shader file not found");
        return;
    }

    fsize = _filelength(_fileno(file));
    unsigned char* buffer = new unsigned char[fsize];
    fread(buffer, fsize, 1, file);
    fclose(file);

    //頂点シェーダー作成
    HRESULT hr = RendererDX11::GetDevice()->CreateVertexShader(buffer, fsize, NULL, &vertexShader);
    if (FAILED(hr)) {
        delete[] buffer;
		assert(false && "CreateVertexShader failed");
        return;
    }

	//入力レイアウト作成
	ID3D11ShaderReflection* pReflector = nullptr;
	hr = D3DReflect(buffer, fsize, IID_ID3D11ShaderReflection, (void**)&pReflector);
    if (SUCCEEDED(hr)) {
        D3D11_SHADER_DESC shaderDesc;
        pReflector->GetDesc(&shaderDesc);

        //inputLayoutDescの要素を格納するvector
        std::vector<D3D11_INPUT_ELEMENT_DESC> inputLayoutDesc;

        for (UINT i = 0; i < shaderDesc.InputParameters; i++) {
            D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
            pReflector->GetInputParameterDesc(i, &paramDesc);

            D3D11_INPUT_ELEMENT_DESC elementDesc = {};
            elementDesc.SemanticName = paramDesc.SemanticName;  //セマンティクス名
            elementDesc.SemanticIndex = paramDesc.SemanticIndex;    //セマンティクスインデックス
            elementDesc.Format = GetDXGIFormat(paramDesc);  //フォーマット自動決定
			elementDesc.InputSlot = 0;  //通常は0、インスタンシングをするときに変わる
            elementDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
            elementDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
            elementDesc.InstanceDataStepRate = 0;

            inputLayoutDesc.push_back(elementDesc);
        }
        //InputLayout作成
        hr = RendererDX11::GetDevice()->CreateInputLayout(
            inputLayoutDesc.data(), (UINT)inputLayoutDesc.size(),
            buffer, fsize, &vertexLayout);

        if (FAILED(hr))
            assert(false && "CreateInputLayout failed from reflection");

		pReflector->Release();  //インターフェース解放

    }
    else {
        assert(false && "D3DReflect failed");
    }

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

DXGI_FORMAT ShaderManager::GetDXGIFormat(const D3D11_SIGNATURE_PARAMETER_DESC& paramDesc) {
    //成分数（マスクのビット数）を計算
    int componentCount = 0;
	if (paramDesc.Mask & 1) componentCount++;
	if (paramDesc.Mask & 2) componentCount++;
	if (paramDesc.Mask & 4) componentCount++;
	if (paramDesc.Mask & 8) componentCount++;

	//成分の型に基づいてDXGI_FORMATを決定
    if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) {
        if (componentCount == 1) return DXGI_FORMAT_R32_FLOAT;
		if (componentCount == 2) return DXGI_FORMAT_R32G32_FLOAT;
		if (componentCount == 3) return DXGI_FORMAT_R32G32B32_FLOAT;
		if (componentCount == 4) return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) {
		if (componentCount == 1) return DXGI_FORMAT_R32_SINT;
		if (componentCount == 2) return DXGI_FORMAT_R32G32_SINT;
		if (componentCount == 3) return DXGI_FORMAT_R32G32B32_SINT;
        if (componentCount == 4) return DXGI_FORMAT_R32G32B32A32_SINT;
    }
    else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) {
		if (componentCount == 1) return DXGI_FORMAT_R32_UINT;
		if (componentCount == 2) return DXGI_FORMAT_R32G32_UINT;
        if (componentCount == 3) return DXGI_FORMAT_R32G32B32_UINT;
		if (componentCount == 4) return DXGI_FORMAT_R32G32B32A32_UINT;
    }

	//対応するフォーマットが見つからない場合はUNKNOWNを返す
    return DXGI_FORMAT_UNKNOWN;
}
