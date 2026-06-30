//=====================================================================================
// Square.h
// Author:Kaito Aoki
// Date:2025/07/15
//=====================================================================================

#ifndef _SQUARE_H
#define _SQUARE_H


#include "Component.h"
#include "Renderer.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class Square : public Component{
	friend class cereal::access;
private:


public:
	std::unique_ptr<VERTEX_BUFFER> _vertexBuffer;
	const TEXTURE* _texture = nullptr;

	void Awake() override;

	void Update() override;

	void Render()const override;

	std::string GetComponentName()override { return "Square"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
	}
};


#endif //!_SQUARE_H
