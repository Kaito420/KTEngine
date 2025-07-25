//=====================================================================================
// GameObject.h
// Author:Kaito Aoki
// Date:2025/06/25
//=====================================================================================

#ifndef _GAMEOBJECT_H
#define _GAMEOBJECT_H

#include "ktvector.hpp"

class GameObject {
private:
	bool _active = true; //アクティブ状態（true:有効, false:無効）
	bool _awakened = false; //Awakeが実行されたかどうか
	bool _started = false; //Startが実行されたかどうか
protected:
	bool _destroy = false;
	struct Transform {
		KTVECTOR3 _position = { 0.0f, 0.0f, 0.0f };
		KTVECTOR3 _scale = { 1.0f, 1.0f, 1.0f };
		KTVECTOR3 _rotation = { 0.0f, 0.0f, 0.0f };	//Degree
	};

	Transform _transform; //位置、スケール、回転を保持するTransform構造体
	

public:
	virtual ~GameObject() {}

	/// <summary>
	/// インスタンス生成直後に実行（コンポーネント有効無効に関係なく呼ばれる）
	/// </summary>
	virtual void Awake() {}

	/// <summary>
	/// 最初のUpdate直前に実行（初回のみ、非アクティブの際は無視）
	/// </summary>
	virtual void Start() {}

	/// <summary>
	/// 毎フレーム実行（非アクティブの際は無視）
	/// </summary>
	virtual void Update() {}

	/// <summary>
	/// //Update後に実行（非アクティブの際は無視）
	/// </summary>
	virtual void LateUpdate() {} 

	/// <summary>
	/// LateUpdate後に実行（非アクティブの際は無視）
	/// </summary>
	virtual void Render() {}

	/// <summary>
	/// 削除直前に実行（非アクティブの際は無視）
	/// </summary>
	virtual void OnDestroy() {}

};


#endif // !_GAMEOBJECT_H