//=====================================================================================
// ScenePhysicsTest.h
// Author:Kaito Aoki
// Date:2026/02/02
//=====================================================================================

#ifndef _SCENEPHYSICSTEST_H
#define _SCENEPHYSICSTEST_H

#include "Scene.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class ScenePhysicsTest : public Scene {
	friend class cereal::access;
public:
	void Initialize() override;
	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Scene>(this));
	}
};

#endif // !_SCENEPHYSICSTEST_H
