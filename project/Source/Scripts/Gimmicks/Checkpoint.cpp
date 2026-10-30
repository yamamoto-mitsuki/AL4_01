#include "Checkpoint.h"

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

SCRIPT(Checkpoint, "Gimmick");

namespace {
// ===== 子の名前（Checkpoint.prefab と同じにする）=====
const std::string kCrystalName = "Crystal";               // 回る・浮く台（直下の子）
const std::string kGemName = "Gem";                       // 宝石（Crystal の子。Primitive Renderer）
const std::string kCoreName = "Core";                     // 芯の光（Crystal の子。Primitive Renderer の Additive）
const std::string kAuraParticleName = "AuraParticle";     // まとい続ける粒（取る前は少なめ。直下の子。Play On Start を外す）
const std::string kBurstParticleName = "BurstParticle";   // はじけた瞬間に飛び散る粒（直下の子。Burst で出す）
const std::string kShockParticleName = "ShockParticle";   // はじけた瞬間に輪に広がる筋（直下の子。Burst で出す）
const std::string kChargeParticleName = "ChargeParticle"; // 復活の前、吸い込まれる粒（直下の子。Converge）

// ===== 音（中間地点の専用の音は無いので、今ある音の高さを変えて使う）=====
constexpr float kActivateChimePitch = 0.75f; // 取った：円の和音を低く（落ち着いた鐘のように）
constexpr float kActivateWhooshPitch = 1.3f; // 取った：吹っ飛びの風の音を高く短く
constexpr float kActivateWhooshVolume = 0.7f;
constexpr float kRespawnWhooshPitch = 0.9f; // 復活：吹っ飛びの風の音を少し低く
constexpr float kRespawnChimePitch = 1.0f;  // 復活：円の和音（低い方）

constexpr float kSameSpotSq = 1.0e-4f; // 覚えた位置と、この距離（0.01m）の2乗より近ければ、同じ中間地点
} // namespace

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void Checkpoint::Fields(ComponentUI& ui) {
	ui.Label("キューブが触れたら取る。やられた・R でやり直すと、ここから（時間は続きから）。ポーズの Restart は最初から");
	ui.Label("親の無い Entity に置く。子：Crystal（中に Gem・Core）/ AuraParticle / BurstParticle / ShockParticle / ChargeParticle");
	// --- 中間地点 ---
	ui.Separator("Checkpoint");
	ui.Field("Is Start", isStart_, Tip("スタート地点にする。はじめから取った見た目で、ステージが始まる（始まりの演出のぼかしが消えてから）・中間地点を取らずにやり直すと、ここからキューブが出てくる。ステージに1つ"));
	ui.Field("Order", order_, AtLeast(1.0f).Tip("順番（ステージの中で、先の物ほど大きく）。もっと先の中間地点を取った後に、手前の物に触れても戻らない"));
	ui.Field("Spawn Offset", spawnOffset_, Tip("復活する位置（この Entity の位置からのずれ。m）。Entity は地面の上の面に置き、Y はキューブの半分より少し上"));
	// --- 取った後の色 ---
	ui.Separator("Lit");
	ui.ColorField("Lit Color", litColor_);
	ui.Field("Lit Intensity", litIntensity_, AtLeast(0.0f).Tip("取った後の Gem・Core の明るさ（1 より大きいとブルームで光る）。取る前は Idle の色・明るさ"));
	ui.Field("Color Time", colorTime_, AtLeast(0.01f).Tip("取ってから、取った後の色に変わりきるまでの秒数"));
	// --- 取る前の色 ---
	ui.Separator("Idle");
	ui.ColorField("Idle Color", idleColor_);
	ui.Field("Idle Intensity", idleIntensity_, AtLeast(0.0f).Tip("取る前の Gem の明るさ（1 より大きいと、うっすらブルームで光る）。夜でも明るい背景でも、ここにあると分かるように"));
	ui.Field("Idle Core Glow", idleCoreGlow_, Range(0.0f, 1.0f).Tip("取る前の芯の光（取った後の何倍か）。0 なら取る前は芯を光らせない"));
	ui.Field("Idle Pulse", idlePulse_, Range(0.0f, 1.0f).Tip("取る前に明るさを揺らす幅（1 ± この割合）。0 なら揺らさない"));
	ui.Field("Idle Pulse Speed", idlePulseSpeed_, AtLeast(0.0f).Tip("揺らす速さ（ラジアン/秒）"));
	ui.Field("Idle Aura Rate", idleAuraRate_, Range(0.0f, 1.0f).Tip("取る前に流す Aura Particle の量（子の Rate Over Time に掛ける）。ここに中間地点があると分かるように。0 なら取る前は流さない"));
	// --- はじける ---
	ui.Separator("Burst");
	ui.Field("Flash Intensity", flashIntensity_, AtLeast(0.0f).Tip("はじけた瞬間（取った・復活）に足す明るさ。Pop Time でだんだん 0 へ"));
	ui.Field("Pop Scale", popScale_, AtLeast(1.0f).Tip("はじけた瞬間の Crystal の大きさの倍率。Pop Time でだんだん 1 へ"));
	ui.Field("Pop Time", popTime_, AtLeast(0.01f).Tip("跳ねた大きさ・光・速い回りが落ち着くまでの秒数"));
	ui.Field("Hit Stop", hitStopTime_, AtLeast(0.0f).Tip("取った瞬間に時間を止める秒数（0 なら止めない。復活では止めない）"));
	ui.Field("Rumble Power", rumblePower_, Range(0.0f, 1.0f).Tip("はじけたときのコントローラーの振動の強さ"));
	ui.Field("Rumble Time", rumbleTime_, AtLeast(0.0f).Tip("震える秒数"));
	// --- 回る・浮く ---
	ui.Separator("Motion");
	ui.Field("Idle Spin Speed", idleSpinSpeed_, Tip("取る前に Crystal が回る速さ（度/秒）"));
	ui.Field("Lit Spin Speed", litSpinSpeed_, Tip("取った後に回る速さ（度/秒）"));
	ui.Field("Spin Boost", spinBoost_, AtLeast(1.0f).Tip("はじけた瞬間、回る速さに掛ける倍率（Pop Time でだんだん 1 へ）"));
	ui.Field("Bob Height", bobHeight_, AtLeast(0.0f).Tip("取った後、上下に浮く幅（m）"));
	ui.Field("Bob Speed", bobSpeed_, AtLeast(0.0f).Tip("浮く速さ（ラジアン/秒）"));
}

