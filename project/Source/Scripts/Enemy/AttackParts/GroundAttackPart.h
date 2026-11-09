#pragma once
#include <string_view>

#include <HTTEngine/AI/AttackPart.h>
#include <HTTEngine/Components/Animation/AnimatorComponent.h>


/// <summary>
/// 地面を動く敵の部品の基底
/// </summary>
class GroundAttackPart : public AttackPart {
protected:
	/// <summary>
	/// 相手への向きと距離
	/// </summary>
	/// <returns>相手がいるか</returns>
	static bool CalcToTarget(const AttackPartContext& context, Vector3& direction, float& distance);

	// directionの方へ、なめらかに向く。smoothTime 秒で残りの差が約 37% になる（0 ならすぐ向く）
	static void TurnToward(const AttackPartContext& context, const Vector3& direction, float smoothTime);

	// 横の速さ（X・Z）を書く。縦（Y）は Character Controller の重力のまま
	static void SetHorizontalVelocity(const AttackPartContext& context, const Vector3& velocity);

	// アニメを最初から、ちょうど seconds 秒で流しきる（ループしない）。名前が空なら何もしない
	static void PlayFitted(const AttackPartContext& context, std::string_view clip, float seconds);
};