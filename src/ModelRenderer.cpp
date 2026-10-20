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
#include "Animator.h"


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
		if (m_Model->HasSkinning) {
			UpdateBoneMatrices();
			Renderer::SetBoneBuffer(&m_BoneBuffer);
		} else {
			Renderer::SetBoneBuffer(nullptr);
		}
		for (unsigned int i = 0; i < m_Model->SubsetNum; i++) {
			cmdList->DrawIndexedInstanced(m_Model->SubsetArray[i].IndexNum, 1, m_Model->SubsetArray[i].StartIndex, 0, 0);
			Profiler::RecordDrawCall(m_Model->VertexBuffer ? (int)m_Model->VertexBuffer->Size : 0, (int)m_Model->SubsetArray[i].IndexNum);
		}
		Renderer::SetBoneBuffer(nullptr);
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
	if (m_Model->HasSkinning) {
		UpdateBoneMatrices();
		Renderer::SetBoneBuffer(&m_BoneBuffer);
	} else {
		Renderer::SetBoneBuffer(nullptr);
	}

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
	Renderer::SetBoneBuffer(nullptr);
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
	Model->Bones = std::move(modelObj.Bones);
	Model->Animations = std::move(modelObj.Animations);
	Model->HasSkinning = modelObj.HasSkinning;
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


static XMMATRIX UfbxMatrixToXMMatrix(const ufbx_matrix& m)
{
	return XMMATRIX(
		(float)m.m00, (float)m.m10, (float)m.m20, 0.0f,
		(float)m.m01, (float)m.m11, (float)m.m21, 0.0f,
		(float)m.m02, (float)m.m12, (float)m.m22, 0.0f,
		(float)m.m03, (float)m.m13, (float)m.m23, 1.0f
	);
}

static std::string NormalizeBoneName(const std::string& name)
{
	size_t colonPos = name.find_last_of(":|");
	if (colonPos != std::string::npos && colonPos + 1 < name.size()) {
		return name.substr(colonPos + 1);
	}
	return name;
}

static int FindBoneIndexByName(const std::vector<BoneInfo>& bones, const std::string& nodeName)
{
	if (nodeName.empty()) return -1;
	for (size_t i = 0; i < bones.size(); i++) {
		if (bones[i].Name == nodeName) return (int)i;
	}
	std::string normTarget = NormalizeBoneName(nodeName);
	for (size_t i = 0; i < bones.size(); i++) {
		if (NormalizeBoneName(bones[i].Name) == normTarget) return (int)i;
	}
	return -1;
}

