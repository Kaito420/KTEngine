//=====================================================================================
// ModelRenderer.cpp
// Author:Kaito Aoki
// Date:2025/09/14
//=====================================================================================

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include <filesystem>
#include <imgui.h>


#include "Renderer.h"
#include "modelRenderer.h"
#include "Texture.h"
#include "GameObject.h"
#include "ShaderManager.h"
#include "Shader.h"


std::unordered_map<std::string, MODEL*> ModelRenderer::m_ModelPool;


void ModelRenderer::Awake()
{
	if (_owner && !_owner->GetComponent<Shader>()) {
		_owner->AddComponent<Shader>();
	}
}

void ModelRenderer::Render() const
{
	if (!m_Model || !m_Model->VertexBuffer || !m_Model->IndexBuffer) return;

	// Transform 行列計算
	XMMATRIX translation = XMMatrixTranslation(_owner->_transform._position.x, _owner->_transform._position.y, _owner->_transform._position.z);
	KTVECTOR3 radians = { XMConvertToRadians(_owner->_transform._rotation.x),
						  XMConvertToRadians(_owner->_transform._rotation.y),
						  XMConvertToRadians(_owner->_transform._rotation.z) };
	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(radians.x, radians.y, radians.z);
	XMMATRIX scaling = XMMatrixScaling(_owner->_transform._scale.x, _owner->_transform._scale.y, _owner->_transform._scale.z);
	XMMATRIX worldMatrix = scaling * rotation * translation;

	auto cmdList = Renderer::GetCommandListDX12();
	if (!cmdList) return;

	if (Renderer::IsShadowPass()) {
		// 頂点バッファビュー設定
		D3D12_VERTEX_BUFFER_VIEW vbView = {};
		vbView.BufferLocation = m_Model->VertexBuffer->Resource->GetGPUVirtualAddress();
		vbView.StrideInBytes = m_Model->VertexBuffer->Stride;
		vbView.SizeInBytes = m_Model->VertexBuffer->Stride * m_Model->VertexBuffer->Size;
		cmdList->IASetVertexBuffers(0, 1, &vbView);

		// インデックスバッファビュー設定
		D3D12_INDEX_BUFFER_VIEW ibView = {};
		ibView.BufferLocation = m_Model->IndexBuffer->Resource->GetGPUVirtualAddress();
		ibView.SizeInBytes = sizeof(unsigned int) * m_Model->IndexBuffer->Size;
		ibView.Format = DXGI_FORMAT_R32_UINT;
		cmdList->IASetIndexBuffer(&ibView);

		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdList->SetPipelineState(Renderer::GetShadowPipelineState());
		Renderer::SetWorldMatrix(worldMatrix);
		for (unsigned int i = 0; i < m_Model->SubsetNum; i++) {
			cmdList->DrawIndexedInstanced(m_Model->SubsetArray[i].IndexNum, 1, m_Model->SubsetArray[i].StartIndex, 0, 0);
		}
		return;
	}

	int blendMode = 0;
	if (Renderer::IsGeometryPass() && blendMode != 0) return;
	if (!Renderer::IsGeometryPass() && blendMode == 0) return;

	// 頂点バッファビュー設定
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = m_Model->VertexBuffer->Resource->GetGPUVirtualAddress();
	vbView.StrideInBytes = m_Model->VertexBuffer->Stride;
	vbView.SizeInBytes = m_Model->VertexBuffer->Stride * m_Model->VertexBuffer->Size;
	cmdList->IASetVertexBuffers(0, 1, &vbView);

	// インデックスバッファビュー設定
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = m_Model->IndexBuffer->Resource->GetGPUVirtualAddress();
	ibView.SizeInBytes = sizeof(unsigned int) * m_Model->IndexBuffer->Size;
	ibView.Format = DXGI_FORMAT_R32_UINT;
	cmdList->IASetIndexBuffer(&ibView);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// PSOバインド
	{
		std::string vsId = "Geometry";
		std::string psId = "Geometry";
		auto shaderComp = _owner->GetComponent<Shader>();
		if (shaderComp) {
			vsId = shaderComp->GetVertexShaderID();
			psId = shaderComp->GetPixelShaderID();
		}
		ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(vsId, psId, 0, Renderer::GetCullModeDX12(), Renderer::GetDepthEnableDX12(), Renderer::GetDepthWriteDX12(), D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		if (pso == nullptr) return;
		cmdList->SetPipelineState(pso);
	}

	Renderer::SetWorldMatrix(worldMatrix);

	for (unsigned int i = 0; i < m_Model->SubsetNum; i++)
	{
		MATERIAL material = m_Model->SubsetArray[i].Material.Material;
		material.BaseColor = material.Diffuse;
		material.EmissionColor = material.Emission;
		material.Metallic = 0.0f;
		material.SpecularPbr = 0.5f;
		material.Roughness = 0.5f;
		material.NormalWeight = 1.0f;
		material.ShadingModelID = 0;

		const TEXTURE* tex = nullptr;
		auto shaderComp = _owner->GetComponent<Shader>();
		if (shaderComp && shaderComp->GetTexture()) {
			tex = shaderComp->GetTexture();
		} else {
			tex = m_Model->SubsetArray[i].Material.Texture;
		}
		material.TextureEnable = (tex != nullptr);
		if (shaderComp) {
			XMFLOAT4 scColor = shaderComp->GetBaseColor();
			if (tex && scColor.x == 0.0f && scColor.y == 0.0f && scColor.z == 0.0f) {
				scColor.x = 1.0f;
				scColor.y = 1.0f;
				scColor.z = 1.0f;
			}
			material.BaseColor = scColor;
			material.EmissionColor = shaderComp->GetEmissionColor();
			material.Metallic = shaderComp->GetMetallic();
			material.SpecularPbr = shaderComp->GetSpecular();
			material.Roughness = shaderComp->GetRoughness();
			material.NormalWeight = shaderComp->GetNormalWeight();
			material.ShadingModelID = shaderComp->GetShadingModelID();
			material.FlipU = shaderComp->GetFlipU() ? 1 : 0;
			material.FlipV = shaderComp->GetFlipV() ? 1 : 0;
			material.HasNormalMap = shaderComp->GetNormalMap() ? 1 : 0;
			material.HasMetallicMap = shaderComp->GetMetallicMap() ? 1 : 0;
			material.HasRoughnessMap = shaderComp->GetRoughnessMap() ? 1 : 0;
		} else {
			material.BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			material.HasNormalMap = 0;
			material.HasMetallicMap = 0;
			material.HasRoughnessMap = 0;
		}

		Renderer::SetConstant(3, &material, sizeof(MATERIAL));

		if (tex) {
			Renderer::SetTexture(6, tex);
		}
		if (shaderComp && shaderComp->GetNormalMap()) {
			Renderer::SetTexture(7, shaderComp->GetNormalMap());
		}
		if (shaderComp && shaderComp->GetMetallicMap()) {
			Renderer::SetTexture(8, shaderComp->GetMetallicMap());
		}
		if (shaderComp && shaderComp->GetRoughnessMap()) {
			Renderer::SetTexture(9, shaderComp->GetRoughnessMap());
		}

		cmdList->DrawIndexedInstanced(m_Model->SubsetArray[i].IndexNum, 1, m_Model->SubsetArray[i].StartIndex, 0, 0);
	}
}

void ModelRenderer::Preload(const char* FileName)
{
	if (m_ModelPool.count(FileName) > 0)
	{
		return;
	}

	MODEL* model = new MODEL;
	LoadModel(FileName, model);

	m_ModelPool[FileName] = model;

}


void ModelRenderer::UnloadAll()
{
	for (std::pair<const std::string, MODEL*> pair : m_ModelPool)
	{
		pair.second->VertexBuffer.reset();
		pair.second->IndexBuffer.reset();

		for (unsigned int i = 0; i < pair.second->SubsetNum; i++)
		{
			// eNX`͎ŃfXgN^Ȃ(̃|C^̂)߁A蓮ŃZbg(ۂɂ_texturePoolŊǗĂ̂łł͉̕Kv͂ȂAQƂnullptrɂĂ)
			pair.second->SubsetArray[i].Material.Texture = nullptr;
		}

		delete[] pair.second->SubsetArray;
		delete pair.second;
	}

	m_ModelPool.clear();
}


void ModelRenderer::Load(const char* FileName)
{
	//t@CȂ牽
	if(FileName == nullptr || strlen(FileName) == 0)
		return;

	//dǂݍݖh~
	if(m_ModelPool.find(FileName) != m_ModelPool.end())
	{
		m_Model = m_ModelPool[FileName];
		return;
	}

	//VK[h
	MODEL* newModel = new MODEL();
	LoadModel(FileName, newModel);
	if (newModel->VertexBuffer != nullptr) {
		m_ModelPool[FileName] = newModel;
		m_Model = newModel;
	}
	else {
		delete newModel;
		m_Model = nullptr;
	}

}


void ModelRenderer::LoadModel(const char* FileName, MODEL* Model)
{
	MODEL_OBJ modelObj;
	LoadObj(FileName, &modelObj);

	// _obt@쐬
	Model->VertexBuffer = Renderer::CreateVertexBuffer(sizeof(Vertex), modelObj.VertexNum);
	void* data = nullptr;
	HRESULT hr = Model->VertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, modelObj.VertexArray, sizeof(Vertex) * modelObj.VertexNum);
		Model->VertexBuffer->Resource->Unmap(0, nullptr);
	}

	// CfbNXobt@쐬
	Model->IndexBuffer = Renderer::CreateIndexBuffer(modelObj.IndexNum);
	hr = Model->IndexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, modelObj.IndexArray, sizeof(unsigned int) * modelObj.IndexNum);
		Model->IndexBuffer->Resource->Unmap(0, nullptr);
	}

	// TuZbgݒ
	{
		Model->SubsetArray = new SUBSET[modelObj.SubsetNum];
		Model->SubsetNum = modelObj.SubsetNum;
		if (modelObj.SubsetNum > 0) {
			memset(Model->SubsetArray, 0, sizeof(SUBSET) * modelObj.SubsetNum);
		}

		for (unsigned int i = 0; i < modelObj.SubsetNum; i++)
		{
			Model->SubsetArray[i].StartIndex = modelObj.SubsetArray[i].StartIndex;
			Model->SubsetArray[i].IndexNum = modelObj.SubsetArray[i].IndexNum;
			Model->SubsetArray[i].Material.Material = modelObj.SubsetArray[i].Material.Material;

			// eNX`[h
			Model->SubsetArray[i].Material.Texture = Texture::Load(modelObj.SubsetArray[i].Material.TextureName);

			if (Model->SubsetArray[i].Material.Texture)
				Model->SubsetArray[i].Material.Material.TextureEnable = true;
			else
				Model->SubsetArray[i].Material.Material.TextureEnable = false;
		}
	}

	delete[] modelObj.VertexArray;
	delete[] modelObj.IndexArray;
	delete[] modelObj.SubsetArray;
}