//=============================================================================
// 最初に1回 / 毎フレーム
//=============================================================================
// ===== 最初に1回 =====
void Checkpoint::Start() {
	// --- 子の見た目を探して、置いた値（取る前の見た目）を覚える（無ければ、その見た目は動かさない）---
	crystal_ = gameObject.FindChild(kCrystalName);
	if (crystal_) {
		crystalPosition_ = crystal_.GetTransform().translation;
		crystalScale_ = crystal_.GetTransform().scale;
		gem_ = crystal_.FindChild(kGemName);
		core_ = crystal_.FindChild(kCoreName);
	}
	if (core_) {
		if (const PrimitiveRendererComponent* renderer = core_.GetComponent<PrimitiveRendererComponent>()) {
			coreColor_ = renderer->material.color;
		}
	}
	if (const GameObject aura = gameObject.FindChild(kAuraParticleName)) {
		if (const ParticleSystemComponent* particle = aura.GetComponent<ParticleSystemComponent>()) {
			auraRate_ = particle->rateOverTime;
		}
	}
	// --- この中間地点から復活するところなら、はじめから取った後の見た目にして、幕が開くのを待つ ---
	// どの中間地点かは Transform の位置で見分ける（親が無いので、これがワールドの位置。最初のフレームはワールドの位置がまだ無い）
	const LevelInfo* level = LevelInfo::Find();
	const size_t stage = (level != nullptr) ? level->GetStageIndex() : 0;
	if (gCheckpoint.IsRespawning(stage) && LengthSq(transform.translation - gCheckpoint.localPosition) < kSameSpotSq) {
		isActivated_ = true;
		isRespawnPoint_ = true;
		glow_ = 1.0f;
	}
	// --- スタート地点：はじめから取った見た目。ほかの中間地点から復活するのでなければ、ここからキューブを出す ---
	// （キューブは IceCube::Start で、ここへ動いて見えないまま待っている）
	if (isStart_) {
		isActivated_ = true;
		glow_ = 1.0f;
		isRespawnPoint_ = !gCheckpoint.IsRespawning(stage);
	}
}

