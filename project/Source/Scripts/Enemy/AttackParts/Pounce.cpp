#include <format>

#include <HTTEngine/Components/Combat/HitboxComponent.h>
#include <HTTEngine/Components/Physics/CharacterControllerComponent.h>
#include <HTTEngine/Diagnostics/LogManager.h>
#include <HTTEngine/Math/MathUtility.h>
#include <HTTEngine/Systems/Combat/HitSystem.h>

#include "GroundAttackPart.h"

//=============================================================================
// 攻撃部品は他のファイル使わない（攻撃表が名前から作る）
//=============================================================================


/// <summary>
/// とびかかる
/// </summary>
class Pounce : public GroundAttackPart {
public:
	void OnBegin(const AttackPartContext& context) override;
	void OnStage(const AttackPartContext& context) override;
	void OnUpdate(const AttackPartContext& context) override;
	void OnEnd(const AttackPartContext& context, AttackPartEnd reason) override;
	void Fields(ComponentUI& ui) override;

private:
	void StartLeap(const AttackPartContext& context);      // 跳ぶ
	void UpdateLeap(const AttackPartContext& context);     // 空中
	void RestoreGravity(const AttackPartContext& context); // 切った重力を戻す
	Vector3 CalcLeapPosition(float elapsed) const;         // 跳んでからelapsed秒の位置
	float CalcCrouchSeconds(const AttackPartContext& context) const; // かがむ秒

	// ===== 攻撃表に出す値 =====
	float leapSeconds_ = 0.8f;       // 空中にいる秒数
	float leapHeight_ = 3.0f;        // 跳ぶ一番高い所
	float maxLeapDistance_ = 14.0f;  // 届く距離
	float turnSmoothTime_ = 0.15f;   // かがむ間に相手の方へ向く速さ
	uint32_t impactPrefab_ = 0;      // 着地点に置く当たり（Hitbox の Circle・Damage Source・Hitbox Telegraph を付けた Prefab）
	AnimationClipName crouchClip_;   // かがむアニメーション
	AnimationClipName leapClip_;     // 空中のアニメーション
	AnimationClipName activeClip_;   // 着地のアニメーション
	AnimationClipName recoveryClip_; // 戻りのアニメーション

	// ===== 実行中だけの値 =====
	GameObject impact_;                      // 着地点の当たり（OnBegin で Prefab から作り、OnEnd で消す）
	bool hasLeapt_ = false;                  // もう跳んだか
	bool isGravityOff_ = false;              // 重力を切っているか（戻し忘れないように覚える）
	bool savedUseGravity_ = true;            // 切る前の Use Gravity
	Vector3 leapStart_ = {0.0f, 0.0f, 0.0f}; // 跳んだ所（ワールド）
	Vector3 leapEnd_ = {0.0f, 0.0f, 0.0f};   // 着地点（ワールド。高さは跳んだ床と同じ）
	float leapDuration_ = 0.0f;              // 実際の空中の秒（跳んだフレームから、予備動作の終わりまで）
	float leapElapsed_ = 0.0f;               // 跳んでからの秒
};
ATTACK_PART(Pounce, "飛びかかり");

namespace {
constexpr float kMinLeapDistance = 0.01f; // これより近ければ、その場で跳ぶ（向きは今のまま）
} // namespace


//=============================================================================
// 始め・段階・毎フレーム・終わり
//=============================================================================
// ===== 始め =====
void Pounce::OnBegin(const AttackPartContext& context) { 
	hasLeapt_ = false;
	isGravityOff_ = false;
	impact_ = {};
	if (impactPrefab_ == 0) {
		LogManager::Warning("Pounce：Impact Prefab が選ばれていません（当たりなしで、跳ぶだけ）");
		return;
	}
	impact_ = GameObject::Instantiate(impactPrefab_, GetWorldPosition(context.self.transform));
}

// ===== 段階に入った =====
void Pounce::OnStage(const AttackPartContext& context) {
	switch (context.stage) { 
	case AttackStage::Windup:
		SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f});
		PlayFitted(context, crouchClip_.View(), CalcCrouchSeconds(context));
		break;
	case AttackStage::Active:
		RestoreGravity(context);
		if (impact_) {
			SyncHitbox(impact_, context); // 当たる（新しい攻撃の回。攻撃の秒が過ぎたら、Hitbox の方でも閉じる）
		}
		PlayFitted(context, activeClip_.View(), context.stageDuration);
		break;
	case AttackStage::Recovery:
		if (impact_) {
			SyncHitbox(impact_, context); // 閉じる
		}
		SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f});
		PlayFitted(context, recoveryClip_.View(), context.stageDuration);
		break;
	case AttackStage::None:
		break;
	}
}

// ===== 毎フレーム =====
void Pounce::OnUpdate(const AttackPartContext& context) {
	if (context.stage != AttackStage::Windup) {
		SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f}); // 着地の後は動かない
		return;
	}
	// --- かがむ：相手の方へ向き続けて、かがむ秒が過ぎたら跳ぶ ---
	if (!hasLeapt_) {
		if (context.stageTime < CalcCrouchSeconds(context)) {
			Vector3 direction = {0.0f, 0.0f, 0.0f};
			float distance = 0.0f;
			if (CalcToTarget(context, direction, distance)) {
				TurnToward(context, direction, turnSmoothTime_);
			}
			return;
		}
		StartLeap(context);
	}
	// --- 空中 ---
	UpdateLeap(context);
}

