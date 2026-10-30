#include "LaunchOrb.h"

#include <algorithm>
#include <array>

#include <MyEngine/Math/MathUtility.h>
#include <MyEngine/Systems/Rendering3D/ArrowStripSystem.h>

SCRIPT(LaunchOrb, "Gimmick");

namespace {
constexpr float kRumbleTime = 0.45f; // 使ったときに震える秒数（水色の円の 0.15 秒より長く。吹っ飛び始めの勢いを手に伝える）
} // namespace

//=============================================================================
// 使ったときに震える秒数
//=============================================================================
float LaunchOrb::GetRumbleTime() const { return kRumbleTime; }

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void LaunchOrb::Fields(ComponentUI& ui) {
	JumpRing::Fields(ui);
	ui.Label("キューブが輪の中でジャンプすると、線の始まりに寄ってから、子の Arrows（Arrow Strip）の線に沿って吹っ飛ぶ。Arrows が無ければ、この Entity の右（Rotation の Z で回る）へまっすぐ");
	ui.Field("Speed", speed_, AtLeast(0.0f).Tip("飛ぶ速さ（m/s。線に沿って）"));
	ui.Field("Duration", duration_, AtLeast(0.0f).Tip("吹っ飛びの長さ（秒）。何にもぶつからなければ、この秒数で普通の空中に戻る（重力が戻る）"));
	ui.Field("End At Path End", endAtPathEnd_, Tip("付ける：線の終わりで吹っ飛びをやめて、その勢いのまま普通の空中へ（曲がった線の先で放り出す）／外す：線の終わりの後は、終わりの向きへまっすぐ（Duration まで）"));
	ui.Field("Pulse Scale", pulseScale_, AtLeast(1.0f).Tip("使ったとき、子の Ring がこの倍にふくらむ"));
	ui.Field("Pulse Time", pulseTime_, AtLeast(0.01f).Tip("ふくらんだ Ring が戻るまでの秒数"));
}

//=============================================================================
// 毎フレーム
//=============================================================================
void LaunchOrb::Start() {
	// 子の見た目と矢印を探す（無ければ、見た目は動かさない・まっすぐ飛ばす）
	ring_ = gameObject.FindChild("Ring");
	arrows_ = gameObject.FindChild("Arrows");
	if (ring_) {
		ringScale_ = ring_.GetTransform().scale;
	}
	pulseTimer_ = pulseTime_; // ふくらんでいない所から
}

void LaunchOrb::Update() {
	pulseTimer_ += Time::GetDeltaTime();
	if (!ring_) {
		return;
	}
	// --- 輪：使った瞬間に Pulse Scale 倍にふくらんで、Pulse Time 秒で元に戻る ---
	const float t = std::clamp(pulseTimer_ / pulseTime_, 0.0f, 1.0f);
	ring_.GetTransform().scale = ringScale_ * MathUtility::Lerp(pulseScale_, 1.0f, t);
}

//=============================================================================
// 飛ぶ線
//=============================================================================
void LaunchOrb::BuildPath(PathCurve& path) const {
	// --- 子の Arrows の線（矢印と同じ線。Arrows の Transform も掛かる）---
	if (arrows_) {
		if (const ArrowStripComponent* strip = arrows_.GetComponent<ArrowStripComponent>()) {
			ArrowStripSystem::BuildPath(*strip, arrows_.GetTransform().worldMatrix, path);
			if (!path.IsEmpty()) {
				return;
			}
		}
	}
	// --- Arrows が無い：真ん中から右へ 1m のまっすぐな線（線の終わりの後も、同じ向きへまっすぐ飛ぶ）---
	const Vector3 center = GetWorldPosition(transform);
	const std::array<Vector3, 2> points = {center, center + GetWorldRight(transform)};
	path.Build(points);
}