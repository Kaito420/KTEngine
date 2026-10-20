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
#include <vector>
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include "ufbx.h"


#include "Renderer.h"
#include "modelRenderer.h"
#include "Texture.h"
#include "GameObject.h"
#include "ShaderManager.h"
#include "Shader.h"
#include "Profiler.h"


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

	// Transform 行�E計箁E
	XMMATRIX worldMatrix = _owner->GetWorldMatrix();

	auto cmdList = Renderer::GetCommandListDX12();
	if (!cmdList) return;

	if (Renderer::IsShadowPass()) {
		// 頂点バッファビュー設宁E
		D3D12_VERTEX_BUFFER_VIEW vbView = {};
		vbView.BufferLocation = m_Model->VertexBuffer->Resource->GetGPUVirtualAddress();
		vbView.StrideInBytes = m_Model->VertexBuffer->Stride;
		vbView.SizeInBytes = m_Model->VertexBuffer->Stride * m_Model->VertexBuffer->Size;
		cmdList->IASetVertexBuffers(0, 1, &vbView);

		// インチE��クスバッファビュー設宁E
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
			Profiler::RecordDrawCall(m_Model->VertexBuffer ? (int)m_Model->VertexBuffer->Size : 0, (int)m_Model->SubsetArray[i].IndexNum);
		}
		return;
	}

	int blendMode = 0;
	if (Renderer::IsGeometryPass() && blendMode != 0) return;
	if (!Renderer::IsGeometryPass() && blendMode == 0) return;

	// 頂点バッファビュー設宁E
	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = m_Model->VertexBuffer->Resource->GetGPUVirtualAddress();
	vbView.StrideInBytes = m_Model->VertexBuffer->Stride;
	vbView.SizeInBytes = m_Model->VertexBuffer->Stride * m_Model->VertexBuffer->Size;
	cmdList->IASetVertexBuffers(0, 1, &vbView);

	// インチE��クスバッファビュー設宁E
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = m_Model->IndexBuffer->Resource->GetGPUVirtualAddress();
	ibView.SizeInBytes = sizeof(unsigned int) * m_Model->IndexBuffer->Size;
	ibView.Format = DXGI_FORMAT_R32_UINT;
	cmdList->IASetIndexBuffer(&ibView);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// PSOバインチE
	{
		std::string vsId = "Geometry";
		std::string psId = "Geometry";
		auto shaderComp = _owner->GetComponent<Shader>();
		if (shaderComp) {
			vsId = shaderComp->GetVertexShaderID();
			psId = shaderComp->GetPixelShaderID();
		}
		D3D12_CULL_MODE cullMode = Renderer::GetCullModeDX12();
		if (m_CullMode == 1) cullMode = D3D12_CULL_MODE_FRONT;
		else if (m_CullMode == 2) cullMode = D3D12_CULL_MODE_NONE;
		ID3D12PipelineState* pso = ShaderManager::Instance().GetPipelineState(vsId, psId, 0, cullMode, Renderer::GetDepthEnableDX12(), Renderer::GetDepthWriteDX12(), D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
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
		XMFLOAT4 subColor = material.Diffuse;
		if (subColor.x == 0.0f && subColor.y == 0.0f && subColor.z == 0.0f) {
			subColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		}
		if (shaderComp) {
			XMFLOAT4 scColor = shaderComp->GetBaseColor();
			if (tex && scColor.x == 0.0f && scColor.y == 0.0f && scColor.z == 0.0f) {
				scColor.x = 1.0f;
				scColor.y = 1.0f;
				scColor.z = 1.0f;
			}
			material.BaseColor = {
				scColor.x * subColor.x,
				scColor.y * subColor.y,
				scColor.z * subColor.z,
				scColor.w * (subColor.w > 0.0f ? subColor.w : 1.0f)
			};
			material.EmissionColor = shaderComp->GetEmissionColor();
			material.Metallic = shaderComp->GetMetallic();
			material.SpecularPbr = shaderComp->GetSpecular();
			material.Roughness = shaderComp->GetRoughness();
			material.NormalWeight = shaderComp->GetNormalWeight();
			material.ShadingModelID = shaderComp->GetShadingModelID();
			material.FlipU = shaderComp->GetFlipU() ? 1 : 0;
			material.FlipV = shaderComp->GetFlipV() ? 1 : 0;
			material.HasNormalMap = shaderComp->GetNormalMap() ? 1 : 0;
			material.HasMetallicMap = shaderComp->GetARMMap() ? 1 : 0;
			material.HasRoughnessMap = 0;
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
		if (shaderComp) {
			Renderer::SetTexture(8, shaderComp->GetARMMap());
		} else {
			Renderer::SetTexture(8, nullptr);
		}

		cmdList->DrawIndexedInstanced(m_Model->SubsetArray[i].IndexNum, 1, m_Model->SubsetArray[i].StartIndex, 0, 0);
		Profiler::RecordDrawCall(m_Model->VertexBuffer ? (int)m_Model->VertexBuffer->Size : 0, (int)m_Model->SubsetArray[i].IndexNum);
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
			// eNX`͎ŃfXgN^ȁẼ|C^́E߁A蓮ŃZbg(ۂɁEtexturePoolŊǗĂ̂łł͉̕Kv͂ȁEQƂnullptrɂāE
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
		ApplyFlipSettings();
		return;
	}

	//VK[h
	MODEL* newModel = new MODEL();
	LoadModel(FileName, newModel);
	if (newModel->VertexBuffer != nullptr) {
		m_ModelPool[FileName] = newModel;
		m_Model = newModel;
		ApplyFlipSettings();
	}
	else {
		delete newModel;
		m_Model = nullptr;
	}

}


void ModelRenderer::LoadModel(const char* FileName, MODEL* Model)
{
	MODEL_OBJ modelObj = {};

	std::filesystem::path filePath(FileName);
	std::string ext = filePath.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == ".fbx") {
		LoadFbx(FileName, &modelObj);
	} else {
		LoadObj(FileName, &modelObj);
	}

	if (modelObj.VertexNum == 0 || modelObj.IndexNum == 0) {
		delete[] modelObj.VertexArray;
		delete[] modelObj.IndexArray;
		delete[] modelObj.SubsetArray;
		return;
	}

	// _obt@�E�
	Model->VertexBuffer = Renderer::CreateVertexBuffer(sizeof(Vertex), modelObj.VertexNum);
	void* data = nullptr;
	HRESULT hr = Model->VertexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, modelObj.VertexArray, sizeof(Vertex) * modelObj.VertexNum);
		Model->VertexBuffer->Resource->Unmap(0, nullptr);
	}

	// CfbNXobt@�E�
	Model->IndexBuffer = Renderer::CreateIndexBuffer(modelObj.IndexNum);
	hr = Model->IndexBuffer->Resource->Map(0, nullptr, &data);
	if (SUCCEEDED(hr)) {
		memcpy(data, modelObj.IndexArray, sizeof(unsigned int) * modelObj.IndexNum);
		Model->IndexBuffer->Resource->Unmap(0, nullptr);
	}

	// TuZbg݁E
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
			strncpy(Model->SubsetArray[i].Material.Name, modelObj.SubsetArray[i].Material.Name, 255);
			strncpy(Model->SubsetArray[i].Material.TextureName, modelObj.SubsetArray[i].Material.TextureName, 255);

			if (modelObj.SubsetArray[i].Material.Texture != nullptr) {
				Model->SubsetArray[i].Material.Texture = modelObj.SubsetArray[i].Material.Texture;
			} else if (modelObj.SubsetArray[i].Material.TextureName[0] != '\0') {
				Model->SubsetArray[i].Material.Texture = Texture::Load(modelObj.SubsetArray[i].Material.TextureName);
			}

			if (Model->SubsetArray[i].Material.Texture)
				Model->SubsetArray[i].Material.Material.TextureEnable = true;
			else
				Model->SubsetArray[i].Material.Material.TextureEnable = false;
		}
	}

	Model->BaseVertices.assign(modelObj.VertexArray, modelObj.VertexArray + modelObj.VertexNum);
	Model->BaseIndices.assign(modelObj.IndexArray, modelObj.IndexArray + modelObj.IndexNum);
	Model->AppliedFlipWinding = false;
	Model->AppliedFlipNormals = false;

	delete[] modelObj.VertexArray;
	delete[] modelObj.IndexArray;
	delete[] modelObj.SubsetArray;
}

