#pragma once

#include "Component.h"
#include "RendererDX11.h"

class BillboardEffect : public Component {

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
};