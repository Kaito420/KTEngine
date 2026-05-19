import re

def update_file(filename, old_str, new_str):
    encodings = ['utf-8', 'shift_jis', 'cp932']
    content = None
    used_enc = None
    for enc in encodings:
        try:
            with open(filename, 'r', encoding=enc) as f:
                content = f.read()
            used_enc = enc
            break
        except UnicodeDecodeError:
            pass
            
    if content is None:
        print(f"Could not read {filename}")
        return
        
    if old_str in content:
        content = content.replace(old_str, new_str)
        with open(filename, 'w', encoding=used_enc) as f:
            f.write(content)
        print(f"Updated {filename} successfully.")
    else:
        print(f"{old_str} not found in {filename}.")

# Scene.h update
old_scene_h = "virtual PhysicsSystem* GetPhysicsSystem() { return _physicsSystem; }"
new_scene_h = "virtual PhysicsSystem* GetPhysicsSystem() { return _physicsSystem; }\n\tstd::shared_ptr<GameObject> GetSelectedGameObject();"
update_file("src/Scene.h", old_scene_h, new_scene_h)

# Scene.cpp update
old_scene_cpp = "PhysicsSystem* Scene::GetPhysicsSystem()"
new_scene_cpp = """std::shared_ptr<GameObject> Scene::GetSelectedGameObject() {
	if (_selectedObjId != -1) {
		for (auto& gameObject : _gameObjects) {
			if (gameObject->_id == _selectedObjId) {
				return gameObject;
			}
		}
	}
	return nullptr;
}

PhysicsSystem* Scene::GetPhysicsSystem()"""
update_file("src/Scene.cpp", old_scene_cpp, new_scene_cpp)
