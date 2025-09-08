//=====================================================================================
// Cube.h
// Author:Kaito Aoki
// Date:2025/09/08
//=====================================================================================

#ifndef _CUBE_H_
#define _CUBE_H_

#include "Component.h"
#include "RendererDX11.h"

class Cube : public Component
{
private:
	ComPtr<ID3D11Buffer> _vertexBuffer;
	ComPtr<ID3D11Buffer> _indexBuffer;

public:

	void Awake() override;
	void Render()const override;

	void ShowUI() override;

	std::string GetComponentName()override { return "Cube"; }
};



#endif// !_CUBE_H_