//=====================================================================================
// Light.h
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#ifndef _LIGHT_H
#define _LIGHT_H

#include "GameObject.h"
#include "RendererDX11.h"
#include "DirectXSerializer.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Light : public GameObject {
	friend class cereal::access;
public:
	LIGHT _lightData;
	void Awake() override;
	void Update() override;

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<GameObject>(this));
		ar(cereal::make_nvp("LightData", _lightData));
	}
};

#endif // !_LIGHT_H