// ===== 毎フレーム =====
void Checkpoint::Update() {
	const float deltaTime = Time::GetDeltaTime();
	++updateCount_;
	// --- 粒は 2 回目の Update から流す ---
	// ゲームが始まった最初のフレームに Play すると、エンジンがその後で粒の一覧を捨ててしまう（Play On Start は無事）ので、1 フレーム遅らせる
	if (updateCount_ == 2) {
		// 取る前も少なめに流す（どこに中間地点があるか分かるように）。取ったら Activate で置いた量に戻す
		SetAuraRate(isActivated_ ? 1.0f : idleAuraRate_);
		if (isActivated_ || idleAuraRate_ > 0.0f) {
			PlayChild(kAuraParticleName);
		}
		if (isRespawnPoint_) {
			PlayChild(kChargeParticleName); // 幕が開くまで、粒が吸い込まれて力をためる
		}
	}
	// --- 復活：幕が開ききって、始まりの演出も終わったら（ぼかしが消えたら）、はじけてキューブを出す ---
	if (isRespawnPoint_ && updateCount_ >= 2 && !SceneTransition::IsBusy()) {
		const LevelInfo* level = LevelInfo::Find();
		if (level == nullptr || !level->IsIntroPlaying()) {
			PlayRespawn();
		}
	}
	UpdateLook(deltaTime);
}

//=============================================================================
// 取る・復活
//=============================================================================
void Checkpoint::OnTriggerEnter(GameObject other) {
	// --- 早期リターン：キューブでない・やられ・クリアの最中 ---
	const IceCube* cube = other.GetComponent<IceCube>();
	if (cube == nullptr || !cube->IsPlayable()) {
		return;
	}
	if (!isActivated_) {
		Activate();
	}
	// --- 隠し結晶：ここまでに取った結晶を、中間地点に覚えてもらう（もう取った中間地点に、結晶を取って戻ってきたときも）---
	if (const LevelInfo* level = LevelInfo::Find()) {
		level->KeepCrystals();
	}
}

// ===== 取る（覚えて、はじけて、光り始める）=====
void Checkpoint::Activate() {
	const LevelInfo* level = LevelInfo::Find();
	const size_t stage = (level != nullptr) ? level->GetStageIndex() : 0;
	// --- もっと先の中間地点をもう取っていたら、戻らない（取らない）---
	if (gCheckpoint.hasCheckpoint && gCheckpoint.stage == stage && gCheckpoint.order > order_) {
		return;
	}
	// --- 覚える（読み直してもシーンの外に残る）---
	gCheckpoint.hasCheckpoint = true;
	gCheckpoint.stage = stage;
	gCheckpoint.order = order_;
	gCheckpoint.localPosition = transform.translation;
	gCheckpoint.spawnPosition = GetWorldPosition(transform) + spawnOffset_;
	isActivated_ = true;
	// --- 勢い：はじけて、一瞬時間を止める。粒がまとい始める ---
	Burst();
	SetAuraRate(1.0f);
	PlayChild(kAuraParticleName);
	if (hitStopTime_ > 0.0f) {
		Time::HitStop(hitStopTime_);
	}
	GameAudio::PlaySe(Se::RingJump, kActivateChimePitch);
	GameAudio::PlaySe(Se::Launch, kActivateWhooshPitch, kActivateWhooshVolume);
}

// ===== 幕が開いた：はじけて、キューブを出す =====
void Checkpoint::PlayRespawn() {
	isRespawnPoint_ = false;
	StopChild(kChargeParticleName, true); // ためていた粒を消して、はじけた粒に入れ替える
	Burst();
	if (IceCube* cube = FindObjectOfType<IceCube>()) {
		cube->Appear();
	}
	GameAudio::PlaySe(Se::Launch, kRespawnWhooshPitch);
	GameAudio::PlaySe(Se::RingJumpLow, kRespawnChimePitch);
}

// ===== はじける（取った・復活で同じ）=====
void Checkpoint::Burst() {
	popTimer_ = 0.0f; // 宝石が跳ねて、白く光って、速く回る（UpdateLook）
	PlayChild(kBurstParticleName);
	PlayChild(kShockParticleName);
	GameRumble::Play(rumblePower_, rumbleTime_);
}

