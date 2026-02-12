//=====================================================================================
// DirectXSerializer.h
// Author:Kaito Aoki
// Date:2026/02/12
//=====================================================================================

#ifndef _DIRECTXSERIALIZER_H_
#define _DIRECTXSERIALIZER_H_

#include <cereal/cereal.hpp>
#include <DirectXMath.h>
#include "RendererDX11.h"

//DirectXMathの型のシリアライズ関数
namespace DirectX {
	//XMFLOAT2
	template <class Archive>
	void serialize(Archive& ar, XMFLOAT2& v) {
		ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y));
	}

	//XMFLOAT3
	template <class Archive>
	void serialize(Archive& ar, XMFLOAT3& v) {
		ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z));
	}

	//XMFLOAT4
	template <class Archive>
	void serialize(Archive& ar,XMFLOAT4& v) {
		ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z), cereal::make_nvp("w", v.w));
	}
}

//RendererDX11の構造体シリアライズ関数

//MATERIAL
template <class Archive>
void serialize(Archive& ar, MATERIAL& m) {
	ar(cereal::make_nvp("Ambient", m.Ambient),
		cereal::make_nvp("Diffuse", m.Diffuse),
		cereal::make_nvp("Specular", m.Specular),
		cereal::make_nvp("Emission", m.Emission),
		cereal::make_nvp("Shininess", m.Shininess),
		cereal::make_nvp("TextureEnable", m.TextureEnable));
}

//LIGHT
template <class Archive>
void serialize(Archive& ar, LIGHT& l) {
	ar(cereal::make_nvp("Enable", l.Enable),
		cereal::make_nvp("Direction", l.Direction),
		cereal::make_nvp("Diffuse", l.Diffuse),
		cereal::make_nvp("Ambient", l.Ambient),
		cereal::make_nvp("Position", l.Position),
		cereal::make_nvp("Parameter", l.Parameter));
}

#endif // !_DIRECTXSERIALIZER_H_