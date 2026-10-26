//=====================================================================================
// Animator.cpp
// Author:Kaito Aoki
// Date:2026/10/20
//=====================================================================================

#include "Animator.h"
#include "GameObject.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace {
	XMVECTOR SampleVec3Track(const std::vector<KeyframeVec3>& keys, float time) {
		if (keys.empty()) {
			return XMVectorZero();
		}
		if (keys.size() == 1 || time <= keys.front().Time) {
			return XMLoadFloat3(&keys.front().Value);
		}
		if (time >= keys.back().Time) {
			return XMLoadFloat3(&keys.back().Value);
		}

		auto it = std::upper_bound(keys.begin(), keys.end(), time,
			[](float t, const KeyframeVec3& kf) { return t < kf.Time; });
		size_t nextIdx = (size_t)std::distance(keys.begin(), it);
		size_t prevIdx = (nextIdx > 0) ? (nextIdx - 1) : 0;

		const KeyframeVec3& k0 = keys[prevIdx];
		const KeyframeVec3& k1 = keys[nextIdx];
		float dt = k1.Time - k0.Time;
		float factor = (dt > 1e-6f) ? std::clamp((time - k0.Time) / dt, 0.0f, 1.0f) : 0.0f;

		return XMVectorLerp(XMLoadFloat3(&k0.Value), XMLoadFloat3(&k1.Value), factor);
	}

	XMVECTOR SampleQuatTrack(const std::vector<KeyframeQuat>& keys, float time) {
		if (keys.empty()) {
			return XMQuaternionIdentity();
		}
		if (keys.size() == 1 || time <= keys.front().Time) {
			return XMQuaternionNormalize(XMLoadFloat4(&keys.front().Value));
		}
		if (time >= keys.back().Time) {
			return XMQuaternionNormalize(XMLoadFloat4(&keys.back().Value));
		}

		auto it = std::upper_bound(keys.begin(), keys.end(), time,
			[](float t, const KeyframeQuat& kf) { return t < kf.Time; });
		size_t nextIdx = (size_t)std::distance(keys.begin(), it);
		size_t prevIdx = (nextIdx > 0) ? (nextIdx - 1) : 0;

		const KeyframeQuat& k0 = keys[prevIdx];
		const KeyframeQuat& k1 = keys[nextIdx];
		float dt = k1.Time - k0.Time;
		float factor = (dt > 1e-6f) ? std::clamp((time - k0.Time) / dt, 0.0f, 1.0f) : 0.0f;

		XMVECTOR q0 = XMQuaternionNormalize(XMLoadFloat4(&k0.Value));
		XMVECTOR q1 = XMQuaternionNormalize(XMLoadFloat4(&k1.Value));
		if (XMVectorGetX(XMQuaternionDot(q0, q1)) < 0.0f) {
			q1 = XMVectorNegate(q1);
		}
		return XMQuaternionNormalize(XMQuaternionSlerp(q0, q1, factor));
	}

	float AdvanceClipTime(float currentTime, float dt, float duration, bool loop, bool& outFinished) {
		outFinished = false;
		if (duration <= 1e-5f) {
			return 0.0f;
		}
		float nextTime = currentTime + dt;
		if (loop) {
			nextTime = std::fmod(nextTime, duration);
			if (nextTime < 0.0f) {
				nextTime += duration;
			}
		} else {
			if (nextTime >= duration) {
				nextTime = duration;
				outFinished = true;
			} else if (nextTime < 0.0f) {
				nextTime = 0.0f;
				outFinished = true;
			}
		}
		return nextTime;
	}
}

Animator::Animator() {
	_executeInEditor = true;
}

void Animator::Awake() {
	_executeInEditor = true;
}

void Animator::EnsureExtraAnimationsLoaded(MODEL* model, const std::string& modelFile) {
	if (!model) return;
	if (m_AppliedModelFile == modelFile) return;

	m_AppliedModelFile = modelFile;
	for (const auto& file : m_ExtraAnimationFiles) {
		if (!file.empty()) {
			ModelRenderer::LoadAnimationFromFbx(file.c_str(), model);
		}
	}
}

