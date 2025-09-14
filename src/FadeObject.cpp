#include "FadeObject.h"
#include "Fade.h"


void FadeObject::Awake()
{
	AddComponent<Fade>();
}


