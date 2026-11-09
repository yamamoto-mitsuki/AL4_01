#include "GroundAttackPart.h"

#include <HTTEngine/Components/Physics/CharacterControllerComponent.h>
#include <HTTEngine/Math/MathUtility.h>
#include <HTTEngine/Systems/Animation/AnimationSystem.h>

namespace {
constexpr float kMinDirectionLengthSq = 0.0001f; // これより短い向き（ほぼ真上・真下に居る）は、向きが決まらないとみなす
} // namespace


//=============================================================================
// 相手・向き
//=============================================================================
// ===== 相手への向きと距離 =====
bool GroundAttackPart::CalcToTarget(const AttackPartContext& context, Vector3& direction, float& distance) {
	if (!context.target) {
		return false;
	}
	Vector3 toTarget = GetWorldPosition(context.target.transform) - GetWorldPosition(context.self.transform);
	toTarget.y = 0.0f; // 地面に平らに（相手が跳んでいても、上を向かない）
	distance = Length(toTarget);
	if (distance * distance > kMinDirectionLengthSq) {
		direction = toTarget / distance;
	}
	return true;
}

// ===== なめらかに向く =====
void GroundAttackPart::TurnToward(const AttackPartContext& context, const Vector3& direction, float smoothTime) {
	if (LengthSq(Vector3{direction.x, 0.0f, direction.z}) <= kMinDirectionLengthSq) {
		return; // 向きが決まらない
	}
	TransformComponent& transform = context.self.transform;
	const float yaw = GetEulerAngles(transform).y;
	const float targetYaw = MathUtility::DirectionToYaw(direction);
	const float rate = MathUtility::CalcSmoothRate(context.deltaTime, smoothTime);
	SetEulerAngles(transform, {0.0f, yaw + MathUtility::DeltaAngle(yaw, targetYaw) * rate, 0.0f});
}


//=============================================================================
// 動き・アニメ
//=============================================================================
// ===== 横の速さを書く =====
void GroundAttackPart::SetHorizontalVelocity(const AttackPartContext& context, const Vector3& velocity) {
	if (CharacterControllerComponent* controller = context.self.GetComponent<CharacterControllerComponent>()) {
		controller->velocity.x = velocity.x;
		controller->velocity.z = velocity.z;
	}
}

// ===== アニメを seconds 秒で流しきる =====
// 前の段階・前の攻撃と同じ名前のアニメが流れていると、Play は速さしか変えない（最初に戻らない）。段階の頭から流したいので Restart も呼ぶ
void GroundAttackPart::PlayFitted(const AttackPartContext& context, std::string_view clip, float seconds) {
	if (clip.empty()) {
		return; // アニメを付けない段階
	}
	const Handle<Entity> self = context.self.handle;
	AnimationSystem::Play(self, clip, false, AnimationSystem::CalcSpeedToFit(self, clip, seconds));
	AnimationSystem::Restart(self);
}