bool Animator::AddAnimationFromFile(const std::string& filePath) {
	if (filePath.empty() || !_owner) return false;
	auto mr = _owner->GetComponent<ModelRenderer>();
	if (!mr || !mr->GetModel()) return false;

	if (ModelRenderer::LoadAnimationFromFbx(filePath.c_str(), mr->GetModel())) {
		if (std::find(m_ExtraAnimationFiles.begin(), m_ExtraAnimationFiles.end(), filePath) == m_ExtraAnimationFiles.end()) {
			m_ExtraAnimationFiles.push_back(filePath);
		}
		return true;
	}
	return false;
}

void Animator::Play(int clipIndex, float crossfadeDuration, bool restartIfSame) {
	if (!_owner) return;
	auto mr = _owner->GetComponent<ModelRenderer>();
	if (!mr || !mr->GetModel()) return;
	MODEL* model = mr->GetModel();
	if (clipIndex < 0 || clipIndex >= (int)model->Animations.size()) return;

	m_BindPoseOverride = false;
	float fadeTime = (crossfadeDuration < 0.0f) ? m_DefaultCrossfadeDuration : crossfadeDuration;

	if (m_CurrentClipIndex == clipIndex) {
		if (restartIfSame) {
			m_CurrentTime = 0.0f;
		}
		m_IsPlaying = true;
		return;
	}

	if (fadeTime > 1e-4f && m_CurrentClipIndex >= 0 && m_CurrentClipIndex < (int)model->Animations.size()) {
		m_PrevClipIndex = m_CurrentClipIndex;
		m_PrevTime = m_CurrentTime;
		m_PrevLooping = m_IsLooping;
		m_CrossfadeDuration = fadeTime;
		m_CrossfadeElapsed = 0.0f;
	} else {
		m_PrevClipIndex = -1;
		m_CrossfadeDuration = 0.0f;
		m_CrossfadeElapsed = 0.0f;
	}

	m_CurrentClipIndex = clipIndex;
	m_CurrentTime = 0.0f;
	m_IsPlaying = true;
}

void Animator::Play(const std::string& clipName, float crossfadeDuration, bool restartIfSame) {
	if (!_owner) return;
	auto mr = _owner->GetComponent<ModelRenderer>();
	if (!mr || !mr->GetModel()) return;
	MODEL* model = mr->GetModel();

	for (int i = 0; i < (int)model->Animations.size(); i++) {
		if (model->Animations[i].Name == clipName) {
			Play(i, crossfadeDuration, restartIfSame);
			return;
		}
	}
}

void Animator::Stop() {
	m_IsPlaying = false;
	m_CurrentTime = 0.0f;
	m_PrevClipIndex = -1;
	m_CrossfadeDuration = 0.0f;
	m_CrossfadeElapsed = 0.0f;

	if (_owner) {
		if (auto mr = _owner->GetComponent<ModelRenderer>()) {
			if (mr->GetModel()) {
				EvaluateCurrentPose(mr->GetModel());
				mr->SetAnimatedLocalTransforms(m_EvaluatedLocalTransforms);
			}
		}
	}
}

void Animator::SetCurrentTime(float time) {
	m_CurrentTime = (std::max)(0.0f, time);
	m_BindPoseOverride = false;
	if (_owner) {
		if (auto mr = _owner->GetComponent<ModelRenderer>()) {
			if (mr->GetModel()) {
				EvaluateCurrentPose(mr->GetModel());
				mr->SetAnimatedLocalTransforms(m_EvaluatedLocalTransforms);
			}
		}
	}
}

void Animator::EvaluateClipPose(const AnimationClip& clip, float time, const std::vector<BoneInfo>& bones, std::vector<BoneTRS>& outPose) const {
	size_t numBones = bones.size();
	outPose.resize(numBones);

	for (size_t i = 0; i < numBones; i++) {
		XMMATRIX defMat = XMLoadFloat4x4(&bones[i].DefaultLocalTransform);
		XMVECTOR defS, defR, defT;
		if (!XMMatrixDecompose(&defS, &defR, &defT, defMat)) {
			defS = XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
			defR = XMQuaternionIdentity();
			defT = defMat.r[3];
		}
		outPose[i].Translation = defT;
		outPose[i].Rotation = XMQuaternionNormalize(defR);
		outPose[i].Scale = defS;
	}

	for (const auto& track : clip.Tracks) {
		int bIdx = track.BoneIndex;
		if (bIdx < 0 || bIdx >= (int)numBones) continue;

		if (!track.Translations.empty()) {
			outPose[bIdx].Translation = SampleVec3Track(track.Translations, time);
		}
		if (!track.Rotations.empty()) {
			outPose[bIdx].Rotation = SampleQuatTrack(track.Rotations, time);
		}
		if (!track.Scales.empty()) {
			outPose[bIdx].Scale = SampleVec3Track(track.Scales, time);
		}
	}
}