//f////////////////////////////////////////////
void ModelRenderer::LoadObj(const char* FileName, MODEL_OBJ* ModelObj)
{

	char dir[MAX_PATH];
	strcpy(dir, FileName);
	PathRemoveFileSpec(dir);





	XMFLOAT3* positionArray;
	XMFLOAT3* normalArray;
	XMFLOAT2* texcoordArray;

	unsigned int	positionNum = 0;
	unsigned int	normalNum = 0;
	unsigned int	texcoordNum = 0;
	unsigned int	vertexNum = 0;
	unsigned int	indexNum = 0;
	unsigned int	in = 0;
	unsigned int	subsetNum = 0;

	MODEL_MATERIAL* materialArray = nullptr;
	unsigned int	materialNum = 0;

	char str[256];
	char* s;
	char c;


	FILE* file;
	file = fopen(FileName, "rt");
	if (!file) return;



	//vfJEg
	while (true)
	{
		fscanf(file, "%s", str);

		if (feof(file) != 0)
			break;

		if (strcmp(str, "v") == 0)
		{
			positionNum++;
		}
		else if (strcmp(str, "vn") == 0)
		{
			normalNum++;
		}
		else if (strcmp(str, "vt") == 0)
		{
			texcoordNum++;
		}
		else if (strcmp(str, "usemtl") == 0)
		{
			subsetNum++;
		}
		else if (strcmp(str, "f") == 0)
		{
			in = 0;

			do
			{
				fscanf(file, "%s", str);
				vertexNum++;
				in++;
				c = fgetc(file);
			} while (c != '\n' && c != '\r');

			//lp͎Op
			if (in == 4)
				in = 6;

			indexNum += in;
		}
	}


	//m
	positionArray = new XMFLOAT3[positionNum];
	normalArray = new XMFLOAT3[normalNum];
	texcoordArray = new XMFLOAT2[texcoordNum];


	ModelObj->VertexArray = new Vertex[vertexNum];
	ModelObj->VertexNum = vertexNum;

	ModelObj->IndexArray = new unsigned int[indexNum];
	ModelObj->IndexNum = indexNum;

	ModelObj->SubsetArray = new SUBSET[subsetNum];
	ModelObj->SubsetNum = subsetNum;
	if (subsetNum > 0) {
		memset(ModelObj->SubsetArray, 0, sizeof(SUBSET) * subsetNum);
	}




	//vf
	XMFLOAT3* position = positionArray;
	XMFLOAT3* normal = normalArray;
	XMFLOAT2* texcoord = texcoordArray;

	unsigned int vc = 0;
	unsigned int ic = 0;
	unsigned int sc = 0;


	fseek(file, 0, SEEK_SET);

	while (true)
	{
		fscanf(file, "%s", str);

		if (feof(file) != 0)
			break;

		if (strcmp(str, "mtllib") == 0)
		{
			//}eAt@C
			fscanf(file, "%s", str);

			char path[256];
			strcpy(path, dir);
			strcat(path, "\\");
			strcat(path, str);

			LoadMaterial(path, &materialArray, &materialNum);
		}
		else if (strcmp(str, "o") == 0)
		{
			//IuWFNg
			fscanf(file, "%s", str);
		}
		else if (strcmp(str, "v") == 0)
		{
			//_W
			fscanf(file, "%f", &position->x);
			fscanf(file, "%f", &position->y);
			fscanf(file, "%f", &position->z);
			position->z = -position->z; // Flip Z to convert RH to LH
			position++;
		}
		else if (strcmp(str, "vn") == 0)
		{
			//@
			fscanf(file, "%f", &normal->x);
			fscanf(file, "%f", &normal->y);
			fscanf(file, "%f", &normal->z);
			normal->z = -normal->z; // Flip Z to convert RH to LH
			normal++;
		}
		else if (strcmp(str, "vt") == 0)
		{
			//eNX`W
			fscanf(file, "%f", &texcoord->x);
			fscanf(file, "%f", &texcoord->y);
			texcoord->y = 1.0f - texcoord->y;
			texcoord++;
		}
		else if (strcmp(str, "usemtl") == 0)
		{
			//}eA
			fscanf(file, "%s", str);

			if (sc != 0)
				ModelObj->SubsetArray[sc - 1].IndexNum = ic - ModelObj->SubsetArray[sc - 1].StartIndex;

			ModelObj->SubsetArray[sc].StartIndex = ic;


			for (unsigned int i = 0; i < materialNum; i++)
			{
				if (strcmp(str, materialArray[i].Name) == 0)
				{
					ModelObj->SubsetArray[sc].Material.Material = materialArray[i].Material;
					strcpy(ModelObj->SubsetArray[sc].Material.TextureName, materialArray[i].TextureName);
					strcpy(ModelObj->SubsetArray[sc].Material.Name, materialArray[i].Name);

					break;
				}
			}

			sc++;

		}
		else if (strcmp(str, "f") == 0)
		{
			//
			in = 0;

			do
			{
				fscanf(file, "%s", str);

				s = strtok(str, "/");
				ModelObj->VertexArray[vc].position = positionArray[atoi(s) - 1];
				if (s[strlen(s) + 1] != '/')
				{
					//eNX`W݂Ȃꍇ
					s = strtok(nullptr, "/");
					ModelObj->VertexArray[vc].uv = texcoordArray[atoi(s) - 1];
				}
				s = strtok(nullptr, "/");
				ModelObj->VertexArray[vc].normal = normalArray[atoi(s) - 1];

				ModelObj->VertexArray[vc].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

				ModelObj->IndexArray[ic] = vc;
				ic++;
				vc++;

				in++;
				c = fgetc(file);
			} while (c != '\n' && c != '\r');

			//lp͎Op
			if (in == 4)
			{
				ModelObj->IndexArray[ic] = vc - 4;
				ic++;
				ModelObj->IndexArray[ic] = vc - 2;
				ic++;
			}
		}
	}


	if (sc != 0)
		ModelObj->SubsetArray[sc - 1].IndexNum = ic - ModelObj->SubsetArray[sc - 1].StartIndex;


	fclose(file);

	// Reverse winding order to match LH coordinate system culling
	for (unsigned int i = 0; i < ModelObj->IndexNum; i += 3)
	{
		std::swap(ModelObj->IndexArray[i + 1], ModelObj->IndexArray[i + 2]);
	}


	delete[] positionArray;
	delete[] normalArray;
	delete[] texcoordArray;
	delete[] materialArray;
}




