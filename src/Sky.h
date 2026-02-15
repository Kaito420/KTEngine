//=====================================================================================
// Sky.h
// Author:Kaito Aoki
// Date:2026/02/15
//=====================================================================================

#ifndef _SKY_H
#define _SKY_H

#include "GameObject.h"
#include "Sphere.h"
#include <string>
#include <memory>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Sky : public GameObject {
	friend class cereal::access;

public:
	//std::shared_ptr<Sphere> _sphere;
	void Awake() override;
	void Update() override;
	void Render()const override;
	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<GameObject>(this));
	}
};

#endif // !_SKY_H