//=====================================================================================
// SceneGame.h
// Author:Kaito Aoki
// Date:2025/09/14
//=====================================================================================

#ifndef _SCENEGAME_H
#define _SCENEGAME_H

#include "Scene.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class SceneGame : public Scene {
	friend class cereal::access;
public:
	void Initialize() override;

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Scene>(this));
	}
};

#endif // !_SCENEGAME_H