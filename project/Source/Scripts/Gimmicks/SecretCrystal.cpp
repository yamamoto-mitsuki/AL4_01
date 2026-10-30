#include "SecretCrystal.h"

#include <algorithm>
#include <cmath>

#include <MyEngine/Components/Effects/ParticleSystemComponent.h>
#include <MyEngine/Math/Easing.h>
#include <MyEngine/Math/MathUtility.h>
#include <MyEngine/Systems/Effects/ParticleSystem.h>

#include "Source/Scripts/Audio/GameAudio.h"
#include "Source/Scripts/Effects/GameRumble.h"
#include "Source/Scripts/IceCube.h"
#include "Source/Scripts/LevelInfo.h"
#include "Source/Scripts/Progress/CheckpointState.h"
#include "Source/Scripts/Progress/GameProgress.h"

SCRIPT(SecretCrystal, "Gimmick");

namespace {
// ===== 子の名前（SecretCrystal.prefab と同じにする）=====
const std::string kBodyName = "Body";                   // 回る・浮く・昇る台（直下の子）
const std::string kGemName = "Gem";                     // 結晶（Body の子。Primitive Renderer）
const std::string kCoreName = "Core";                   // 芯の光（Body の子。Primitive Renderer の Additive）
const std::string kGlowParticleName = "GlowParticle";   // いつもまとっている粒（直下の子。Play On Start を外す）
const std::string kBurstParticleName = "BurstParticle"; // 消える瞬間に丸く散る粒（直下の子。Burst で出す）
const std::string kSparkParticleName = "SparkParticle"; // 消える瞬間に飛び散る筋（直下の子。Burst で出す）
} // namespace

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void SecretCrystal::Fields(ComponentUI& ui) {
	ui.Label("キューブが触れたら取る。回りながら昇って、はじけて消える。ゴールしたら記録（取った後にやられたら、取っていない扱い）。前に取った結晶は灰色の薄い姿で残る");
	ui.Label("子：Body（中に Gem・Core）/ GlowParticle / BurstParticle / SparkParticle。同じステージの結晶は Id を別々に");
	// --- 結晶 ---
	ui.Separator("Crystal");
	ui.Field("Id", id_, AtLeast(1.0f).Tip("ステージの中で見分ける番号（取った記録に使う）。同じステージに置く結晶は別々の番号に。置いた後に変えると、取った記録が別の結晶の物になる"));
	// --- 光る ---
	ui.Separator("Glow");
	ui.Field("Pulse Amount", pulseAmount_, Range(0.0f, 1.0f).Tip("明るさの揺れ（Gem・Core に置いた明るさの何割を、増やしたり減らしたりするか）。0 なら揺れない"));
	ui.Field("Pulse Speed", pulseSpeed_, AtLeast(0.0f).Tip("明るさの揺れる速さ（ラジアン/秒）"));
	// --- 待っている間 ---
	ui.Separator("Idle");
	ui.Field("Idle Spin Speed", idleSpinSpeed_, Tip("回る速さ（度/秒）"));
	ui.Field("Bob Height", bobHeight_, AtLeast(0.0f).Tip("上下に浮く幅（m）"));
	ui.Field("Bob Speed", bobSpeed_, AtLeast(0.0f).Tip("浮く速さ（ラジアン/秒）"));
	// --- 前に取った結晶 ---
	ui.Separator("Ghost");
	ui.ColorField("Ghost Color", ghostColor_);
	ui.Field("Ghost Intensity", ghostIntensity_, AtLeast(0.0f).Tip("前に取った結晶の Gem・Core の明るさ（1 より小さいとブルームで光らない）。色は Ghost Color（灰色。A で透ける）"));
	ui.ColorField("Ghost Particle Color", ghostParticleColor_);
	ui.Field("Ghost Particle Intensity", ghostParticleIntensity_, AtLeast(0.0f).Tip("前に取った結晶の GlowParticle の明るさ。色は Ghost Particle Color"));
	ui.Field("Ghost Glow Rate", ghostGlowRate_, AtLeast(0.0f).Tip("前に取った結晶の GlowParticle の出る数に掛ける倍率"));
	// --- 取った：昇る ---
	ui.Separator("Rise");
	ui.Field("Rise Height", riseHeight_, AtLeast(0.0f).Tip("取った後に昇る高さ（m）"));
	ui.Field("Rise Time", riseTime_, AtLeast(0.01f).Tip("昇りきるまでの秒数（最初に速く、だんだんゆっくり）"));
	ui.Field("Rise Spin Speed", riseSpinSpeed_, Tip("昇りきる時の回る速さ（度/秒）。Idle Spin Speed から、だんだん速くなる"));
	ui.Field("Rise Scale", riseScale_, AtLeast(0.0f).Tip("昇りきる時の大きさの倍率"));
	ui.Field("Rise Intensity", riseIntensity_, AtLeast(0.0f).Tip("昇りきる時に足す明るさ（だんだん増える）"));
	ui.Field("Rise Glow Rate", riseGlowRate_, AtLeast(0.0f).Tip("昇っている間、GlowParticle の出る数に掛ける倍率"));
	// --- 取った：消える ---
	ui.Separator("Vanish");
	ui.Field("Vanish Time", vanishTime_, AtLeast(0.01f).Tip("はじけてから、縮んで消えるまでの秒数"));
	ui.Field("Rumble Power", rumblePower_, Range(0.0f, 1.0f).Tip("取った瞬間・はじけた瞬間のコントローラーの振動の強さ"));
	ui.Field("Rumble Time", rumbleTime_, AtLeast(0.0f).Tip("震える秒数"));
}

