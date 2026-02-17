//=====================================================================================
// CubeObject.h
// Author:Kaito Aoki
// Date:2026/02/17
//=====================================================================================

#ifndef _CUBEOBJECT_H
#define _CUBEOBJECT_H

#include "GameObject.h"

class CubeObject : public GameObject {
	public:
	virtual void Awake() override;
};

#endif // _CUBEOBJECT_H