//=====================================================================================
// ComponentRegistry.h
// Author:Kaito Aoki
// Date:2026/02/17
//=====================================================================================

#ifndef _COMPONENTREGISTRY_H
#define _COMPONENTREGISTRY_H

#include <string>
#include <map>
#include <functional>
#include <vector>
#include "GameObject.h"

class ComponentRegistry {
private:
	//コンポーネント生成関数の型定義
	using CreatorFunc = std::function<void(GameObject*)>;

	static std::map<std::string, CreatorFunc> _creators;

public:
	//コンポーネント登録用テンプレート
	template <typename T>
	static void Register(const std::string& name) {
		//ラムダ式でAddComponent<T>を呼び出す関数を保存
		_creators[name] = [](GameObject* obj) {
			obj->AddComponent<T>();
		};
	}

	//インスペクター描画用
	static void RenderAddComponentMenu(GameObject* target);

};

#endif //!_COMPONENTREGISTRY_H