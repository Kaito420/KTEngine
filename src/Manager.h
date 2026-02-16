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

enum class EngineMode {
	Editor,
	Runtime
};

class Manager {
private:
	static std::shared_ptr<Scene> _editorScene;
	static std::shared_ptr<Scene> _runtimeScene;
	static std::shared_ptr<Scene> _nextScene;

	static std::string _currentScenePath;	//現在シーンのファイルパス（上書き保存用）

	static EngineMode _mode;
public:
	static void Initialize();
	static void Finalize();
	static void Update();
	static void Render();

	static std::shared_ptr<Scene> GetCurrentScene();

	template<typename T>
	static void ChangeScene() {
		_nextScene = std::make_shared<T>();
	}

	static void NewScene();

	static void SaveScene(const std::string& filePath);
	static void OpenScene(const std::string& filePath);

	/// <summary>
	/// メニューバーの描画
	/// </summary>
	static void RenderMenuBar();

	/// <summary>
	/// 現在のパスに上書き保存（パスがなければダイアログ）
	/// </summary>
	static void SaveScene();

	/// <summary>
	/// 名前を付けて保存
	/// </summary>
	static void SaveSceneAs();

	static void Play();
	static void Stop();
	static EngineMode GetMode() { return _mode; }

	static std::string GetCurrentScenePath() { return _currentScenePath; }
};


#endif // !_MANAGER_H
