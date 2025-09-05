//=====================================================================================
// Square.h
// Author:Kaito Aoki
// Date:2025/07/15
//=====================================================================================

#ifndef _SQUARE_H
#define _SQUARE_H


#include "Component.h"
#include "RendererDX11.h"

class Square : public Component{
private:
	ComPtr<ID3D11Buffer> _vertexBuffer;

public:
	void Awake() override;

	void Update() override;

	void Render()const override;

};


#endif //!_SQUARE_H
