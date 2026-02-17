//=====================================================================================
// CubeObject.cpp
// Author:Kaito Aoki
// Date:2026/02/17
//=====================================================================================

#include "CubeObject.h"
#include "Cube.h"

void CubeObject::Awake() {
	AddComponent<Cube>();
}