//=====================================================================================
// Manager.h
// Author:Kaito Aoki
// Date:2025/07/04
//=====================================================================================

#ifndef _MANAGER_H
#define _MANAGER_H

#include <memory>
#include <string>
#include <unordered_map>

class Scene;

class Manager {
private:
	static std::shared_ptr<Scene> _currentScene;

public:
	static void Initialize();
	static void Finalize();
	static void Update();
	static void Render();

	static std::shared_ptr<Scene> GetCurrentScene();
};


#endif // !_MANAGER_H
