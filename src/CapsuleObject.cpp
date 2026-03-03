//=====================================================================================
// CapsuleObject.h
// Author:Kaito Aoki
// Date:2026/03/02
//=====================================================================================

#include "CapsuleObject.h"
#include "Capsule.h"

void CapsuleObject::Awake(){
	AddComponent<Capsule>();
}