void Animator::EvaluateCurrentPose(MODEL* model) {
	if (!model || model->Bones.empty() || model->Animations.empty()) {
		m_EvaluatedLocalTransforms.clear();
		return;
	}

	if (m_CurrentClipIndex < 0 || m_CurrentClipIndex >= (int)model->Animations.size()) {
		m_CurrentClipIndex = 0;
	}

	size_t numBones = model->Bones.size();
	std::vector<BoneTRS> currentPose;
	EvaluateClipPose(model->Animations[m_CurrentClipIndex], m_CurrentTime, model->Bones, currentPose);

	if (m_PrevClipIndex >= 0 && m_PrevClipIndex < (int)model->Animations.size() &&
		m_CrossfadeDuration > 1e-4f && m_CrossfadeElapsed < m_CrossfadeDuration) {
		std::vector<BoneTRS> prevPose;
		EvaluateClipPose(model->Animations[m_PrevClipIndex], m_PrevTime, model->Bones, prevPose);

		float alpha = std::clamp(m_CrossfadeElapsed / m_CrossfadeDuration, 0.0f, 1.0f);
		for (size_t i = 0; i < numBones; i++) {
			currentPose[i].Translation = XMVectorLerp(prevPose[i].Translation, currentPose[i].Translation, alpha);

			XMVECTOR q0 = prevPose[i].Rotation;
			XMVECTOR q1 = currentPose[i].Rotation;
			if (XMVectorGetX(XMQuaternionDot(q0, q1)) < 0.0f) {
				q1 = XMVectorNegate(q1);
			}
			currentPose[i].Rotation = XMQuaternionNormalize(XMQuaternionSlerp(q0, q1, alpha));
			currentPose[i].Scale = XMVectorLerp(prevPose[i].Scale, currentPose[i].Scale, alpha);
		}
	}

	m_EvaluatedLocalTransforms.resize(numBones);
	for (size_t i = 0; i < numBones; i++) {
		XMMATRIX S = XMMatrixScalingFromVector(currentPose[i].Scale);
		XMMATRIX R = XMMatrixRotationQuaternion(currentPose[i].Rotation);
		XMMATRIX T = XMMatrixTranslationFromVector(currentPose[i].Translation);
		XMStoreFloat4x4(&m_EvaluatedLocalTransforms[i], S * R * T);
	}
}

void Animator::Update() {
	if (!_owner) return;
	auto mr = _owner->GetComponent<ModelRenderer>();
	if (!mr) return;

	MODEL* model = mr->GetModel();
	if (!model || !model->HasSkinning || model->Bones.empty()) {
		mr->ClearAnimatedLocalTransforms();
		return;
	}

	EnsureExtraAnimationsLoaded(model, mr->GetLoadedFileName());

	if (model->Animations.empty() || m_BindPoseOverride) {
		mr->ClearAnimatedLocalTransforms();
		return;
	}

	if (m_CurrentClipIndex < 0 || m_CurrentClipIndex >= (int)model->Animations.size()) {
		m_CurrentClipIndex = 0;
	}

	float dt = ImGui::GetIO().DeltaTime;
	if (dt <= 0.0f || dt > 0.1f) {
		dt = 1.0f / 60.0f;
	}

	if (m_IsPlaying) {
		float scaledDt = dt * m_Speed;
		const AnimationClip& currClip = model->Animations[m_CurrentClipIndex];
		bool finished = false;
		m_CurrentTime = AdvanceClipTime(m_CurrentTime, scaledDt, currClip.Duration, m_IsLooping, finished);
		if (finished && !m_IsLooping) {
			m_IsPlaying = false;
		}

		if (m_PrevClipIndex >= 0 && m_PrevClipIndex < (int)model->Animations.size()) {
			const AnimationClip& prevClip = model->Animations[m_PrevClipIndex];
			bool prevFinished = false;
			m_PrevTime = AdvanceClipTime(m_PrevTime, scaledDt, prevClip.Duration, m_PrevLooping, prevFinished);

			m_CrossfadeElapsed += dt;
			if (m_CrossfadeElapsed >= m_CrossfadeDuration) {
				m_PrevClipIndex = -1;
				m_CrossfadeDuration = 0.0f;
				m_CrossfadeElapsed = 0.0f;
			}
		}
	}

	EvaluateCurrentPose(model);
	mr->SetAnimatedLocalTransforms(m_EvaluatedLocalTransforms);
}