//=============================================================================
// 最初に1回 / 毎フレーム
//=============================================================================
// ===== 最初に1回 =====
void SecretCrystal::Start() {
	// --- 子の見た目を探して、置いた値を覚える（無ければ、その見た目は動かさない）---
	body_ = gameObject.FindChild(kBodyName);
	if (body_) {
		bodyPosition_ = body_.GetTransform().translation;
		bodyScale_ = body_.GetTransform().scale;
		gem_ = body_.FindChild(kGemName);
		core_ = body_.FindChild(kCoreName);
	}
	if (gem_) {
		if (const PrimitiveRendererComponent* renderer = gem_.GetComponent<PrimitiveRendererComponent>()) {
			gemColor_ = renderer->material.color;
			gemIntensity_ = renderer->material.intensity;
		}
	}
	if (core_) {
		if (const PrimitiveRendererComponent* renderer = core_.GetComponent<PrimitiveRendererComponent>()) {
			coreIntensity_ = renderer->material.intensity;
		}
	}
	if (const GameObject glow = gameObject.FindChild(kGlowParticleName)) {
		if (const ParticleSystemComponent* particle = glow.GetComponent<ParticleSystemComponent>()) {
			glowRate_ = particle->rateOverTime;
		}
	}
	spinSpeed_ = idleSpinSpeed_;

	// --- 前に取った（取ってクリアした）結晶なら、灰色の薄い姿にする。粒も灰色で薄く ---
	const LevelInfo* level = LevelInfo::Find();
	const size_t stage = (level != nullptr) ? level->GetStageIndex() : 0;
	if (GameProgress::IsCrystalCollected(stage, id_)) {
		BecomeGhost();
		return;
	}
	// --- 中間地点までに取って、中間地点から復活した：取った扱いのままなので、消えた姿で始める（粒も出さない）---
	if (gCheckpoint.HasCrystal(stage, id_)) {
		state_ = State::Gone;
		if (body_) {
			body_.GetTransform().scale = {0.0f, 0.0f, 0.0f};
		}
	}
}

// ===== 前に取った結晶の姿にする =====
void SecretCrystal::BecomeGhost() {
	state_ = State::Ghost;
	// --- Gem・Core：灰色で透ける。明るさは UpdateLook が、この値からゆっくり揺らす ---
	gemIntensity_ = ghostIntensity_;
	coreIntensity_ = ghostIntensity_;
	if (gem_) {
		if (PrimitiveRendererComponent* renderer = gem_.GetComponent<PrimitiveRendererComponent>()) {
			renderer->material.color = ghostColor_;
		}
	}
	if (core_) {
		if (PrimitiveRendererComponent* renderer = core_.GetComponent<PrimitiveRendererComponent>()) {
			renderer->material.color = ghostColor_;
		}
	}
	// --- まとう粒：灰色で、薄く、少なく（流し始めるのは、いつもと同じ 2 回目の Update）---
	if (const GameObject glow = gameObject.FindChild(kGlowParticleName)) {
		if (ParticleSystemComponent* particle = glow.GetComponent<ParticleSystemComponent>()) {
			particle->colorA = ghostParticleColor_;
			particle->colorB = ghostParticleColor_;
			particle->intensity = ghostParticleIntensity_;
			particle->rateOverTime = glowRate_ * ghostGlowRate_;
		}
	}
}

