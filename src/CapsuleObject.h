//=====================================================================================
// CapsuleObject.h
// Author:Kaito Aoki
// Date:2026/03/02
//=====================================================================================

#ifndef _CAPSULEOBJECT_H
#define _CAPSULEOBJECT_H

#include "GameObject.h"

class CapsuleObject : public GameObject {
public:
	virtual void Awake()override;
};

#endif //!_CAPSULEOBJECT_H