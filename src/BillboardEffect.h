#pragma once

#include "Component.h"
#include "RendererDX11.h"
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>

class BillboardEffect : public Component {
	friend class cereal::access;

	int _numXCut = 1;
	int _numYCut = 1;
	int _frame = 0;
	int _maxFrame = 0;
	bool _loop = false;
	bool _anim = true;
	ComPtr<ID3D11Buffer> _vertexBuffer;
	ID3D11ShaderResourceView* _texture = nullptr;
public:
	void Awake() override;
	void Update() override;
	void Render() const override;

	void SetEffect(const char* fileName, int xCut, int yCut, bool loop);
	std::string GetComponentName() override { return "BillboardEffect"; }

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		ar(cereal::make_nvp("NumXCut", _numXCut));
		ar(cereal::make_nvp("NumYCut", _numYCut));
		ar(cereal::make_nvp("Frame", _frame));
		ar(cereal::make_nvp("MaxFrame", _maxFrame));
		ar(cereal::make_nvp("Loop", _loop));
		ar(cereal::make_nvp("Anim", _anim));
	}
};