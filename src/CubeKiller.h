#pragma once

#include "Component.h"
#include "Collider.h"

class CubeKiller : public Component {
public:
	int _cntKill = 0;
	void Update() override;
	void OnCollisionEnter(Collider* other)override;
};