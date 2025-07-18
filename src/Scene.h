//=====================================================================================
// Scene.h
// Author:Kaito Aoki
// Date:2025/07/04
//=====================================================================================

#ifndef  _SCENE_H
#define _SCENE_H

#include <memory>
#include <list>
#include "GameObject.h"

class Scene {
private:

protected:
	std::list<std::shared_ptr<GameObject>> _gameObjects;

public:
	virtual void Initialize();
	virtual void Finalize();
	virtual void Update();
	virtual void Render();

	virtual std::shared_ptr<Scene> Clone() { return nullptr; }



};

#endif // ! _SCENE_H

