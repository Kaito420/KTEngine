#include "Explosion.h"
#include "BillboardEffect.h"

void Explosion::Awake()
{
	AddComponent<BillboardEffect>();
}
