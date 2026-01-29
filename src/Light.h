//=====================================================================================
// Light.h
// Author:Kaito Aoki
// Date:2026/01/29
//=====================================================================================

#ifndef _LIGHT_H
#define _LIGHT_H

#include "GameObject.h"
#include "RendererDX11.h"

class Light : GameObject {
public:
	LIGHT _lightData;
	void Awake() override;
	void Update() override;
};

#endif // !_LIGHT_H