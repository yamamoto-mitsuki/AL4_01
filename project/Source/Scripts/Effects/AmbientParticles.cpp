#include "AmbientParticles.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <MyEngine/Camera/Camera.h>
#include <MyEngine/Components/ParticleSystemComponent.h>
#include <MyEngine/Systems/CameraSystem.h>
#include <MyEngine/Systems/ParticleSystem.h>

SCRIPT(AmbientParticles, "Effects");

namespace {
constexpr float kSwayRatioY = 1.37f;  // 上下の揺れの速さ（左右の何倍か。同じだと、斜めの線の上を行き来するだけになる）
constexpr float kSwayOffsetY = 1.0f;  // 上下の揺れの始まりのずれ（ラジアン。左右と同時に折り返さないように）
constexpr int kPrewarmFrame = 2;      // Prewarm を出すフレーム（ゲームが始まった最初のフレームに出した粒は、エンジンが捨てるので、その次）
constexpr float kMinSwayPeriod = 0.1f; // Sway Period の下限（0 で割らないように）
} // namespace

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void AmbientParticles::Fields(ComponentUI& ui) {
	ui.Label("同じ Entity の Particle System を、カメラの前（見えている範囲）に出して、ふわふわ漂わせる。親の無い Entity に付ける（位置はここが毎フレーム決める）。見た目と量は Particle System の方で");
	ui.Field("Depth", depth_, AtLeast(0.0f).Tip("粒が出る奥行き（カメラから何 m。Min〜Max）。ゲームの面より手前にも出すと、粒が前を横切って奥行きが出る"));
	ui.Field("Margin", margin_, AtLeast(1.0f).Tip("見えている範囲の何倍の広さに出すか。1 より大きいと、画面の外で出た粒が端から流れ込んでくる"));
	ui.Field("Drift", drift_, Tip("漂う向きと速さ（m/s）。雪なら Y を負（下へ）、蛍なら Y を少し正（上へ）"));
	ui.Field("Sway", sway_, AtLeast(0.0f).Tip("揺れの大きさ（m/s。X：左右・Y：上下）。0 なら揺れずに Drift の向きへまっすぐ"));
	ui.Field("Sway Period", swayPeriod_, AtLeast(kMinSwayPeriod).Tip("揺れの1往復の秒数。長いほど、ゆったり揺れる"));
	ui.Field("Follow Rate", followRate_, AtLeast(0.01f).Tip("目標の速さ（Drift ＋ 揺れ）に近づく強さ（1/秒）。小さいほど、ふわっと遅れて揺れ、出たときの向き（Speed）が長く残る"));
	ui.Field("Prewarm", prewarm_, AtLeast(0.0f).Tip("始まってすぐ出しておく数（Rate Over Time × Lifetime の Min に掛ける）。0 なら、始まりは粒が無く、だんだん増える"));
}

//=============================================================================
// 毎フレーム
//=============================================================================
void AmbientParticles::Update() {
	ParticleSystemComponent* particle = GetComponent<ParticleSystemComponent>();
	if (particle == nullptr) {
		return; // Inspector で外した
	}
	const Camera* camera = CameraSystem::GetMainCamera();
	const float deltaTime = Time::GetDeltaTime();

	// ===== 置き場所：カメラの前の、奥行きの真ん中 =====
	// ビュー行列の3列目が、カメラの前の向き（DepthOfFieldFade と同じ取り方）
	const Matrix4x4& view = camera->GetViewMatrix();
	const Vector3 forward = {view.m[0][2], view.m[1][2], view.m[2][2]};
	const float nearDepth = (std::min)(depth_.x, depth_.y);
	const float farDepth = (std::max)(depth_.x, depth_.y);
	transform.translation = camera->GetTranslation() + forward * ((nearDepth + farDepth) * 0.5f);

	// ===== 出る場所：一番奥（Depth の Max）で見えている広さの箱 =====
	// 見えている高さ ＝ 2 × 奥行き × tan(縦の視野角 ÷ 2)、幅 ＝ 高さ × 画面の横長さ
	// 箱は Entity の向き（回していなければワールドの軸）のまま。このゲームのカメラは +Z を向いて回らないので、そのまま合う
	const float viewHeight = 2.0f * farDepth * std::tan(camera->GetFovY() * 0.5f) * margin_;
	particle->shape = ParticleShape::Box;
	particle->boxSize = {viewHeight * camera->GetAspectRatio(), viewHeight, farDepth - nearDepth};
	particle->fromSurface = false;
	particle->converge = false;
	particle->space = ParticleSpace::World; // 出た所に残る（Entity はカメラと一緒に動くが、粒は付いていかない）

	// ===== 動き：「Drift ＋ 揺れ」の速さへ、ゆっくり近づく =====
	// Particle System は毎フレーム「速さ += Gravity × 秒」「速さ *= (1 − Drag × 秒)」をするので、
	// Gravity ＝ 目標の速さ × Drag にすると、速さが目標へ近づいてそこで落ち着く（WindParticles と同じ）
	// 目標を毎フレーム sin で揺らすと、全部の粒がそよ風に吹かれるように、ゆっくり左右・上下に揺れる
	swayTime_ += deltaTime; // ポーズ・ヒットストップ中は 0（揺れも止める）
	const float phase = swayTime_ * 2.0f * std::numbers::pi_v<float> / (std::max)(swayPeriod_, kMinSwayPeriod);
	const Vector3 sway = {sway_.x * std::sin(phase), sway_.y * std::sin(phase * kSwayRatioY + kSwayOffsetY), 0.0f};
	particle->drag = followRate_;
	particle->gravity = (drift_ + sway) * followRate_;

	// ===== 始まってすぐ、画面に粒を出しておく（Prewarm）=====
	// Play On Start だけだと、始まりは画面に粒が無く、Lifetime の秒数をかけて増えていく
	if (isPrewarmed_ || deltaTime <= 0.0f) {
		return;
	}
	++frameCount_;
	if (frameCount_ < kPrewarmFrame) {
		return;
	}
	isPrewarmed_ = true;
	const float count = (std::max)(0.0f, particle->rateOverTime) * (std::min)(particle->lifetime.x, particle->lifetime.y) * prewarm_;
	if (count >= 1.0f) {
		ParticleSystem::Emit(gameObject.handle, static_cast<int>(count)); // Max Particles を越える分は、エンジンが出さない
	}
}
