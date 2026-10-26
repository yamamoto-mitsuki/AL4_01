#include "DepthOfFieldFade.h"

#include <algorithm>
#include <cmath>

#include <MyEngine/Camera/Camera.h>
#include <MyEngine/Components/DepthOfFieldComponent.h>
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
	// --- ピントを Distance に切り替える（FollowTarget なら、今ピントが合っている奥行きを Focus Distance に入れてから）---
	if (depthOfField->focus == DepthOfFieldFocus::FollowTarget) {
		if (const TransformComponent* target = CameraSystem::GetFollowTargetTransform()) {
			depthOfField->focusDistance = CalcDepthFromCamera(GetWorldPosition(*target));
		}
		depthOfField->focus = DepthOfFieldFocus::Distance;
	}
	// --- 今の値を覚えて、動かし始める ---
	from_ = {
	    .focusDistance = depthOfField->focusDistance,
	    .focusRange = depthOfField->focusRange,
	    .farTransition = depthOfField->farTransition,
	    .nearTransition = depthOfField->nearTransition,
	    .maxBlur = depthOfField->maxBlur,
	};
	to_ = to;
	duration_ = (std::max)(duration, 0.01f); // 0 で割らないように
	elapsed_ = 0.0f;
	isPlaying_ = true;
	return true;
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
	if (DepthOfFieldComponent* depthOfField = EntityManager::Get<DepthOfFieldComponent>(entity_)) {
		depthOfField->focusDistance = std::lerp(from_.focusDistance, to_.focusDistance, t);
		depthOfField->focusRange = std::lerp(from_.focusRange, to_.focusRange, t);
		depthOfField->farTransition = std::lerp(from_.farTransition, to_.farTransition, t);
		depthOfField->nearTransition = std::lerp(from_.nearTransition, to_.nearTransition, t);
		depthOfField->maxBlur = std::lerp(from_.maxBlur, to_.maxBlur, t);
	}
	isPlaying_ = (elapsed_ < duration_);
}