// ===== 毎フレーム =====
void SecretCrystal::Update() {
	const float deltaTime = Time::GetDeltaTime();
	++updateCount_;
	// --- 粒は 2 回目の Update から流す ---
	// ゲームが始まった最初のフレームに Play すると、エンジンがその後で粒の一覧を捨ててしまう（Play On Start は無事）ので、1 フレーム遅らせる
	if (updateCount_ == 2 && (state_ == State::Idle || state_ == State::Ghost)) {
		PlayChild(kGlowParticleName);
	}

	// --- 昇る・消える の進み ---
	switch (state_) {
	case State::Rising:
		timer_ += deltaTime;
		if (timer_ >= riseTime_) {
			Vanish();
		}
		break;
	case State::Vanishing:
		timer_ += deltaTime;
		if (timer_ >= vanishTime_) {
			state_ = State::Gone;
			if (body_) {
				body_.GetTransform().scale = {0.0f, 0.0f, 0.0f}; // 見えなくする（Entity は消さない。粒がまだ飛んでいるので）
			}
		}
		break;
	default:
		break;
	}
	if (state_ != State::Gone) {
		UpdateLook(deltaTime);
	}
}

//=============================================================================
// 取る・消える
//=============================================================================
void SecretCrystal::OnTriggerEnter(GameObject other) {
	// --- 早期リターン：もう取った・前に取った・キューブでない・やられ・クリアの最中・まだ見えないまま待っている ---
	if (state_ != State::Idle) {
		return;
	}
	const IceCube* cube = other.GetComponent<IceCube>();
	if (cube == nullptr || !cube->IsPlayable() || cube->IsWaitingRespawn()) {
		return;
	}
	Collect();
}

void SecretCrystal::OnTriggerStay(GameObject other) {
	// 重なったまま取れるようになった時も取る（復活した所の近くに置いた結晶に、見えないまま重なっていた・始まりの演出の間など）
	OnTriggerEnter(other);
}

// ===== 取る（LevelInfo に覚えてもらって、昇り始める）=====
void SecretCrystal::Collect() {
	// --- 取った記録は、ゴールした時に保存する（LevelInfo::Clear）。この後やられると、読み直しで消える ---
	if (LevelInfo* level = LevelInfo::Find()) {
		level->CollectCrystal(id_);
	}
	state_ = State::Rising;
	timer_ = 0.0f;
	// --- 昇っている間は、まとう粒を増やす（光の尾を引いて昇る）---
	if (const GameObject glow = gameObject.FindChild(kGlowParticleName)) {
		if (ParticleSystemComponent* particle = glow.GetComponent<ParticleSystemComponent>()) {
			particle->rateOverTime = glowRate_ * riseGlowRate_;
		}
	}
	GameRumble::Play(rumblePower_, rumbleTime_);
	GameAudio::PlaySe(Se::Crystal);
}

// ===== 昇りきった：はじけて、縮み始める =====
void SecretCrystal::Vanish() {
	state_ = State::Vanishing;
	timer_ = 0.0f;
	// --- はじける粒を、今の結晶の位置（昇った所）から出す ---
	const Vector3 position = body_ ? body_.GetTransform().translation : bodyPosition_;
	SetChildTranslation(kBurstParticleName, position);
	SetChildTranslation(kSparkParticleName, position);
	PlayChild(kBurstParticleName);
	PlayChild(kSparkParticleName);
	StopChild(kGlowParticleName, false); // まとっていた粒は、出ている物だけ残して消えていく
	GameRumble::Play(rumblePower_, rumbleTime_);
	GameAudio::PlaySe(Se::CrystalBurst);
}

