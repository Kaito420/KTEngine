//=====================================================================================
// Component.h
// Author:Kaito Aoki
// Date:2025/08/14
//=====================================================================================

#ifndef _COMPONENT_H
#define _COMPONENT_H

class Component {
protected:
	bool _active = false;
	bool _awakend = false;
public:
	virtual ~Component() {}

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

#endif // !_COMPONENT_H