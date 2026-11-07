#include "BossCat.h"

#include <string_view>

#include <MyEngine/Components/Rendering3D/ModelRendererComponent.h>
#include <MyEngine/Systems/Animation/AnimationSystem.h>
#include <MyEngine/Systems/Gameplay/AttackTableSystem.h>

SCRIPT(BossCat, "Enemy");

namespace {
// --- アニメの名前（Boss.gltf の中の名前。Blender のアクションの名前）。攻撃・待ちのアニメは攻撃表の部品の値に書く ---
constexpr std::string_view kClipStagger = "Stagger";         // 怯み
constexpr std::string_view kClipPhaseChange = "PhaseChange"; // フェーズの移行
constexpr std::string_view kClipDown = "Down";               // 撃破

// --- 顔のマス（BossFace.png は横 4 × 縦 2。顔の UV は左上の 1 マスに貼ってあるので、マスの大きさずつずらす）---
constexpr int kFaceColumns = 4;                       // 横のマスの数
constexpr float kFaceCellWidth = 1.0f / kFaceColumns; // 1 マスの横の大きさ（UV）
constexpr float kFaceCellHeight = 1.0f / 2.0f;        // 1 マスの縦の大きさ（UV。下の段は V が大きい方）
} // namespace


//=============================================================================
// 初期化・更新
//=============================================================================
// ===== 始め =====
void BossCat::Start() { SetFace(BossFace::Normal); }

// ===== 毎フレーム =====
void BossCat::Update() {
	const AttackTableStatus status = AttackTableSystem::GetStatus(gameObject.handle);
	if (!status.isRunning) {
		return; // まだ動き始めていない（攻撃表が選ばれていない・最初のフレーム）
	}
	if (status.state != lastState_) {
		lastState_ = status.state;
		OnStateChanged(status.state, status.duration);
	}
	SetFace(ChooseFace(status.state));
}


//=============================================================================
// 顔の表情
//=============================================================================
// ===== 顔を変える =====
void BossCat::SetFace(BossFace face) {
	face_ = face;
	ModelRendererComponent* renderer = GetComponent<ModelRendererComponent>();
	if (renderer == nullptr) {
		return;
	}
	const int index = static_cast<int>(face);
	renderer->uvTransform.translation = {static_cast<float>(index % kFaceColumns) * kFaceCellWidth, static_cast<float>(index / kFaceColumns) * kFaceCellHeight, 0.0f};
}

// ===== 状態ごとの顔 =====
BossFace BossCat::ChooseFace(AttackTableState state) {
	switch (state) {
	case AttackTableState::Attacking:
		return BossFace::Angry;
	case AttackTableState::Staggered:
		return BossFace::Hurt;
	case AttackTableState::Defeated:
		return BossFace::Dizzy;
	case AttackTableState::Selecting:
	case AttackTableState::PhaseTransition:
		return BossFace::Normal;
	}
	return BossFace::Normal;
}


//=============================================================================
// 状態が変わった瞬間
//=============================================================================
void BossCat::OnStateChanged(AttackTableState state, float duration) {
	const Handle<Entity> self = gameObject.handle;
	switch (state) {
	case AttackTableState::Staggered:
		AnimationSystem::Play(self, kClipStagger, false, AnimationSystem::CalcSpeedToFit(self, kClipStagger, duration));
		AnimationSystem::Restart(self);
		break;
	case AttackTableState::PhaseTransition:
		AnimationSystem::Play(self, kClipPhaseChange, false, AnimationSystem::CalcSpeedToFit(self, kClipPhaseChange, duration));
		AnimationSystem::Restart(self);
		break;
	case AttackTableState::Defeated:
		AnimationSystem::Play(self, kClipDown, false, 1.0f);
		AnimationSystem::Restart(self);
		break;
	case AttackTableState::Selecting:
	case AttackTableState::Attacking:
		break;
	}
}