#include "DistanceTrail.h"

#include <cmath>

#include <MyEngine/Systems/ParticleSystem.h>

SCRIPT(DistanceTrail, "Effects");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void DistanceTrail::Fields(ComponentUI& ui) {
	ui.Label("同じ Entity の Particle System を、進んだ長さに合わせて出す（Play On Start を外し、Rate Over Time・Burst Count を 0、Simulation Space を World に）");
	ui.Field("Per Meter", perMeter_, AtLeast(0.0f).Tip("1m 進むごとに出す粒の数（多いほど、跡が詰まって線のように見える）"));
	ui.Field("Min Speed", minSpeed_, AtLeast(0.0f).Tip("これより遅いときは出さない（m/s）。止まりかけ・溶けて沈むときに、粒がたまらないように"));
	ui.Field("Max Step", maxStep_, AtLeast(0.0f).Tip("1フレームにこれより長く動いたら出さない（m）。やり直し・場所を飛んだときに、飛んだ間に粒が並ばないように"));
	ui.Field("Ground Only", groundOnly_, Tip("地面に乗っている間だけ出す（自分か親の Rigidbody の Is Grounded を見る）。外すと、空中でも動けば出る"));
}

//=============================================================================
// 地面を見る Rigidbody
//=============================================================================
const RigidbodyComponent* DistanceTrail::FindRigidbody() const {
	// 自分に付いていれば、それ（Rigidbody と同じ Entity に付けたとき）
	if (const RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>()) {
		return rigidbody;
	}
	// 無ければ親の（キューブの子にして、足元へずらして置いたとき）
	// GameObject には親を返す関数が無いので、Entity（名前・親などの元の情報）を EntityManager から読む
	const Entity* self = EntityManager::Get(gameObject.handle);
	if (self == nullptr) {
		return nullptr;
	}
	return EntityManager::Get<RigidbodyComponent>(self->parent);
}

//=============================================================================
// 最初に1回 / 毎フレーム
//=============================================================================
// ===== 最初に1回 =====
void DistanceTrail::Start() { previousPosition_ = GetWorldPosition(transform); }

// ===== 毎フレーム =====
void DistanceTrail::Update() {
	const float deltaTime = Time::GetDeltaTime();
	if (deltaTime <= 0.0f) {
		return; // ヒットストップ・ポーズ中（動いていないので出さない。位置も覚え直さない）
	}
	// --- このフレームに進んだ長さ ---
	const Vector3 position = GetWorldPosition(transform);
	const float step = Length(position - previousPosition_);
	previousPosition_ = position;
	// --- 早期リターン：場所を飛んだ（1フレームで Max Step より長い）・遅すぎる・地面にいない（Ground Only）---
	// 端数も捨てる（次に動き出したとき・着地したとき、前の端数で1つ余分に出ないように）
	// Is Grounded は前のフレームの物理の結果（1フレーム遅れるが、粒には分からない）
	const RigidbodyComponent* rigidbody = groundOnly_ ? FindRigidbody() : nullptr;
	const bool isAirborne = groundOnly_ && (rigidbody == nullptr || !rigidbody->isGrounded);
	if (step > maxStep_ || step / deltaTime < minSpeed_ || isAirborne) {
		carry_ = 0.0f;
		return;
	}
	// --- 進んだ長さ × Per Meter の数だけ出す（端数は次のフレームへ持ち越す。フレームの速さで出る数が変わらない）---
	// 例：Per Meter 8・1フレームに 0.1m 進んだ → 0.8 個 → このフレームは 0 個で、0.8 を持ち越す → 次のフレームで 1.6 → 1 個出して 0.6 を持ち越す
	carry_ += step * perMeter_;
	const float count = std::floor(carry_);
	carry_ -= count;
	if (count > 0.0f) {
		// Emit は Particle System の Shape の中から出す（Cone の Radius を足元の幅くらいにすると、ばらけて自然になる）
		ParticleSystem::Emit(gameObject.handle, static_cast<int>(count));
	}
}