static void ExtractAnimationsFromScene(
	ufbx_scene* scene,
	const char* FileName,
	const std::vector<BoneInfo>& bones,
	const std::vector<int>& nodeToBoneIndex,
	std::vector<AnimationClip>& outClips)
{
	if (!scene || bones.empty() || scene->anim_stacks.count == 0) return;

	std::string fileStem = "";
	if (FileName && strlen(FileName) > 0) {
		fileStem = std::filesystem::path(FileName).stem().string();
	}

	for (size_t stackIdx = 0; stackIdx < scene->anim_stacks.count; stackIdx++) {
		ufbx_anim_stack* stack = scene->anim_stacks.data[stackIdx];
		if (!stack || !stack->anim) continue;

		ufbx_bake_opts bakeOpts = {};
		bakeOpts.trim_start_time = true;
		ufbx_error bakeErr;
		ufbx_baked_anim* baked = ufbx_bake_anim(scene, stack->anim, &bakeOpts, &bakeErr);
		if (!baked) continue;

		AnimationClip clip;
		std::string rawStackName = (stack->name.length > 0)
			? std::string(stack->name.data, stack->name.length)
			: ("Anim_" + std::to_string(stackIdx));

		if (!fileStem.empty() && (rawStackName == "mixamo.com" || rawStackName.find("Take ") == 0)) {
			clip.Name = fileStem + " (" + rawStackName + ")";
		} else if (!fileStem.empty() && rawStackName.empty()) {
			clip.Name = fileStem;
		} else {
			clip.Name = rawStackName;
		}
		clip.SourceFile = FileName ? FileName : "";

		// Avoid adding duplicate clips from the same file
		bool alreadyExists = false;
		for (const auto& existing : outClips) {
			if (existing.Name == clip.Name && existing.SourceFile == clip.SourceFile) {
				alreadyExists = true;
				break;
			}
		}
		if (alreadyExists) {
			ufbx_free_baked_anim(baked);
			continue;
		}

		float duration = (float)baked->playback_duration;
		if (duration <= 0.0f && baked->key_time_max > baked->key_time_min) {
			duration = (float)(baked->key_time_max - baked->key_time_min);
		}
		clip.Duration = (duration > 0.0f) ? duration : 0.0f;

		for (size_t bni = 0; bni < baked->nodes.count; bni++) {
			const ufbx_baked_node& bn = baked->nodes.data[bni];
			int boneIdx = -1;
			if (bn.typed_id < nodeToBoneIndex.size()) {
				boneIdx = nodeToBoneIndex[bn.typed_id];
			}
			if (boneIdx < 0 && bn.typed_id < scene->nodes.count) {
				ufbx_node* uNode = scene->nodes.data[bn.typed_id];
				if (uNode && uNode->name.length > 0) {
					boneIdx = FindBoneIndexByName(bones, std::string(uNode->name.data, uNode->name.length));
				}
			}
			if (boneIdx < 0 || boneIdx >= (int)bones.size()) continue;

			BoneAnimationTrack track;
			track.BoneIndex = boneIdx;
			track.BoneName = bones[boneIdx].Name;

			track.Translations.reserve(bn.translation_keys.count);
			for (size_t k = 0; k < bn.translation_keys.count; k++) {
				const ufbx_baked_vec3& key = bn.translation_keys.data[k];
				KeyframeVec3 kf;
				kf.Time = (float)key.time;
				kf.Value = XMFLOAT3((float)key.value.x, (float)key.value.y, (float)key.value.z);
				track.Translations.push_back(kf);
				if (kf.Time > clip.Duration) clip.Duration = kf.Time;
			}

			track.Rotations.reserve(bn.rotation_keys.count);
			for (size_t k = 0; k < bn.rotation_keys.count; k++) {
				const ufbx_baked_quat& key = bn.rotation_keys.data[k];
				KeyframeQuat kf;
				kf.Time = (float)key.time;
				kf.Value = XMFLOAT4((float)key.value.x, (float)key.value.y, (float)key.value.z, (float)key.value.w);
				track.Rotations.push_back(kf);
				if (kf.Time > clip.Duration) clip.Duration = kf.Time;
			}

			track.Scales.reserve(bn.scale_keys.count);
			for (size_t k = 0; k < bn.scale_keys.count; k++) {
				const ufbx_baked_vec3& key = bn.scale_keys.data[k];
				KeyframeVec3 kf;
				kf.Time = (float)key.time;
				kf.Value = XMFLOAT3((float)key.value.x, (float)key.value.y, (float)key.value.z);
				track.Scales.push_back(kf);
				if (kf.Time > clip.Duration) clip.Duration = kf.Time;
			}

			clip.Tracks.push_back(std::move(track));
		}

		ufbx_free_baked_anim(baked);
		if (!clip.Tracks.empty()) {
			outClips.push_back(std::move(clip));
		}
	}
}

bool ModelRenderer::LoadAnimationFromFbx(const char* FileName, MODEL* TargetModel)
{
	if (!FileName || strlen(FileName) == 0 || !TargetModel || TargetModel->Bones.empty()) return false;

	ufbx_load_opts opts = {};
	opts.target_axes = ufbx_axes_left_handed_y_up;
	opts.target_unit_meters = 1.0f;
	opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
	opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
	opts.ignore_geometry = true;
	opts.ignore_missing_external_files = true;

	ufbx_error error;
	ufbx_scene* scene = ufbx_load_file(FileName, &opts, &error);
	if (!scene) return false;

	std::vector<int> nodeToBoneIndex(scene->nodes.count, -1);
	for (size_t ni = 0; ni < scene->nodes.count; ni++) {
		ufbx_node* node = scene->nodes.data[ni];
		if (!node) continue;
		std::string nodeName = (node->name.length > 0)
			? std::string(node->name.data, node->name.length)
			: (node->is_root ? "Root" : "");
		nodeToBoneIndex[node->typed_id] = FindBoneIndexByName(TargetModel->Bones, nodeName);
	}

	size_t beforeCount = TargetModel->Animations.size();
	ExtractAnimationsFromScene(scene, FileName, TargetModel->Bones, nodeToBoneIndex, TargetModel->Animations);
	ufbx_free_scene(scene);
	return TargetModel->Animations.size() > beforeCount;
}

