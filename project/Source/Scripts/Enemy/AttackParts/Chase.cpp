#include <MyEngine/Systems/Animation/AnimationSystem.h>

#include "GroundAttackPart.h"

//=============================================================================
// 攻撃部品は他のファイル使わない（攻撃表が名前から作る）
//=============================================================================


/// <summary>
/// 待ちの部品
/// </summary>
class Chase : public GroundAttackPart {
public:
	void OnBegin(const AttackPartContext& context) override;
	void OnUpdate(const AttackPartContext& context) override;
	void OnEnd(const AttackPartContext& context, AttackPartEnd reason) override;
	void Fields(ComponentUI& ui) override;

private:
	// ===== 攻撃表に出す値 =====
	float moveSpeed_ = 4.0f;      // 近づく速さ
	float stopDistance_ = 5.0f;   // 相手とこの距離まで近づいたら止まる
	float resumeMargin_ = 0.5f;   // 止まった後、Stop Distanceよりこれだけ離れたらまた歩き出す
	float turnSmoothTime_ = 0.3f; // 相手の方を向く速さ
	AnimationClipName idleClip_;  // 止まっている間のアニメーション
	AnimationClipName walkClip_;  // 歩いている間のアニメーション
	float walkAnimSpeed_ = 1.0f;  // 歩きのアニメーションの速さ

	// ===== 実行中だけの値 =====
	bool isWalking_ = false; // 今歩いているか
};
ATTACK_PART(Chase, "待ちの部品。相手の方を向き、遠ければ近づき、近ければ止まる。");


//=============================================================================
// 始め・毎フレーム・終わり
//=============================================================================
// ===== 始め =====
void Chase::OnBegin(const AttackPartContext& /*context*/) { isWalking_ = false; }

// ===== 毎フレーム =====
void Chase::OnUpdate(const AttackPartContext& context) { 
	Vector3 direction = {0.0f, 0.0f, 0.0f};
	float distance = 0.0f;
	// --- 相手がいないとき ---
	if (!CalcToTarget(context, direction, distance)) {
		isWalking_ = false;
		SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f});
		AnimationSystem::Play(context.self.handle, idleClip_.View(), true);
		return;
	}

	// --- 向く ---
	TurnToward(context, direction, turnSmoothTime_);

	// --- 歩く・止まる ---
	isWalking_ = isWalking_ ? (distance > stopDistance_) : (distance > stopDistance_ + resumeMargin_);
	SetHorizontalVelocity(context, isWalking_ ? direction * moveSpeed_ : Vector3{0.0f, 0.0f, 0.0f});

	// --- アニメ：同じ名前を毎フレーム渡しても、最初には戻らない（速さだけ変わる）---
	if (isWalking_) {
		AnimationSystem::Play(context.self.handle, walkClip_.View(), true, walkAnimSpeed_);
	} else {
		AnimationSystem::Play(context.self.handle, idleClip_.View(), true);
	}
}

// ===== 終わり =====
void Chase::OnEnd(const AttackPartContext& context, AttackPartEnd /*reason*/) {
	isWalking_ = false;
	SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f});
}


//=============================================================================
// 攻撃表に出す値
//=============================================================================
void Chase::Fields(ComponentUI& ui) {
	ui.Field("Move Speed###moveSpeed", moveSpeed_, AtLeast(0.0f).Tip("近づく速さ"));
	ui.Field("Stop Distance###stopDistance", stopDistance_, AtLeast(0.0f).Tip("相手とこの距離まで近づいたら止まる"));
	ui.Field("Resume Margin###resumeMargin", resumeMargin_, AtLeast(0.0f).Tip("止まった後、Stop Distanceよりこれだけ離れたらまた歩き出す"));
	ui.Field("Turn Smooth Time###turnSmoothTime", turnSmoothTime_, AtLeast(0.0f).Tip("相手の方を向く速さ"));
	ui.ClipField("Idle Clip###idleClip", idleClip_, Tip("止まっている間のアニメーション"));
	ui.ClipField("Walk Clip###walkClip", walkClip_, Tip("歩いている間のアニメーション"));
	ui.Field("Walk Anim Speed###walkAnimSpeed", walkAnimSpeed_, AtLeast(0.0f).Tip("歩きのアニメーションの速さ"));
}