// ===== 終わり（途中で止められたときも呼ばれる）=====
void Pounce::OnEnd(const AttackPartContext& context, AttackPartEnd /*reason*/) {
	RestoreGravity(context); // 空中で怯んだら、ここから重力で落ちる
	SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f});
	if (impact_) {
		HitSystem::Deactivate(impact_.handle);
		EntityManager::Destroy(impact_.handle); // 攻撃のたびに作るので、終わったら消す
	}
	impact_ = {};
}


//=============================================================================
// 跳ぶ・空中
//=============================================================================
// ===== 跳ぶ =====
void Pounce::StartLeap(const AttackPartContext& context) {
	hasLeapt_ = true;

	// --- 着地点 ---
	leapStart_ = GetWorldPosition(context.self.transform);
	Vector3 direction = MathUtility::YawToDirection(GetEulerAngles(context.self.transform).y); // 相手が居ない・真上に居るときは前へ
	float distance = 0.0f;
	CalcToTarget(context, direction, distance);
	distance = (std::min)(distance, maxLeapDistance_);
	leapEnd_ = leapStart_ + direction * distance;

	// --- 空中の秒 ---
	leapDuration_ = (std::max)(context.stageDuration - context.stageTime + context.deltaTime, context.deltaTime);
	leapElapsed_ = 0.0f;

	// --- 着地点の向きへぴったり向く（空中では回らない）---
	if (distance > kMinLeapDistance) {
		SetEulerAngles(context.self.transform, {0.0f, MathUtility::DirectionToYaw(direction), 0.0f});
	}

	// --- 予告：着地点の当たりを動かして、空中の間 Warning（Just Lead Time 秒前から、ジャスト判定の範囲が有効）---
	if (impact_) {
		impact_.transform.translation = leapEnd_; // 一番上の Entity なので、translation がそのままワールドの位置
		if (HitboxComponent* hitbox = impact_.GetComponent<HitboxComponent>()) {
			hitbox->instigator = EntityManager::RefOf(context.self.handle); // 撃った本人＝この敵（敵とその子には当たらない・知らせが敵に届く）
		}
		HitSystem::Warn(impact_.handle, leapDuration_);
	}

	// --- 重力を切る（縦もこの部品が書く）---
	if (CharacterControllerComponent* controller = context.self.GetComponent<CharacterControllerComponent>()) {
		savedUseGravity_ = controller->useGravity;
		controller->useGravity = false;
		isGravityOff_ = true;
	}

	PlayFitted(context, leapClip_.View(), leapDuration_);
}

// ===== 空中 =====
void Pounce::UpdateLeap(const AttackPartContext& context) {
	CharacterControllerComponent* controller = context.self.GetComponent<CharacterControllerComponent>();
	if (controller == nullptr || context.deltaTime <= 0.0f) {
		return;
	}
	const float next = (std::min)(leapElapsed_ + context.deltaTime, leapDuration_);
	controller->velocity = (CalcLeapPosition(next) - CalcLeapPosition(leapElapsed_)) / context.deltaTime;
	leapElapsed_ = next;
}

// ===== 跳んでから elapsed 秒の位置 =====
Vector3 Pounce::CalcLeapPosition(float elapsed) const {
	const float s = (leapDuration_ > 0.0f) ? (std::min)(elapsed / leapDuration_, 1.0f) : 1.0f;
	Vector3 position = Lerp(leapStart_, leapEnd_, s);
	position.y += 4.0f * leapHeight_ * s * (1.0f - s);
	return position;
}

// ===== 切った重力を戻す =====
void Pounce::RestoreGravity(const AttackPartContext& context) {
	if (!isGravityOff_) {
		return;
	}
	if (CharacterControllerComponent* controller = context.self.GetComponent<CharacterControllerComponent>()) {
		controller->useGravity = savedUseGravity_;
	}
	isGravityOff_ = false;
}

// ===== かがむ秒 =====
float Pounce::CalcCrouchSeconds(const AttackPartContext& context) const { return (std::max)(context.stageDuration - leapSeconds_, 0.0f); }

//=============================================================================
// 攻撃表に出す値
//=============================================================================
void Pounce::Fields(ComponentUI& ui) {
	ui.Field("Leap Seconds###leapSeconds", leapSeconds_, AtLeast(0.0f).Tip("空中にいる秒数"));
	ui.Field("Leap Height###leapHeight", leapHeight_, AtLeast(0.0f).Tip("跳ぶ一番高い所"));
	ui.Field("Max Leap Distance###maxLeapDistance", maxLeapDistance_, AtLeast(0.0f).Tip("届く距離"));
	ui.Field("Turn Smooth Time###turnSmoothTime", turnSmoothTime_, AtLeast(0.0f).Tip("かがむ間に相手の方へ向く速さ"));
	ui.AssetField("Impact Prefab###impactPrefab", impactPrefab_, AssetType::Prefab, Tip("着地点に置く当たり（Hitbox の Circle・Damage Source・Hitbox Telegraph を付けた Prefab）"));
	ui.ClipField("Crouch Clip###crouchClip", crouchClip_, Tip("かがむアニメーション"));
	ui.ClipField("Leap Clip###leapClip", leapClip_, Tip("空中のアニメーション"));
	ui.ClipField("Active Clip###activeClip", activeClip_, Tip("着地のアニメーション"));
	ui.ClipField("Recovery Clip###recoveryClip", recoveryClip_, Tip("戻りのアニメーション"));
}
