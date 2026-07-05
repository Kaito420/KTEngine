//=====================================================================================
// ModelRenderer.h
// Author:Kaito Aoki
// Date:2025/09/14
//=====================================================================================

#ifndef _MODELRENDERER_H_
#define _MODELRENDERER_H_

#include "Renderer.h"

// マテリアル構造体
struct MODEL_MATERIAL
{
	char						Name[256];
	MATERIAL					Material;
	char						TextureName[256];
	const TEXTURE* Texture;

};


// サブセット構造体
struct SUBSET
{
	unsigned int	StartIndex;
	unsigned int	IndexNum;
	MODEL_MATERIAL	Material;
};


// オブジェクトデータ構造体
struct MODEL_OBJ
{
	Vertex* VertexArray;
	unsigned int	VertexNum;

	unsigned int* IndexArray;
	unsigned int	IndexNum;

	SUBSET* SubsetArray;
	unsigned int	SubsetNum;
};

struct MODEL
{
	std::unique_ptr<VERTEX_BUFFER> VertexBuffer;
	std::unique_ptr<INDEX_BUFFER> IndexBuffer;

	SUBSET* SubsetArray;
	unsigned int	SubsetNum;
};


#include "Component.h"
#include <string>
#include <unordered_map>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>


class ModelRenderer : public Component{
	friend class cereal::access;
private:

	static std::unordered_map<std::string, MODEL*> m_ModelPool;

	static void LoadModel(const char* FileName, MODEL* Model);
	static void LoadObj(const char* FileName, MODEL_OBJ* ModelObj);
	static void LoadMaterial(const char* FileName, MODEL_MATERIAL** MaterialArray, unsigned int* MaterialNum);

	MODEL* m_Model{};

public:

	static void Preload(const char* FileName);
	static void UnloadAll();

	void Awake() override;
	void Load(const char* FileName);
	void Render()const override;
	void ShowUI() override;
	std::string GetLoadedFileName() const;
	std::string GetComponentName() override { return "ModelRenderer"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		std::string filename = "";
		if (m_Model) {
			for (const auto& pair : m_ModelPool) {
				if (pair.second == m_Model) {
					filename = pair.first;
					break;
				}
			}
		}
		ar(cereal::make_nvp("FileName", filename));

		if (!filename.empty()) {
			Load(filename.c_str());
		}
	}

};

#endif // !_MODELRENDERER_H_