//}eAǂ///////////////////////////////////////////////////////////////////
void ModelRenderer::LoadMaterial(const char* FileName, MODEL_MATERIAL** MaterialArray, unsigned int* MaterialNum)
{

	char dir[MAX_PATH];
	strcpy(dir, FileName);
	PathRemoveFileSpec(dir);



	char str[256];

	FILE* file;
	file = fopen(FileName, "rt");
	if (!file)
	{
		MODEL_MATERIAL* defaultMat = new MODEL_MATERIAL[1];
		memset(defaultMat, 0, sizeof(MODEL_MATERIAL));
		defaultMat[0].Material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
		defaultMat[0].Material.BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		defaultMat[0].Material.SpecularPbr = 0.5f;
		defaultMat[0].Material.Roughness = 0.5f;
		defaultMat[0].Material.Shininess = 1.0f;
		defaultMat[0].Texture = nullptr;
		defaultMat[0].TextureName[0] = '\0';
		*MaterialArray = defaultMat;
		*MaterialNum = 1;
		return;
	}

	MODEL_MATERIAL* materialArray;
	unsigned int materialNum = 0;

	//vfJEg
	while (true)
	{
		fscanf(file, "%s", str);

		if (feof(file) != 0)
			break;


		if (strcmp(str, "newmtl") == 0)
		{
			materialNum++;
		}
	}


	//m
	materialArray = new MODEL_MATERIAL[materialNum];


	//vf
	int mc = -1;

	fseek(file, 0, SEEK_SET);

	while (true)
	{
		fscanf(file, "%s", str);

		if (feof(file) != 0)
			break;


		if (strcmp(str, "newmtl") == 0)
		{
			//}eA
			mc++;
			fscanf(file, "%s", materialArray[mc].Name);
			strcpy(materialArray[mc].TextureName, "");

			materialArray[mc].Material.Emission.x = 0.0f;
			materialArray[mc].Material.Emission.y = 0.0f;
			materialArray[mc].Material.Emission.z = 0.0f;
			materialArray[mc].Material.Emission.w = 0.0f;
		}
		else if (strcmp(str, "Ka") == 0)
		{
			//ArGg
			fscanf(file, "%f", &materialArray[mc].Material.Ambient.x);
			fscanf(file, "%f", &materialArray[mc].Material.Ambient.y);
			fscanf(file, "%f", &materialArray[mc].Material.Ambient.z);
			materialArray[mc].Material.Ambient.w = 1.0f;
		}
		else if (strcmp(str, "Kd") == 0)
		{
			//fBt[Y
			fscanf(file, "%f", &materialArray[mc].Material.Diffuse.x);
			fscanf(file, "%f", &materialArray[mc].Material.Diffuse.y);
			fscanf(file, "%f", &materialArray[mc].Material.Diffuse.z);
			materialArray[mc].Material.Diffuse.w = 1.0f;
		}
		else if (strcmp(str, "Ks") == 0)
		{
			//XyL
			fscanf(file, "%f", &materialArray[mc].Material.Specular.x);
			fscanf(file, "%f", &materialArray[mc].Material.Specular.y);
			fscanf(file, "%f", &materialArray[mc].Material.Specular.z);
			materialArray[mc].Material.Specular.w = 1.0f;
		}
		else if (strcmp(str, "Ns") == 0)
		{
			//XyLx
			fscanf(file, "%f", &materialArray[mc].Material.Shininess);
		}
		else if (strcmp(str, "d") == 0)
		{
			//At@
			fscanf(file, "%f", &materialArray[mc].Material.Diffuse.w);
		}
		else if (strcmp(str, "map_Kd") == 0)
		{
			//eNX`
			fscanf(file, "%s", str);

			char path[256];
			strcpy(path, dir);
			strcat(path, "\\");
			strcat(path, str);

			strcat(materialArray[mc].TextureName, path);
		}
	}

	fclose(file);

	*MaterialArray = materialArray;
	*MaterialNum = materialNum;
}

