#include <format>
#include <string>

#include <MyEngine/Components/Combat/HitboxComponent.h>
#include <MyEngine/Diagnostics/LogManager.h>
#include <MyEngine/Systems/Combat/HitSystem.h>

#include "GroundAttackPart.h"

//=============================================================================
// 攻撃部品は他のファイル使わない（攻撃表が名前から作る）
//=============================================================================


/// <summary>
/// その場の攻撃
/// <para>しっぽの薙ぎ払い・ボスの周りの攻撃など</para>
/// </summary>
class StationaryAttack : public GroundAttackPart {
public:
	void OnBegin(const AttackPartContext& context) override;
	void OnStage(const AttackPartContext& context) override;
	void OnUpdate(const AttackPartContext& context) override;
	void OnEnd(const AttackPartContext& context, AttackPartEnd reason) override;
	void Fields(ComponentUI& ui) override;

private:
	// ===== 攻撃表に出す値 =====
	AnimationClipName windupClip_;    // 予備動作のアニメーション
	AnimationClipName activeClip_;    // 攻撃のアニメーション
	AnimationClipName recoveryClip_;  // 硬直のアニメーション
	std::string hitboxName_;          // 窓を合わせる Hitbox の子の Entity の名前
	bool faceTargetInWindup_ = false; // 予備動作の間、相手の方へ向き続けるか
	float turnSmoothTime_ = 0.2f;     // 向く速さ

	// ===== 実行中だけの値 =====
	GameObject hitbox_; // 窓を合わせる Hitbox の Entity（OnBegin で名前から探す）
};
ATTACK_PART(StationaryAttack, "その場の攻撃。しっぽの薙ぎ払いなど");


//=============================================================================
// 始め・段階・毎フレーム・終わり
//=============================================================================
// ===== 始め =====
void StationaryAttack::OnBegin(const AttackPartContext& context) {
	hitbox_ = context.self.Find(hitboxName_);
	if (!hitbox_) {
		if (!hitboxName_.empty()) {
			LogManager::Warning(std::format("StationaryAttack：{} の子に \"{}\" がありません（当たりなしで、アニメだけ流します）", context.self.GetName(), hitboxName_));
		}
		return;
	}
	// 撃った本人＝この敵（空のままだと Hitbox の Entity が本人になり、ジャスト回避・当たりの知らせで「誰の攻撃か」が敵にならない）
	if (HitboxComponent* hitbox = hitbox_.GetComponent<HitboxComponent>()) {
		hitbox->instigator = EntityManager::RefOf(context.self.handle);
	}
}

// ===== 段階に入った =====
void StationaryAttack::OnStage(const AttackPartContext& context) {
	SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f}); // その場の攻撃なので動かない
	if (hitbox_) {
		SyncHitbox(hitbox_, context); // 予備動作 → Warning、攻撃 → Active（新しい攻撃の回）、硬直 → Off
	}
	switch (context.stage) {
	case AttackStage::Windup:
		PlayFitted(context, windupClip_.View(), context.stageDuration);
		break;
	case AttackStage::Active:
		PlayFitted(context, activeClip_.View(), context.stageDuration);
		break;
	case AttackStage::Recovery:
		PlayFitted(context, recoveryClip_.View(), context.stageDuration);
		break;
	case AttackStage::None:
		break;
	}
}

// ===== 毎フレーム =====
void StationaryAttack::OnUpdate(const AttackPartContext& context) {
	if (context.stage != AttackStage::Windup || !faceTargetInWindup_) {
		return; // 予備動作のみ
	}
	Vector3 direction = {0.0f, 0.0f, 0.0f};
	float distance = 0.0f;
	if (CalcToTarget(context, direction, distance)) {
		TurnToward(context, direction, turnSmoothTime_);
	}
}

// ===== 終わり（途中で止められたときも呼ばれる）=====
void StationaryAttack::OnEnd(const AttackPartContext& /*context*/, AttackPartEnd /*reason*/) {
	if (hitbox_) {
		HitSystem::Deactivate(hitbox_.handle); // 怯みで止められたときも、窓を開けっぱなしにしない
	}
}


//=============================================================================
// 攻撃表に出す値
//=============================================================================
void StationaryAttack::Fields(ComponentUI& ui) {
	ui.ClipField("Windup Clip###windupClip", windupClip_, Tip("予備動作のアニメーション"));
	ui.ClipField("Active Clip###activeClip", activeClip_, Tip("攻撃のアニメーション"));
	ui.ClipField("Recovery Clip###recoveryClip", recoveryClip_, Tip("硬直のアニメーション"));
	ui.Field("Hitbox###hitbox", hitboxName_, Tip("窓を合わせる Hitbox の子の Entity の名前"));
	ui.Field("Face Target In Windup###faceTargetInWindup", faceTargetInWindup_, Tip("予備動作の間、相手の方へ向き続けるか"));
	ui.Field("Turn Smooth Time###turnSmoothTime", turnSmoothTime_, AtLeast(0.0f).Tip("向く速さ"));
}
