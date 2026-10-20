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


#include <vector>
#include <string>

// ボーン階層・逆バインド行列情報
struct BoneInfo
{
	std::string Name;
	int ParentIndex = -1;
	XMFLOAT4X4 InverseBindMatrix;
	XMFLOAT4X4 DefaultLocalTransform;
};

// キーフレーム構造体
struct KeyframeVec3
{
	float Time = 0.0f;
	XMFLOAT3 Value = { 0.0f, 0.0f, 0.0f };
};

struct KeyframeQuat
{
	float Time = 0.0f;
	XMFLOAT4 Value = { 0.0f, 0.0f, 0.0f, 1.0f };
};

// 1ボーンあたりのアニメーショントラック
struct BoneAnimationTrack
{
	int BoneIndex = -1;
	std::string BoneName;
	std::vector<KeyframeVec3> Translations;
	std::vector<KeyframeQuat> Rotations;
	std::vector<KeyframeVec3> Scales;
};

// アニメーションクリップ（1つのモーションデータ）
struct AnimationClip
{
	std::string Name;
	std::string SourceFile;
	float Duration = 0.0f;
	std::vector<BoneAnimationTrack> Tracks;
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

	std::vector<BoneInfo> Bones;
	std::vector<AnimationClip> Animations;
	bool HasSkinning = false;
};

struct MODEL
{
	std::unique_ptr<VERTEX_BUFFER> VertexBuffer;
	std::unique_ptr<INDEX_BUFFER> IndexBuffer;

	std::vector<Vertex> BaseVertices;
	std::vector<unsigned int> BaseIndices;
	bool AppliedFlipWinding = false;
	bool AppliedFlipNormals = false;

	SUBSET* SubsetArray;
	unsigned int	SubsetNum;

	std::vector<BoneInfo> Bones;
	std::vector<AnimationClip> Animations;
	bool HasSkinning = false;
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
	static void LoadFbx(const char* FileName, MODEL_OBJ* ModelObj);
	static void LoadMaterial(const char* FileName, MODEL_MATERIAL** MaterialArray, unsigned int* MaterialNum);

	MODEL* m_Model{};
	bool m_FlipWinding = false;
	bool m_FlipNormals = false;
	int m_CullMode = 0; // 0: Back (Default), 1: Front, 2: None (Double-Sided)

	mutable BONE_BUFFER m_BoneBuffer{};
	mutable bool m_UseAnimatedBones = false;
	mutable std::vector<XMFLOAT4X4> m_AnimatedLocalTransforms;
	int m_TestBoneIndex = 0;
	XMFLOAT3 m_TestBoneRotation = { 0.0f, 0.0f, 0.0f };

	void ApplyFlipSettings();
	void UpdateBoneMatrices() const;

public:

	static void Preload(const char* FileName);
	static void UnloadAll();
	static bool LoadAnimationFromFbx(const char* FileName, MODEL* TargetModel);

	void Awake() override;
	void Load(const char* FileName);
	void Render()const override;
	void ShowUI() override;
	std::string GetLoadedFileName() const;
	std::string GetComponentName() override { return "ModelRenderer"; }

	MODEL* GetModel() const { return m_Model; }
	void SetAnimatedLocalTransforms(const std::vector<XMFLOAT4X4>& localTransforms) const {
		m_AnimatedLocalTransforms = localTransforms;
		m_UseAnimatedBones = !localTransforms.empty();
	}
	void ClearAnimatedLocalTransforms() const {
		m_UseAnimatedBones = false;
		m_AnimatedLocalTransforms.clear();
	}

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
		try {
			ar(cereal::make_nvp("FlipWinding", m_FlipWinding));
			ar(cereal::make_nvp("FlipNormals", m_FlipNormals));
			ar(cereal::make_nvp("CullMode", m_CullMode));
		} catch (...) {}

		if (!filename.empty()) {
			Load(filename.c_str());
		}
	}

};

#endif // !_MODELRENDERER_H_