std::string ModelRenderer::GetLoadedFileName() const {
	if (!m_Model) return "";
	for (const auto& pair : m_ModelPool) {
		if (pair.second == m_Model) {
			return pair.first;
		}
	}
	return "";
}

void ModelRenderer::ShowUI() {
	ImGui::Text("Model Renderer Component");
	ImGui::Separator();

	std::string currentPath = GetLoadedFileName();
	std::string currentFileName = "None";
	if (!currentPath.empty()) {
		std::filesystem::path p(currentPath);
		currentFileName = p.filename().string();
	}

	if (ImGui::BeginCombo("Select Model", currentFileName.c_str())) {
		std::string modelDir = "asset/model";
		if (std::filesystem::exists(modelDir) && std::filesystem::is_directory(modelDir)) {
			for (const auto& entry : std::filesystem::directory_iterator(modelDir)) {
				if (entry.is_regular_file()) {
					std::filesystem::path filePath = entry.path();
					std::string ext = filePath.extension().string();
					if (ext == ".obj" || ext == ".OBJ" || ext == ".fbx" || ext == ".FBX" || ext == ".gltf" || ext == ".GLTF") {
						std::string filename = filePath.filename().string();
						std::string relativePath = filePath.generic_string();

						bool isSelected = (currentPath == relativePath || currentFileName == filename);
						if (ImGui::Selectable(filename.c_str(), isSelected)) {
							Load(relativePath.c_str());
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
				}
			}
		} else {
			ImGui::Selectable("asset/model not found", false, ImGuiSelectableFlags_Disabled);
		}
		ImGui::EndCombo();
	}

	if (m_Model) {
		ImGui::Spacing();
		ImGui::Text("Loaded Model Info:");
		ImGui::Text(" - Path: %s", currentPath.c_str());
		ImGui::Text(" - Subsets: %u", m_Model->SubsetNum);
		if (m_Model->VertexBuffer) {
			ImGui::Text(" - Vertices: %u", m_Model->VertexBuffer->Size);
		}
		if (m_Model->IndexBuffer) {
			ImGui::Text(" - Indices: %u", m_Model->IndexBuffer->Size);
		}
	}
}