//=============================================================================
// 見た目
//=============================================================================
void SecretCrystal::UpdateLook(float deltaTime) {
	// ===== 進み具合 =====
	// rise：昇る（0：取った瞬間 〜 1：昇りきった）。昇った後（Vanishing）は 1 のまま
	// vanish：消える（0：はじけた瞬間 〜 1：消えた）
	float rise = 0.0f;
	float vanish = 0.0f;
	if (state_ == State::Rising) {
		rise = std::clamp(timer_ / riseTime_, 0.0f, 1.0f);
	} else if (state_ == State::Vanishing) {
		rise = 1.0f;
		vanish = std::clamp(timer_ / vanishTime_, 0.0f, 1.0f);
	}
	const bool isCollected = (state_ == State::Rising || state_ == State::Vanishing);

	// ===== Body：回る・浮く・昇る・大きさ =====
	if (body_) {
		TransformComponent& body = body_.GetTransform();
		// --- 回る：取ったら、だんだん速く（InQuad：最後にぐっと速くなる）。消える間は昇りきった時の速さのまま ---
		if (state_ == State::Rising) {
			spinSpeed_ = Ease::InQuad(rise, idleSpinSpeed_, riseSpinSpeed_);
		}
		spinAngle_ = std::remainder(spinAngle_ + spinSpeed_ * deltaTime, 360.0f); // −180〜180 に収める（ずっと増え続けないように）
		SetEulerAngles(body, {0.0f, spinAngle_, 0.0f});
		// --- 浮く：待っている間だけ上下に。取ったら止めて、昇る（OutCubic：最初に速く、だんだんゆっくり）---
		bobTime_ += deltaTime;
		const float bob = isCollected ? 0.0f : std::sin(bobTime_ * bobSpeed_) * bobHeight_;
		if (state_ == State::Rising) {
			riseOffset_ = Ease::OutCubic(rise, 0.0f, riseHeight_);
		}
		body.translation = bodyPosition_ + Vector3{0.0f, bob + riseOffset_, 0.0f};
		// --- 大きさ：昇るにつれて少し大きく、はじけたら縮んで消える（InQuad：最初はゆっくり、最後に一気に）---
		const float grow = MathUtility::Lerp(1.0f, riseScale_, Ease::OutCubic(rise, 0.0f, 1.0f));
		const float shrink = 1.0f - Ease::InQuad(vanish, 0.0f, 1.0f);
		body.scale = bodyScale_ * (grow * shrink);
		// --- まとう粒も結晶に付いていく（昇る間、光の尾を引く）---
		SetChildTranslation(kGlowParticleName, body.translation);
	}

	// ===== 明るさ：いつも光って、ゆっくり揺れる。昇るにつれて強く =====
	// 前に取った結晶は、BecomeGhost で入れた灰色の明るさ（Ghost Intensity）から揺れる
	const float pulse = 1.0f + pulseAmount_ * std::sin(bobTime_ * pulseSpeed_);
	const float boost = riseIntensity_ * Ease::InQuad(rise, 0.0f, 1.0f);
	if (gem_) {
		if (PrimitiveRendererComponent* renderer = gem_.GetComponent<PrimitiveRendererComponent>()) {
			renderer->material.intensity = gemIntensity_ * pulse + boost;
		}
	}
	if (core_) {
		if (PrimitiveRendererComponent* renderer = core_.GetComponent<PrimitiveRendererComponent>()) {
			renderer->material.intensity = coreIntensity_ * pulse + boost;
		}
	}
}

// ===== 子 =====
void SecretCrystal::SetChildTranslation(const std::string& name, const Vector3& translation) const {
	if (const GameObject child = gameObject.FindChild(name)) {
		child.GetTransform().translation = translation;
	}
}

void SecretCrystal::PlayChild(const std::string& name) const {
	if (const GameObject child = gameObject.FindChild(name)) {
		ParticleSystem::Play(child.handle);
	}
}

void SecretCrystal::StopChild(const std::string& name, bool clear) const {
	if (const GameObject child = gameObject.FindChild(name)) {
		ParticleSystem::Stop(child.handle, clear);
	}
}
