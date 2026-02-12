//=====================================================================================
// SceneTitle.h
// Author:Kaito Aoki
// Date:2025/07/04
//=====================================================================================
#ifndef _SCENETITLE_H
#define _SCENETITLE_H
#include "Scene.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class SceneTitle : public Scene {
	friend class cereal::access;
public:
	void Initialize() override;

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Scene>(this));
	}
};

#endif // !_SCENETITLE_H