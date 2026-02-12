//=====================================================================================
// ModelRenderer.h
// Author:Kaito Aoki
// Date:2025/09/14
//=====================================================================================

#ifndef _MODELRENDERER_H_
#define _MODELRENDERER_H_

// マテリアル構造体
struct MODEL_MATERIAL
{
	char						Name[256];
	MATERIAL					Material;
	char						TextureName[256];
	ID3D11ShaderResourceView* Texture;

};


// 描画サブセット構造体
struct SUBSET
{
	unsigned int	StartIndex;
	unsigned int	IndexNum;
	MODEL_MATERIAL	Material;
};


// モデル構造体
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
	ID3D11Buffer* VertexBuffer;
	ID3D11Buffer* IndexBuffer;

	SUBSET* SubsetArray;
	unsigned int	SubsetNum;
};


#include "Component.h"
#include "RendererDX11.h"
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


	using Component::Component;

	void Load(const char* FileName);
	void Render()const override;
	std::string GetComponentName() override { return "ModelRenderer"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		std::string fileName;
		if (Archive::is_saving::value) {
			//セーブ時の処理
			for (const auto& pair : m_ModelPool) {
				if (pair.second == m_Model) {//モデルが見つかった場合そのファイルネームを保存
					fileName = pair.first;
					break;
				}
			}
		}
		ar(cereal::make_nvp("FileName", fileName));
		if (Archive::is_loading::value) {
			//ロード時の処理
			Load(fileName.c_str());
		}
	}
};

#endif // !_MODELRENDERER_H_