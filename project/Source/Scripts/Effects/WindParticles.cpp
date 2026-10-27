#include "WindParticles.h"

#include <algorithm>
#include <cmath>

#include <MyEngine/Components/ParticleSystemComponent.h>
#include <MyEngine/Components/WindZoneComponent.h>
#include <MyEngine/Systems/ParticleSystem.h>
#include <MyEngine/Systems/WindZoneSystem.h>

SCRIPT(WindParticles, "Effects");

namespace {
constexpr float kMaxGustStrength = 3.0f; // 強さの上限（風が重なって速くなりすぎても、筋が伸びすぎないように）
}

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void WindParticles::Fields(ComponentUI& ui) {
	ui.Label("同じ Entity の Particle System を、Wind Zone に合わせて流す（Play On Start を外す。数は Rate Over Time、見た目は Particle System の方で）");
	ui.Field("Follow Rate", followRate_, AtLeast(0.1f).Tip("粒が風の速さに追いつく強さ（1/秒）。大きいほど、出た瞬間から風の速さで流れる。小さいと、ふわっと動き出す"));
	ui.Field("Life Rate", lifeRate_, AtLeast(0.0f).Tip("粒の寿命（範囲の端から端まで、風の速さで流れる秒数に掛ける。Min〜Max）。1 なら範囲を流れ切るくらい"));
	ui.Field("Show Gust", showGust_, Tip("今の風の強さ（Wind Zone の Speed の何倍か）で、筋の長さ（Stretch）・明るさ（Intensity）・出る数（Rate Over Time）を変える。弱まると短く暗く少なく、強まると長く明るく多くなる"));
}

//=============================================================================
// 最初に1回
//=============================================================================
void WindParticles::Start() {
	// Show Gust で変える前の値を覚える（毎フレーム、ここに今の風の強さを掛けて書く）
	if (const ParticleSystemComponent* particle = GetComponent<ParticleSystemComponent>()) {
		baseStretch_ = particle->stretch;
		baseIntensity_ = particle->intensity;
	}
}

//=============================================================================
// 毎フレーム
//=============================================================================
void WindParticles::Update() {
	const WindZoneComponent* wind = GetComponent<WindZoneComponent>();
	ParticleSystemComponent* particle = GetComponent<ParticleSystemComponent>();
	if (wind == nullptr || particle == nullptr) {
		return; // どちらかが無い（Inspector で外した）
	}
	const float deltaTime = Time::GetDeltaTime();
	if (deltaTime <= 0.0f) {
		return; // ヒットストップ・ポーズ中（出さない）
	}
	// --- 風が止まっている（Wind Zone の Enabled を外した）間は出さない（出ている粒は寿命まで流れる）---
	if (!wind->enabled || wind->speed <= 0.0f) {
		carry_ = 0.0f;
		return;
	}

	// ===== 風の速さ（向き × 速さ。ゆれ込み）=====
	// 範囲の真ん中の風を読む（Sample は前のフレームの範囲を使うので、最初のフレームは 0 → そのときは自分の右向き × Speed）
	const WindZoneSystem::WindSample sample = WindZoneSystem::Sample(GetWorldPosition(transform));
	const Vector3 windVelocity = sample.IsBlowing() ? sample.velocity : GetWorldRight(transform) * wind->speed;

	// ===== 風の強さを見た目に出す（Show Gust）=====
	// 強さ＝今の風の速さ ÷ Wind Zone の Speed（ゆれが無ければ 1。Gust 0.8 なら 0.2〜1.8 の間で変わる）
	// Stretch と Intensity は描くときに読まれるので、もう出ている粒も、すぐに伸び縮み・明るさが変わる
	const float gustStrength = std::clamp(Length(windVelocity) / wind->speed, 0.0f, kMaxGustStrength);
	if (showGust_) {
		particle->stretch = (std::max)(baseStretch_ * gustStrength, 1.0f); // 1 より短いと、筋ではなく丸い粒になる
		// 明るさは強さの2乗（弱い風では筋がほとんど消え、強い風でははっきり光る。止んだ・吹いたが一目で分かる）
		particle->intensity = baseIntensity_ * gustStrength * gustStrength;
	}

	// ===== 出る場所：Wind Zone と同じ箱（Entity の Scale・Rotation は Particle System も同じように掛ける）=====
	particle->shape = ParticleShape::Box;
	particle->boxSize = wind->size;
	particle->fromSurface = false;
	particle->space = ParticleSpace::World; // 出た所から流れていく（Entity と一緒に動かさない）
	particle->converge = false;

	// ===== 動き：出た瞬間は止まっていて、風の速さまで加速して、そこで止まる =====
	// Particle System は毎フレーム「速さ += Gravity × 秒」「速さ *= (1 − Drag × 秒)」をする
	// 速さが v のとき、1秒あたり Gravity だけ増えて、Drag × v だけ減る。増える分と減る分が同じになる速さ（Gravity ÷ Drag）で止まる
	// → Gravity ＝ 風の速さ × Drag にすると、ちょうど風の速さで流れる（Drag が大きいほど早く追いつく）
	particle->speed = {0.0f, 0.0f}; // Box の Speed は「真ん中から外へ」なので使わない（風と関係ない向きに散らないように）
	particle->drag = followRate_;
	particle->gravity = windVelocity * followRate_;

	// ===== 寿命：範囲の端から端まで（ローカルの X の長さ）を、風の速さで流れる秒数 × Life Rate =====
	// 範囲の X の長さは Size.x × Scale.x（ワールド行列の1行目の長さが、X の Scale）
	const Matrix4x4& world = transform.worldMatrix;
	const float scaleX = Length(Vector3{world.m[0][0], world.m[0][1], world.m[0][2]});
	const float crossTime = wind->size.x * scaleX / wind->speed;
	particle->lifetime = lifeRate_ * crossTime;

	// ===== 出す：Rate Over Time の数を、1秒かけて（端数は次のフレームへ持ち越す）=====
	// ParticleSystem::Play（や Play On Start）で流さず、ここで Emit する理由：
	// 「風が止まっている間は出さない」を、上の早期リターンだけで毎フレーム決められる
	// （Play で流すと、Wind Zone の Enabled を外したときに Stop、戻したときに Play を、切り替わりを覚えて呼び分けることになる）
	const float rateScale = showGust_ ? gustStrength : 1.0f; // Show Gust なら、強い風ほど多く出す
	carry_ += (std::max)(0.0f, particle->rateOverTime) * rateScale * deltaTime;
	const float count = std::floor(carry_);
	carry_ -= count;
	if (count > 0.0f) {
		ParticleSystem::Emit(gameObject.handle, static_cast<int>(count));
	}
}