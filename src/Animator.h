//=====================================================================================
// Animator.h
// Author:Kaito Aoki
// Date:2026/10/20
//=====================================================================================

#ifndef _ANIMATOR_H_
#define _ANIMATOR_H_

#include "Component.h"
#include "ModelRenderer.h"
#include <string>
#include <vector>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

class Animator : public Component {
	friend class cereal::access;
private:
	int m_CurrentClipIndex = 0;
	float m_CurrentTime = 0.0f;
	float m_Speed = 1.0f;
	bool m_IsPlaying = true;
	bool m_IsLooping = true;
	bool m_BindPoseOverride = false;

	// クロスフェード補間用ステート
	int m_PrevClipIndex = -1;
	float m_PrevTime = 0.0f;
	bool m_PrevLooping = true;
	float m_CrossfadeDuration = 0.0f;
	float m_CrossfadeElapsed = 0.0f;
	float m_DefaultCrossfadeDuration = 0.25f;

	// 外部FBXから追加読み込みしたアニメーションファイルパス一覧
	std::vector<std::string> m_ExtraAnimationFiles;
	std::string m_AppliedModelFile;

	std::vector<XMFLOAT4X4> m_EvaluatedLocalTransforms;

	struct BoneTRS {
		XMVECTOR Translation;
		XMVECTOR Rotation;
		XMVECTOR Scale;
	};

	void EnsureExtraAnimationsLoaded(MODEL* model, const std::string& modelFile);
	void EvaluateClipPose(const AnimationClip& clip, float time, const std::vector<BoneInfo>& bones, std::vector<BoneTRS>& outPose) const;
	void EvaluateCurrentPose(MODEL* model);

public:
	Animator();
	virtual ~Animator() = default;

	void Awake() override;
	void Update() override;
	void ShowUI() override;
	std::string GetComponentName() override { return "Animator"; }

	// 再生制御API
	void Play(int clipIndex, float crossfadeDuration = -1.0f, bool restartIfSame = false);
	void Play(const std::string& clipName, float crossfadeDuration = -1.0f, bool restartIfSame = false);
	void Pause() { m_IsPlaying = false; }
	void Resume() { m_IsPlaying = true; m_BindPoseOverride = false; }
	void Stop();

	void SetSpeed(float speed) { m_Speed = speed; }
	float GetSpeed() const { return m_Speed; }

	void SetLooping(bool loop) { m_IsLooping = loop; }
	bool IsLooping() const { return m_IsLooping; }

	bool IsPlaying() const { return m_IsPlaying; }
	int GetCurrentClipIndex() const { return m_CurrentClipIndex; }
	float GetCurrentTime() const { return m_CurrentTime; }
	void SetCurrentTime(float time);

	bool AddAnimationFromFile(const std::string& filePath);

	template <class Archive>
	void serialize(Archive& ar) {
		ar(cereal::base_class<Component>(this));
		try {
			ar(
				cereal::make_nvp("CurrentClipIndex", m_CurrentClipIndex),
				cereal::make_nvp("Speed", m_Speed),
				cereal::make_nvp("IsPlaying", m_IsPlaying),
				cereal::make_nvp("IsLooping", m_IsLooping),
				cereal::make_nvp("DefaultCrossfadeDuration", m_DefaultCrossfadeDuration),
				cereal::make_nvp("ExtraAnimationFiles", m_ExtraAnimationFiles)
			);
		} catch (...) {}
	}
};

#endif // !_ANIMATOR_H_
