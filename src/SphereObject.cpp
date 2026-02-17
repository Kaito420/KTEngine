//=====================================================================================
// SphereObject.cpp
// Author:Kaito Aoki
// Date:2026/02/17
//=====================================================================================

#include "SphereObject.h"
#include "Sphere.h"

void SphereObject::Awake() {
	AddComponent<Sphere>();
}