//=============================================================================
// 見た目
//=============================================================================
void Checkpoint::UpdateLook(float deltaTime) {
	// --- 取った後の色へ、Color Time 秒で変わる ---
	if (isActivated_) {
		glow_ = (std::min)(glow_ + deltaTime / colorTime_, 1.0f);
	}
	const float glow = Ease::OutCubic(glow_, 0.0f, 1.0f);
	// --- はじけた後の落ち着き具合（1：はじけた瞬間 〜 0：落ち着いた）。最初に速く、だんだんゆっくり戻る ---
	popTimer_ += deltaTime;
	const float pop = 1.0f - Ease::OutCubic(std::clamp(popTimer_ / popTime_, 0.0f, 1.0f), 0.0f, 1.0f);
	// --- 取る前の明るさの揺れ（1 ± Idle Pulse。浮く時間と同じ時計で）---
	bobTime_ += deltaTime;
	const float pulse = 1.0f + idlePulse_ * std::sin(bobTime_ * idlePulseSpeed_);

	if (crystal_) {
		TransformComponent& crystal = crystal_.GetTransform();
		// --- 回る：取る前はゆっくり、取った後は速く。はじけた瞬間はもっと速く ---
		const float spinSpeed = MathUtility::Lerp(idleSpinSpeed_, litSpinSpeed_, glow) * (1.0f + (spinBoost_ - 1.0f) * pop);
		spinAngle_ = std::remainder(spinAngle_ + spinSpeed * deltaTime, 360.0f); // −180〜180 に収める（ずっと増え続けないように）
		SetEulerAngles(crystal, {0.0f, spinAngle_, 0.0f});
		// --- 浮く：取った後だけ、上下にゆっくり ---
		crystal.translation = crystalPosition_ + Vector3{0.0f, std::sin(bobTime_ * bobSpeed_) * bobHeight_ * glow, 0.0f};
		// --- はじけた瞬間に大きく跳ねて、だんだん元の大きさへ ---
		crystal.scale = crystalScale_ * (1.0f + (popScale_ - 1.0f) * pop);
	}
	// --- 宝石：Idle Color（淡く少し光る・揺れる）から、Lit Color（明るい・ブルームで光る）へ。はじけた瞬間は白く強く光る ---
	const float flash = flashIntensity_ * pop * pop; // 2乗：光る時間を短く、鋭く
	if (gem_) {
		if (PrimitiveRendererComponent* renderer = gem_.GetComponent<PrimitiveRendererComponent>()) {
			renderer->material.color = MathUtility::Lerp(idleColor_, litColor_, glow);
			renderer->material.intensity = MathUtility::Lerp(idleIntensity_ * pulse, litIntensity_, glow) + flash;
		}
	}
	// --- 芯の光：取る前はうっすら（Idle Core Glow。揺れる）→ 取った後は光る（Additive なので RGB に掛けて明るさを変える）---
	if (core_) {
		if (PrimitiveRendererComponent* renderer = core_.GetComponent<PrimitiveRendererComponent>()) {
			const float core = MathUtility::Lerp(idleCoreGlow_ * pulse, 1.0f, glow);
			renderer->material.color = {coreColor_.x * core, coreColor_.y * core, coreColor_.z * core, coreColor_.w};
			renderer->material.intensity = litIntensity_ + flash;
		}
	}
}

// ===== 子の粒 =====
void Checkpoint::SetAuraRate(float scale) const {
	if (const GameObject aura = gameObject.FindChild(kAuraParticleName)) {
		if (ParticleSystemComponent* particle = aura.GetComponent<ParticleSystemComponent>()) {
			particle->rateOverTime = auraRate_ * scale;
		}
	}
}

void Checkpoint::PlayChild(const std::string& name) const {
	if (const GameObject child = gameObject.FindChild(name)) {
		ParticleSystem::Play(child.handle);
	}
}

void Checkpoint::StopChild(const std::string& name, bool clear) const {
	if (const GameObject child = gameObject.FindChild(name)) {
		ParticleSystem::Stop(child.handle, clear);
	}
}
