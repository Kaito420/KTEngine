#include "CubeKiller.h"
#include "Cube.h"
#include "Collider.h"
#include "GameObject.h"
#include "Manager.h"
#include "Scene.h"
#include "SceneResult.h"

void CubeKiller::Update()
{
	if (_cntKill >= 5)
		Manager::ChangeScene<SceneResult>();
}

void CubeKiller::OnCollisionEnter(Collider* other)
{
	if (other->GetOwner()->GetComponent<Cube>()) {
		other->GetOwner()->Destroy();
		_cntKill++;
	}
}