void ModelRenderer::UpdateBoneMatrices() const
{
	if (!m_Model || !m_Model->HasSkinning || m_Model->Bones.empty()) {
		m_BoneBuffer.HasSkinning = 0;
		return;
	}

	if (!_owner || !_owner->GetComponent<Animator>()) {
		m_UseAnimatedBones = false;
	}

	m_BoneBuffer.HasSkinning = 1;
	size_t numBones = m_Model->Bones.size();
	if (numBones > MAX_BONES) numBones = MAX_BONES;

	std::vector<XMMATRIX> globalMatrices(numBones, XMMatrixIdentity());
	for (size_t i = 0; i < numBones; i++) {
		XMMATRIX localMat = (m_UseAnimatedBones && i < m_AnimatedLocalTransforms.size())
			? XMLoadFloat4x4(&m_AnimatedLocalTransforms[i])
			: XMLoadFloat4x4(&m_Model->Bones[i].DefaultLocalTransform);
		if ((int)i == m_TestBoneIndex && (m_TestBoneRotation.x != 0.0f || m_TestBoneRotation.y != 0.0f || m_TestBoneRotation.z != 0.0f)) {
			XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(
				XMConvertToRadians(m_TestBoneRotation.x),
				XMConvertToRadians(m_TestBoneRotation.y),
				XMConvertToRadians(m_TestBoneRotation.z)
			);
			localMat = rotMat * localMat;
		}

		int parentIdx = m_Model->Bones[i].ParentIndex;
		if (parentIdx >= 0 && parentIdx < (int)i) {
			globalMatrices[i] = localMat * globalMatrices[parentIdx];
		} else {
			globalMatrices[i] = localMat;
		}

		XMMATRIX invBind = XMLoadFloat4x4(&m_Model->Bones[i].InverseBindMatrix);
		XMMATRIX finalMat = invBind * globalMatrices[i];
		XMStoreFloat4x4(&m_BoneBuffer.BoneMatrices[i], XMMatrixTranspose(finalMat));
	}

	for (size_t i = numBones; i < MAX_BONES; i++) {
		XMStoreFloat4x4(&m_BoneBuffer.BoneMatrices[i], XMMatrixIdentity());
	}
}

