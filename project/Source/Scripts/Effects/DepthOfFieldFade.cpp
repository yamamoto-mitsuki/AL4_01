#include "DepthOfFieldFade.h"

#include <algorithm>
#include <cmath>

#include <MyEngine/Camera/Camera.h>
#include <MyEngine/Components/TransformComponent.h>
#include <MyEngine/ECS/EntityManager.h>
#include <MyEngine/Math/Easing.h>
#include <MyEngine/Systems/CameraSystem.h>

namespace {
// メインカメラから point までの奥行き（カメラの前の向きに測った長さ。エンジンがピントを測るのと同じ測り方）
float CalcDepthFromCamera(const Vector3& point) {
	const Camera* camera = CameraSystem::GetMainCamera();
	// ビュー行列の3列目が、カメラの前の向き
	const Matrix4x4& view = camera->GetViewMatrix();
	const Vector3 forward = {view.m[0][2], view.m[1][2], view.m[2][2]};
	return (std::max)(Dot(point - camera->GetTranslation(), forward), 0.0f);
}

// Component の今の値
DepthOfFieldFade::Settings ReadSettings(const DepthOfFieldComponent& component) {
	return {
	    .focusDistance = component.focusDistance,
	    .focusRange = component.focusRange,
	    .farTransition = component.farTransition,
	    .nearTransition = component.nearTransition,
	    .maxBlur = component.maxBlur,
	};
}
} // namespace

//=============================================================================
// 始める
//=============================================================================
bool DepthOfFieldFade::Begin(const Settings& to, float duration) {
	// --- 動かす Depth Of Field を探す（有効な物の最初の1つ。エンジンが絵に使うのも、この1つ）---
	entity_ = {};
	EntityManager::ForEach<DepthOfFieldComponent>([this](Handle<Entity> entity, const DepthOfFieldComponent& component) {
		if (!entity_.IsValid() && component.enabled && EntityManager::IsActiveInHierarchy(entity)) {
			entity_ = entity;
		}
	});
	DepthOfFieldComponent* depthOfField = EntityManager::Get<DepthOfFieldComponent>(entity_);
	if (depthOfField == nullptr) {
		isPlaying_ = false;
		return false;
	}
	// --- 元の値を覚える（最初の Begin の時だけ。Restore で戻す先）---
	if (!hasOriginal_) {
		original_ = ReadSettings(*depthOfField);
		originalFocus_ = depthOfField->focus;
		hasOriginal_ = true;
	}
	isRestoring_ = false;
	// --- ピントを Distance に切り替える（FollowTarget なら、今ピントが合っている奥行きを Focus Distance に入れてから）---
	if (depthOfField->focus == DepthOfFieldFocus::FollowTarget) {
		if (const TransformComponent* target = CameraSystem::GetFollowTargetTransform()) {
			depthOfField->focusDistance = CalcDepthFromCamera(GetWorldPosition(*target));
		}
		depthOfField->focus = DepthOfFieldFocus::Distance;
	}
	StartMove(*depthOfField, to, duration);
	return true;
}

bool DepthOfFieldFade::Restore(float duration) {
	const DepthOfFieldComponent* depthOfField = EntityManager::Get<DepthOfFieldComponent>(entity_);
	if (depthOfField == nullptr || !hasOriginal_) {
		return false;
	}
	// --- 戻す先：元の値。元が FollowTarget なら、ピントは遊ぶキャラの奥行きへ（戻しきって FollowTarget にした時に、ぼけ方が跳ばないように）---
	Settings to = original_;
	if (originalFocus_ == DepthOfFieldFocus::FollowTarget) {
		if (const TransformComponent* target = CameraSystem::GetFollowTargetTransform()) {
			to.focusDistance = CalcDepthFromCamera(GetWorldPosition(*target));
		}
	}
	StartMove(*depthOfField, to, duration);
	isRestoring_ = true;
	return true;
}

void DepthOfFieldFade::StartMove(const DepthOfFieldComponent& component, const Settings& to, float duration) {
	from_ = ReadSettings(component);
	to_ = to;
	duration_ = (std::max)(duration, 0.01f); // 0 で割らないように
	elapsed_ = 0.0f;
	isPlaying_ = true;
}

//=============================================================================
// 毎フレーム
//=============================================================================
void DepthOfFieldFade::Update(float deltaTime) {
	if (!isPlaying_) {
		return;
	}
	elapsed_ = (std::min)(elapsed_ + deltaTime, duration_);
	// ゆっくり始まって、ゆっくり止まる（InOutSine）
	const float t = Ease::Apply(elapsed_ / duration_, Ease::Type::InOutSine);
	DepthOfFieldComponent* depthOfField = EntityManager::Get<DepthOfFieldComponent>(entity_);
	if (depthOfField != nullptr) {
		depthOfField->focusDistance = std::lerp(from_.focusDistance, to_.focusDistance, t);
		depthOfField->focusRange = std::lerp(from_.focusRange, to_.focusRange, t);
		depthOfField->farTransition = std::lerp(from_.farTransition, to_.farTransition, t);
		depthOfField->nearTransition = std::lerp(from_.nearTransition, to_.nearTransition, t);
		depthOfField->maxBlur = std::lerp(from_.maxBlur, to_.maxBlur, t);
	}
	isPlaying_ = (elapsed_ < duration_);
	// --- 戻しきった：ピントの合わせ方と、Focus Distance も元へ（FollowTarget のときは、Focus Distance は使われない）---
	if (!isPlaying_ && isRestoring_) {
		if (depthOfField != nullptr) {
			depthOfField->focus = originalFocus_;
			depthOfField->focusDistance = original_.focusDistance;
		}
		isRestoring_ = false;
		hasOriginal_ = false;
	}
}

void DepthOfFieldFade::Finish() { Update(duration_); } // 残りの秒数より長く進めれば、終わりの値になる