void ModelRenderer::ApplyFlipSettings()
{
	if (!m_Model || !m_Model->VertexBuffer || !m_Model->IndexBuffer) return;
	if (m_Model->BaseVertices.empty() || m_Model->BaseIndices.empty()) return;

	if (m_Model->AppliedFlipNormals != m_FlipNormals) {
		std::vector<Vertex> verts = m_Model->BaseVertices;
		if (m_FlipNormals) {
			for (auto& v : verts) {
				v.normal.x = -v.normal.x;
				v.normal.y = -v.normal.y;
				v.normal.z = -v.normal.z;
			}
		}
		void* data = nullptr;
		if (SUCCEEDED(m_Model->VertexBuffer->Resource->Map(0, nullptr, &data))) {
			memcpy(data, verts.data(), sizeof(Vertex) * verts.size());
			m_Model->VertexBuffer->Resource->Unmap(0, nullptr);
		}
		m_Model->AppliedFlipNormals = m_FlipNormals;
	}

	if (m_Model->AppliedFlipWinding != m_FlipWinding) {
		std::vector<unsigned int> idxs = m_Model->BaseIndices;
		if (m_FlipWinding) {
			for (size_t i = 0; i + 2 < idxs.size(); i += 3) {
				std::swap(idxs[i + 1], idxs[i + 2]);
			}
		}
		void* data = nullptr;
		if (SUCCEEDED(m_Model->IndexBuffer->Resource->Map(0, nullptr, &data))) {
			memcpy(data, idxs.data(), sizeof(unsigned int) * idxs.size());
			m_Model->IndexBuffer->Resource->Unmap(0, nullptr);
		}
		m_Model->AppliedFlipWinding = m_FlipWinding;
	}
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




//}eAǁE//////////////////////////////////////////////////////////////////
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


void ModelRenderer::LoadFbx(const char* FileName, MODEL_OBJ* ModelObj)
{
	ModelObj->VertexArray = nullptr;
	ModelObj->VertexNum = 0;
	ModelObj->IndexArray = nullptr;
	ModelObj->IndexNum = 0;
	ModelObj->SubsetArray = nullptr;
	ModelObj->SubsetNum = 0;

	if (!FileName || strlen(FileName) == 0) return;

	ufbx_load_opts opts = {};
	opts.target_axes = ufbx_axes_left_handed_y_up;
	opts.target_unit_meters = 1.0f;
	opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
	opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
	opts.generate_missing_normals = true;
	opts.normalize_normals = true;
	opts.reverse_winding = true;
	opts.use_blender_pbr_material = true;
	opts.ignore_missing_external_files = true;

	ufbx_error error;
	ufbx_scene* scene = ufbx_load_file(FileName, &opts, &error);
	if (!scene) {
		OutputDebugStringA("Failed to load FBX file: ");
		OutputDebugStringA(FileName);
		OutputDebugStringA(" - ");
		OutputDebugStringA(error.description.data);
		OutputDebugStringA("\n");
		return;
	}

	std::filesystem::path fbxPath(FileName);
	std::filesystem::path fbxDir = fbxPath.parent_path();

	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<SUBSET> subsets;

	for (size_t nodeIdx = 0; nodeIdx < scene->nodes.count; nodeIdx++) {
		ufbx_node* node = scene->nodes.data[nodeIdx];
		if (!node || !node->mesh || node->is_geometry_transform_helper) continue;

		ufbx_mesh* mesh = node->mesh;
		ufbx_matrix geomToWorld = node->geometry_to_world;
		ufbx_matrix normalMatrix = ufbx_matrix_for_normals(&geomToWorld);

		// Compute bounding box center of this mesh in world space to auto-detect inward vs outward normals
		XMFLOAT3 minPos = { 1e30f, 1e30f, 1e30f };
		XMFLOAT3 maxPos = { -1e30f, -1e30f, -1e30f };
		for (size_t vi = 0; vi < mesh->num_vertices; vi++) {
			ufbx_vec3 wp = ufbx_transform_position(&geomToWorld, mesh->vertices.data[vi]);
			if ((float)wp.x < minPos.x) minPos.x = (float)wp.x;
			if ((float)wp.y < minPos.y) minPos.y = (float)wp.y;
			if ((float)wp.z < minPos.z) minPos.z = (float)wp.z;
			if ((float)wp.x > maxPos.x) maxPos.x = (float)wp.x;
			if ((float)wp.y > maxPos.y) maxPos.y = (float)wp.y;
			if ((float)wp.z > maxPos.z) maxPos.z = (float)wp.z;
		}
		XMFLOAT3 meshCenter = {
			0.5f * (minPos.x + maxPos.x),
			0.5f * (minPos.y + maxPos.y),
			0.5f * (minPos.z + maxPos.z)
		};

		// Check whether mesh normals point outward or inward relative to meshCenter
		double normalOutwardSum = 0.0;
		if (mesh->vertex_normal.exists) {
			for (size_t idx = 0; idx < mesh->num_indices; idx++) {
				ufbx_vec3 p = ufbx_transform_position(&geomToWorld, ufbx_get_vertex_vec3(&mesh->vertex_position, idx));
				ufbx_vec3 n = ufbx_transform_direction(&normalMatrix, ufbx_get_vertex_vec3(&mesh->vertex_normal, idx));
				double nl = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
				if (nl > 1e-6) {
					n.x /= nl; n.y /= nl; n.z /= nl;
				}
				normalOutwardSum += (p.x - meshCenter.x) * n.x + (p.y - meshCenter.y) * n.y + (p.z - meshCenter.z) * n.z;
			}
		}
		float normalSign = (normalOutwardSum < 0.0) ? -1.0f : 1.0f;

		size_t maxTriIndices = mesh->max_face_triangles * 3;
		if (maxTriIndices < 3) maxTriIndices = 3;
		std::vector<uint32_t> triIndices(maxTriIndices);

		for (size_t partIdx = 0; partIdx < mesh->material_parts.count; partIdx++) {
			const ufbx_mesh_part& part = mesh->material_parts.data[partIdx];
			if (part.num_triangles == 0) continue;

			SUBSET subset = {};
			subset.StartIndex = (unsigned int)indices.size();
			subset.Material.Material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
			subset.Material.Material.BaseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			subset.Material.Material.Ambient = { 1.0f, 1.0f, 1.0f, 1.0f };
			subset.Material.Material.Specular = { 0.5f, 0.5f, 0.5f, 1.0f };
			subset.Material.Material.Emission = { 0.0f, 0.0f, 0.0f, 0.0f };
			subset.Material.Material.Shininess = 1.0f;
			subset.Material.Material.Roughness = 0.5f;
			subset.Material.Material.SpecularPbr = 0.5f;
			subset.Material.Texture = nullptr;
			subset.Material.TextureName[0] = '\0';
			subset.Material.Name[0] = '\0';

			const ufbx_material* mat = nullptr;
			if (part.index < node->materials.count) {
				mat = node->materials.data[part.index];
			} else if (part.index < mesh->materials.count) {
				mat = mesh->materials.data[part.index];
			}

			if (mat) {
				if (mat->name.length > 0) {
					strncpy(subset.Material.Name, mat->name.data, 255);
					subset.Material.Name[255] = '\0';
				}

				// Extract BaseColor / Diffuse
				if (mat->pbr.base_color.has_value) {
					ufbx_vec4 c = mat->pbr.base_color.value_vec4;
					float factor = mat->pbr.base_factor.has_value ? (float)mat->pbr.base_factor.value_real : 1.0f;
					float alpha = (mat->pbr.base_color.value_components >= 4) ? (float)c.w : 1.0f;
					subset.Material.Material.Diffuse = { (float)c.x * factor, (float)c.y * factor, (float)c.z * factor, alpha };
					subset.Material.Material.BaseColor = subset.Material.Material.Diffuse;
				} else if (mat->fbx.diffuse_color.has_value) {
					ufbx_vec3 c = mat->fbx.diffuse_color.value_vec3;
					float factor = mat->fbx.diffuse_factor.has_value ? (float)mat->fbx.diffuse_factor.value_real : 1.0f;
					subset.Material.Material.Diffuse = { (float)c.x * factor, (float)c.y * factor, (float)c.z * factor, 1.0f };
					subset.Material.Material.BaseColor = subset.Material.Material.Diffuse;
				}

				// Extract Texture (BaseColor or Diffuse)
				const ufbx_texture* ufbxTex = mat->pbr.base_color.texture;
				if (!ufbxTex) {
					ufbxTex = mat->fbx.diffuse_color.texture;
				}
				if (!ufbxTex && mat->textures.count > 0) {
					for (size_t ti = 0; ti < mat->textures.count; ti++) {
						const ufbx_material_texture& mt = mat->textures.data[ti];
						std::string propName(mt.material_prop.data, mt.material_prop.length);
						if (propName.find("Diffuse") != std::string::npos || propName.find("BaseColor") != std::string::npos || propName.find("base_color") != std::string::npos) {
							ufbxTex = mt.texture;
							break;
						}
					}
				}
				if (ufbxTex && ufbxTex->type != UFBX_TEXTURE_FILE && ufbxTex->file_textures.count > 0) {
					ufbxTex = ufbxTex->file_textures.data[0];
				}

				if (ufbxTex) {
					ufbx_blob embeddedContent = ufbxTex->content;
					if ((embeddedContent.size == 0 || !embeddedContent.data) && ufbxTex->has_file && ufbxTex->file_index < scene->texture_files.count) {
						embeddedContent = scene->texture_files.data[ufbxTex->file_index].content;
					}

					if (embeddedContent.size > 0 && embeddedContent.data != nullptr) {
						std::string memKey = std::string(FileName) + "#tex_" + std::to_string(ufbxTex->typed_id);
						subset.Material.Texture = Texture::LoadFromMemory(memKey, embeddedContent.data, embeddedContent.size);
						strncpy(subset.Material.TextureName, memKey.c_str(), 255);
						subset.Material.TextureName[255] = '\0';
					} else {
						std::string resolvedTexPath = "";
						std::vector<std::filesystem::path> candidates;
						if (ufbxTex->filename.length > 0) {
							std::filesystem::path p(std::string(ufbxTex->filename.data, ufbxTex->filename.length));
							candidates.push_back(p);
							candidates.push_back(fbxDir / p.filename());
						}
						if (ufbxTex->relative_filename.length > 0) {
							std::filesystem::path p(std::string(ufbxTex->relative_filename.data, ufbxTex->relative_filename.length));
							candidates.push_back(fbxDir / p);
							candidates.push_back(fbxDir / p.filename());
						}
						if (ufbxTex->absolute_filename.length > 0) {
							std::filesystem::path p(std::string(ufbxTex->absolute_filename.data, ufbxTex->absolute_filename.length));
							candidates.push_back(fbxDir / p.filename());
							candidates.push_back(p);
						}

						for (const auto& cand : candidates) {
							std::error_code ec;
							if (std::filesystem::is_regular_file(cand, ec)) {
								resolvedTexPath = cand.generic_string();
								break;
							}
						}
						if (resolvedTexPath.empty() && !candidates.empty()) {
							resolvedTexPath = candidates[0].generic_string();
						}
						if (!resolvedTexPath.empty()) {
							strncpy(subset.Material.TextureName, resolvedTexPath.c_str(), 255);
							subset.Material.TextureName[255] = '\0';
						}
					}
				}
			}

			for (size_t f = 0; f < part.num_faces; f++) {
				ufbx_face face = mesh->faces.data[part.face_indices.data[f]];
				if (face.num_indices < 3) continue;

				uint32_t numTris = ufbx_triangulate_face(triIndices.data(), triIndices.size(), mesh, face);
				for (uint32_t t = 0; t < numTris; t++) {
					Vertex triVerts[3];
					for (int corner = 0; corner < 3; corner++) {
						uint32_t idx = triIndices[t * 3 + corner];

						ufbx_vec3 pos = ufbx_get_vertex_vec3(&mesh->vertex_position, idx);
						pos = ufbx_transform_position(&geomToWorld, pos);

						ufbx_vec3 norm = { 0.0f, 1.0f, 0.0f };
						if (mesh->vertex_normal.exists) {
							norm = ufbx_get_vertex_vec3(&mesh->vertex_normal, idx);
							norm = ufbx_transform_direction(&normalMatrix, norm);
							float len = std::sqrt((float)(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z));
							if (len > 1e-6f) {
								norm.x = (norm.x / len) * normalSign;
								norm.y = (norm.y / len) * normalSign;
								norm.z = (norm.z / len) * normalSign;
							}
						}

						ufbx_vec2 uv = { 0.0f, 0.0f };
						if (mesh->vertex_uv.exists) {
							uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, idx);
							uv.y = 1.0f - (float)uv.y;
						}

						ufbx_vec4 col = { 1.0f, 1.0f, 1.0f, 1.0f };
						if (mesh->vertex_color.exists) {
							col = ufbx_get_vertex_vec4(&mesh->vertex_color, idx);
						}

						triVerts[corner].position = XMFLOAT3((float)pos.x, (float)pos.y, (float)pos.z);
						triVerts[corner].normal = XMFLOAT3((float)norm.x, (float)norm.y, (float)norm.z);
						triVerts[corner].uv = XMFLOAT2((float)uv.x, (float)uv.y);
						triVerts[corner].color = XMFLOAT4((float)col.x, (float)col.y, (float)col.z, (float)col.w);
					}

					// Auto-align triangle winding to CW (FrontCounterClockwise = FALSE) relative to outward normal
					XMFLOAT3 e1 = {
						triVerts[1].position.x - triVerts[0].position.x,
						triVerts[1].position.y - triVerts[0].position.y,
						triVerts[1].position.z - triVerts[0].position.z
					};
					XMFLOAT3 e2 = {
						triVerts[2].position.x - triVerts[0].position.x,
						triVerts[2].position.y - triVerts[0].position.y,
						triVerts[2].position.z - triVerts[0].position.z
					};
					XMFLOAT3 faceCross = {
						e1.y * e2.z - e1.z * e2.y,
						e1.z * e2.x - e1.x * e2.z,
						e1.x * e2.y - e1.y * e2.x
					};
					XMFLOAT3 avgNorm = {
						triVerts[0].normal.x + triVerts[1].normal.x + triVerts[2].normal.x,
						triVerts[0].normal.y + triVerts[1].normal.y + triVerts[2].normal.y,
						triVerts[0].normal.z + triVerts[1].normal.z + triVerts[2].normal.z
					};
					float windingDot = faceCross.x * avgNorm.x + faceCross.y * avgNorm.y + faceCross.z * avgNorm.z;

					unsigned int baseVertIdx = (unsigned int)vertices.size();
					vertices.push_back(triVerts[0]);
					vertices.push_back(triVerts[1]);
					vertices.push_back(triVerts[2]);

					if (windingDot < 0.0f) {
						indices.push_back(baseVertIdx + 0);
						indices.push_back(baseVertIdx + 2);
						indices.push_back(baseVertIdx + 1);
					} else {
						indices.push_back(baseVertIdx + 0);
						indices.push_back(baseVertIdx + 1);
						indices.push_back(baseVertIdx + 2);
					}
				}
			}

			subset.IndexNum = (unsigned int)indices.size() - subset.StartIndex;
			if (subset.IndexNum > 0) {
				subsets.push_back(subset);
			}
		}
	}

	ufbx_free_scene(scene);

	if (vertices.empty() || indices.empty()) {
		return;
	}

	ModelObj->VertexNum = (unsigned int)vertices.size();
	ModelObj->VertexArray = new Vertex[ModelObj->VertexNum];
	memcpy(ModelObj->VertexArray, vertices.data(), sizeof(Vertex) * ModelObj->VertexNum);

	ModelObj->IndexNum = (unsigned int)indices.size();
	ModelObj->IndexArray = new unsigned int[ModelObj->IndexNum];
	memcpy(ModelObj->IndexArray, indices.data(), sizeof(unsigned int) * ModelObj->IndexNum);

	ModelObj->SubsetNum = (unsigned int)subsets.size();
	ModelObj->SubsetArray = new SUBSET[ModelObj->SubsetNum];
	memcpy(ModelObj->SubsetArray, subsets.data(), sizeof(SUBSET) * ModelObj->SubsetNum);
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
			for (const auto& entry : std::filesystem::recursive_directory_iterator(modelDir)) {
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

	ImGui::Spacing();
	ImGui::Text("Culling & Winding Options:");
	const char* cullItems[] = { "Back (Default)", "Front (Inverted)", "None (Double-Sided)" };
	ImGui::Combo("Cull Mode", &m_CullMode, cullItems, IM_ARRAYSIZE(cullItems));

	if (ImGui::Checkbox("Flip Winding (Indices)", &m_FlipWinding)) {
		ApplyFlipSettings();
	}
	if (ImGui::Checkbox("Flip Normals", &m_FlipNormals)) {
		ApplyFlipSettings();
	}
	if (ImGui::Button("Invert Both (Winding & Normals)")) {
		m_FlipWinding = !m_FlipWinding;
		m_FlipNormals = !m_FlipNormals;
		ApplyFlipSettings();
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