void ModelRenderer::LoadFbx(const char* FileName, MODEL_OBJ* ModelObj)
{
	ModelObj->VertexArray = nullptr;
	ModelObj->VertexNum = 0;
	ModelObj->IndexArray = nullptr;
	ModelObj->IndexNum = 0;
	ModelObj->SubsetArray = nullptr;
	ModelObj->SubsetNum = 0;
	ModelObj->Bones.clear();
	ModelObj->Animations.clear();
	ModelObj->HasSkinning = false;

	if (!FileName || strlen(FileName) == 0) return;

	ufbx_load_opts opts = {};
	opts.target_axes = ufbx_axes_left_handed_y_up;
	opts.target_unit_meters = 1.0f;
	opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
	opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
	opts.generate_missing_normals = true;
	opts.normalize_normals = true;
	opts.reverse_winding = true;
	opts.clean_skin_weights = true;
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

	// Build bone hierarchy (parent-first topological order via DFS from root_node)
	std::vector<int> nodeToBoneIndex(scene->nodes.count, -1);
	if (scene->skin_deformers.count > 0 && scene->root_node) {
		std::vector<ufbx_node*> stack;
		stack.push_back(scene->root_node);
		while (!stack.empty() && ModelObj->Bones.size() < MAX_BONES) {
			ufbx_node* cur = stack.back();
			stack.pop_back();

			int boneIdx = (int)ModelObj->Bones.size();
			nodeToBoneIndex[cur->typed_id] = boneIdx;

			BoneInfo bi;
			if (cur->name.length > 0) {
				bi.Name = std::string(cur->name.data, cur->name.length);
			} else {
				bi.Name = cur->is_root ? "Root" : ("Node_" + std::to_string(cur->typed_id));
			}
			bi.ParentIndex = (cur->parent && cur->parent->typed_id < nodeToBoneIndex.size())
				? nodeToBoneIndex[cur->parent->typed_id]
				: -1;
			XMStoreFloat4x4(&bi.DefaultLocalTransform, UfbxMatrixToXMMatrix(cur->node_to_parent));
			XMStoreFloat4x4(&bi.InverseBindMatrix, XMMatrixIdentity());
			ModelObj->Bones.push_back(bi);

			// Push children in reverse order so they are popped in forward order
			for (size_t ci = cur->children.count; ci > 0; ci--) {
				stack.push_back(cur->children.data[ci - 1]);
			}
		}
	}

	// Extract FBX animation clips
	if (!ModelObj->Bones.empty() && scene->anim_stacks.count > 0) {
		ExtractAnimationsFromScene(scene, FileName, ModelObj->Bones, nodeToBoneIndex, ModelObj->Animations);
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

		// Extract skinning cluster mappings and inverse bind matrices for this mesh
		ufbx_skin_deformer* skin = (mesh->skin_deformers.count > 0) ? mesh->skin_deformers.data[0] : nullptr;
		std::vector<int> clusterToBoneIndex;
		if (skin && !ModelObj->Bones.empty()) {
			ufbx_matrix invGeomToWorld = ufbx_matrix_invert(&geomToWorld);
			clusterToBoneIndex.assign(skin->clusters.count, -1);
			for (size_t ci = 0; ci < skin->clusters.count; ci++) {
				ufbx_skin_cluster* cluster = skin->clusters.data[ci];
				if (cluster && cluster->bone_node && cluster->bone_node->typed_id < nodeToBoneIndex.size()) {
					int bIdx = nodeToBoneIndex[cluster->bone_node->typed_id];
					if (bIdx >= 0 && bIdx < (int)ModelObj->Bones.size()) {
						clusterToBoneIndex[ci] = bIdx;
						ufbx_matrix invBind = ufbx_matrix_mul(&cluster->geometry_to_bone, &invGeomToWorld);
						XMStoreFloat4x4(&ModelObj->Bones[bIdx].InverseBindMatrix, UfbxMatrixToXMMatrix(invBind));
						ModelObj->HasSkinning = true;
					}
				}
			}
		}

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

						// Extract up to 4 skinning weights per vertex
						if (skin && idx < mesh->vertex_indices.count) {
							uint32_t vertexIdx = mesh->vertex_indices.data[idx];
							if (vertexIdx < skin->vertices.count) {
								ufbx_skin_vertex sv = skin->vertices.data[vertexIdx];
								float weightSum = 0.0f;
								int assigned = 0;
								for (uint32_t wi = 0; wi < sv.num_weights && assigned < 4; wi++) {
									ufbx_skin_weight sw = skin->weights.data[sv.weight_begin + wi];
									if (sw.cluster_index < clusterToBoneIndex.size()) {
										int boneIdx = clusterToBoneIndex[sw.cluster_index];
										if (boneIdx >= 0 && sw.weight > 0.0f) {
											triVerts[corner].boneIndices[assigned] = (uint32_t)boneIdx;
											triVerts[corner].boneWeights[assigned] = (float)sw.weight;
											weightSum += (float)sw.weight;
											assigned++;
										}
									}
								}
								if (weightSum > 1e-6f) {
									for (int wi = 0; wi < 4; wi++) {
										triVerts[corner].boneWeights[wi] /= weightSum;
									}
								}
							}
						}
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
		ImGui::Text(" - Skinning: %s (%zu Bones)", m_Model->HasSkinning ? "Enabled" : "None", m_Model->Bones.size());
		ImGui::Text(" - Animations: %zu Clips", m_Model->Animations.size());
		if ((m_Model->HasSkinning || !m_Model->Animations.empty()) && _owner && !_owner->GetComponent<Animator>()) {
			if (ImGui::Button("Add Animator Component")) {
				_owner->AddComponent<Animator>();
			}
		}

		if (m_Model->HasSkinning && !m_Model->Bones.empty()) {
			ImGui::Spacing();
			if (ImGui::TreeNode("Skeleton & Bone Skinning Test")) {
				if (m_TestBoneIndex < 0 || m_TestBoneIndex >= (int)m_Model->Bones.size()) {
					m_TestBoneIndex = 0;
				}
				const char* currentBoneName = m_Model->Bones[m_TestBoneIndex].Name.c_str();
				if (ImGui::BeginCombo("Select Bone", currentBoneName)) {
					for (int bi = 0; bi < (int)m_Model->Bones.size(); bi++) {
						bool isSelected = (m_TestBoneIndex == bi);
						std::string label = "[" + std::to_string(bi) + "] " + m_Model->Bones[bi].Name;
						if (ImGui::Selectable(label.c_str(), isSelected)) {
							m_TestBoneIndex = bi;
						}
						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				ImGui::DragFloat3("Bone Rot (Deg)", &m_TestBoneRotation.x, 1.0f, -180.0f, 180.0f);
				if (ImGui::Button("Reset Bone Rotation")) {
					m_TestBoneRotation = { 0.0f, 0.0f, 0.0f };
				}
				ImGui::TreePop();
			}
		}
	}
}
