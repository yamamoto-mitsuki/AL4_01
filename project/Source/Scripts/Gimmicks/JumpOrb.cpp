#include "JumpOrb.h"

#include <algorithm>

#include <MyEngine/Math/MathUtility.h>

SCRIPT(JumpOrb, "Gimmick");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void JumpOrb::Fields(ComponentUI& ui) {
	JumpRing::Fields(ui);
	ui.Label("キューブが触れている間、空中でもう一度跳べる。使ったら、円から一度出るまで使えない");
	ui.Field("Spin Speed", spinSpeed_, Tip("子の Ring が回る速さ（度/秒。マイナスで逆回り）"));
	ui.Field("Pop Scale", popScale_, AtLeast(1.0f).Tip("使ったとき、子の Ring がこの倍まで広がって消える"));
	ui.Field("Pop Time", popTime_, AtLeast(0.01f).Tip("子の Ring が広がって消えるまでの秒数"));
	ui.Field("Used Brightness", usedBrightness_, Range(0.0f, 1.0f).Tip("使った後の、子の Core の明るさ（元の色に掛ける。キューブが円から出ると元に戻る）"));
}

//=============================================================================
// 毎フレーム
//=============================================================================
void JumpOrb::Start() {
	// 子の見た目を探して、元の大きさと色を覚える（無ければ、その見た目は動かさない）
	ring_ = gameObject.FindChild("Ring");
	core_ = gameObject.FindChild("Core");
	if (ring_) {
		ringScale_ = ring_.GetTransform().scale;
		if (const PrimitiveRendererComponent* renderer = ring_.GetComponent<PrimitiveRendererComponent>()) {
			ringColor_ = renderer->material.color;
		}
	}
	if (core_) {
		if (const PrimitiveRendererComponent* renderer = core_.GetComponent<PrimitiveRendererComponent>()) {
			coreColor_ = renderer->material.color;
		}
	}
}

void JumpOrb::Update() {
	const float deltaTime = Time::GetDeltaTime();
	popTimer_ += deltaTime;
	// --- 輪：ゆっくり回る。使ったら Pop Time 秒で、広がりながら薄くなって消える ---
	if (ring_) {
		const float t = IsUsed() ? std::clamp(popTimer_ / popTime_, 0.0f, 1.0f) : 0.0f; // 0：元のまま 〜 1：消えた
		TransformComponent& ring = ring_.GetTransform();
		Rotate(ring, {0.0f, 0.0f, 1.0f}, spinSpeed_ * deltaTime);
		ring.scale = ringScale_ * MathUtility::Lerp(1.0f, popScale_, t);
		if (PrimitiveRendererComponent* renderer = ring_.GetComponent<PrimitiveRendererComponent>()) {
			renderer->material.color = ringColor_ * (1.0f - t); // 色ごと薄くする（Transparent は A が、Additive は RGB が効く）
		}
	}
	// --- 真ん中の玉：使ったら暗くなる（A はそのまま）---
	if (core_) {
		if (PrimitiveRendererComponent* renderer = core_.GetComponent<PrimitiveRendererComponent>()) {
			const float brightness = IsUsed() ? usedBrightness_ : 1.0f;
			renderer->material.color = {coreColor_.x * brightness, coreColor_.y * brightness, coreColor_.z * brightness, coreColor_.w};
		}
	}
}