void Animator::ShowUI() {
	ImGui::Text("Animator Component");
	ImGui::Separator();

	if (!_owner) return;
	auto mr = _owner->GetComponent<ModelRenderer>();
	if (!mr || !mr->GetModel()) {
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Requires a loaded ModelRenderer on this GameObject.");
		return;
	}

	MODEL* model = mr->GetModel();
	EnsureExtraAnimationsLoaded(model, mr->GetLoadedFileName());

	ImGui::Text("Skeleton Bones: %zu", model->Bones.size());
	ImGui::Text("Animation Clips: %zu", model->Animations.size());

	// External FBX Animation Loader
	ImGui::Spacing();
	if (ImGui::BeginCombo("Add FBX Animation", "Select .fbx file...")) {
		std::string animDir = "asset/animation";
		if (std::filesystem::exists(animDir) && std::filesystem::is_directory(animDir)) {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(animDir)) {
				if (entry.is_regular_file()) {
					std::filesystem::path filePath = entry.path();
					std::string ext = filePath.extension().string();
					if (ext == ".fbx" || ext == ".FBX") {
						std::string filename = filePath.filename().string();
						std::string relativePath = filePath.generic_string();
						if (ImGui::Selectable(filename.c_str(), false)) {
							AddAnimationFromFile(relativePath);
						}
					}
				}
			}
		} else {
			ImGui::Selectable("asset/animation not found", false, ImGuiSelectableFlags_Disabled);
		}
		ImGui::EndCombo();
	}

	if (model->Animations.empty()) {
		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "No animation clips found in this model.");
		return;
	}

	if (m_CurrentClipIndex < 0 || m_CurrentClipIndex >= (int)model->Animations.size()) {
		m_CurrentClipIndex = 0;
	}

	const AnimationClip& activeClip = model->Animations[m_CurrentClipIndex];

	ImGui::Spacing();
	if (ImGui::BeginCombo("Active Clip", activeClip.Name.c_str())) {
		for (int i = 0; i < (int)model->Animations.size(); i++) {
			bool isSelected = (m_CurrentClipIndex == i);
			std::string label = "[" + std::to_string(i) + "] " + model->Animations[i].Name +
				" (" + std::to_string(model->Animations[i].Duration).substr(0, 4) + "s)";
			if (ImGui::Selectable(label.c_str(), isSelected)) {
				Play(i, m_DefaultCrossfadeDuration, false);
			}
			if (isSelected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	// Playback Controls
	ImGui::Spacing();
	if (m_IsPlaying) {
		if (ImGui::Button("Pause")) {
			Pause();
		}
	} else {
		if (ImGui::Button("Play")) {
			Resume();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Stop")) {
		Stop();
	}
	ImGui::SameLine();
	if (ImGui::Button("Bind Pose (T-Pose)")) {
		m_IsPlaying = false;
		m_BindPoseOverride = true;
		mr->ClearAnimatedLocalTransforms();
	}

	ImGui::Checkbox("Loop", &m_IsLooping);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat("Speed", &m_Speed, 0.05f, -5.0f, 5.0f, "%.2fx");

	// Timeline Scrubber
	float maxDuration = (std::max)(0.001f, activeClip.Duration);
	std::string timelineFormat = "%.3f / " + std::to_string(activeClip.Duration).substr(0, 5) + " s";
	if (ImGui::SliderFloat("Timeline (s)", &m_CurrentTime, 0.0f, maxDuration, timelineFormat.c_str())) {
		m_BindPoseOverride = false;
		EvaluateCurrentPose(model);
		mr->SetAnimatedLocalTransforms(m_EvaluatedLocalTransforms);
	}

	// Crossfade Settings
	ImGui::Spacing();
	ImGui::SliderFloat("Crossfade Duration (s)", &m_DefaultCrossfadeDuration, 0.0f, 2.0f, "%.2f s");
	if (m_PrevClipIndex >= 0 && m_CrossfadeDuration > 0.0f) {
		float progress = std::clamp(m_CrossfadeElapsed / m_CrossfadeDuration, 0.0f, 1.0f);
		ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), "Crossfading...");
	}
}
