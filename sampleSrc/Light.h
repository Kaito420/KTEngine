// ======================================================================
// Light.h
// author: Kaito Aoki
// Date: 2026/05/11/
// ======================================================================

#ifndef _LIGHT_
#define _LIGHT_H

#include "Object.h"
#include "RenderManager.h"

class Light : public Object {

public:
	KTVECTOR4 m_color;
	float m_intensity; // 光の強さ
	int m_diffuseModel; // 0:Lambert, 1:half-Lambert, 2:normalized-Lambert
	int m_shadingModel; // 0:Smooth, 1:Toon
	int m_specularModel; // 0:off, 1:Phong
	int m_rimLightModel; // 0:off, 1:on
	float m_rimPower; // リムライトの強さ
	KTVECTOR3 m_rimColor; // リムライトの色

	Light();
	void Update() override;
	void Draw() override;
};

#endif // !_LIGHT_H
