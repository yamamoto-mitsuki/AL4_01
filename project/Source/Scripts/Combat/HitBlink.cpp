#include "HitBlink.h"

#include <cmath>

#include <HTTEngine/Components/Combat/HealthComponent.h>
#include <HTTEngine/Components/Rendering3D/ModelRendererComponent.h>

SCRIPT(HitBlink, "Combat");


//=============================================================================
// 更新
//=============================================================================
// ===== 毎フレーム（System の後）=====
// 被弾の後の無敵の残り（Health の invincibleTimer）は、DamageSystem が当たったフレームに入れる
// LateUpdate なら System の後なので、当たったそのフレームから点滅が始まる
void HitBlink::LateUpdate() {
	// --- 点滅するか ---
	const HealthComponent* health = GetComponent<HealthComponent>();
	const bool shouldBlink = (health != nullptr) && (health->invincibleTimer > 0.0f) && !health->IsDefeated() && (blinkInterval_ > 0.0f);
	if (!shouldBlink) {
		StopBlink(); // 無敵が切れた・倒れた（倒れた姿は消さない）
		return;
	}

	// --- 点滅の始め ---
	if (!isBlinking_) {
		isBlinking_ = true;
		blinkElapsed_ = 0.0f;
	}

	// --- 消える・出る ---
	// 2 区間で 1 回の点滅。前半は消す（当たってすぐ消えるので、当たったことが分かりやすい）、後半は出す
	const bool isVisible = std::fmod(blinkElapsed_, blinkInterval_ * 2.0f) >= blinkInterval_;
	SetVisible(isVisible);
	blinkElapsed_ += Time::GetDeltaTime(); // ヒットストップ中は 0 なので、点滅も止まる
}

// ===== 無効になった・消える前 =====
void HitBlink::OnDisable() { StopBlink(); } // 消えたまま残らないように


//=============================================================================
// 道具
//=============================================================================
// ===== 点滅を終える =====
void HitBlink::StopBlink() {
	if (!isBlinking_) {
		return; // 点滅していないときは Enabled を触らない（ほかの処理が消している物を出してしまわないように）
	}
	isBlinking_ = false;
	SetVisible(true);
}

// ===== 表示を切り替える =====
void HitBlink::SetVisible(bool visible) {
	if (ModelRendererComponent* renderer = GetComponent<ModelRendererComponent>()) {
		renderer->enabled = visible;
	}
}


//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void HitBlink::Fields(ComponentUI& ui) {
	ui.Field("Blink Interval###blinkInterval", blinkInterval_, AtLeast(0.0f).Tip("消える・出るを切り替える秒（小さいほど速く点滅する）"));
}
