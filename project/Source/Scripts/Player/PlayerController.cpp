#include "PlayerController.h"

#include <cmath>
#include <format>
#include <string_view>

#include <HTTEngine/Components/Combat/HealthComponent.h>
#include <HTTEngine/Curve/Curve.h>
#include <HTTEngine/Curve/CurveManager.h>
#include <HTTEngine/Diagnostics/LogManager.h>
#include <HTTEngine/Input/InputManager.h>
#include <HTTEngine/Math/Easing.h>
#include <HTTEngine/Systems/Animation/AnimationSystem.h>
#include <HTTEngine/Systems/Combat/HitSystem.h>
#include <HTTEngine/Systems/Physics/PhysicsSystem.h>

#include "JustCounter.h"

SCRIPT(PlayerController, "Player");

namespace {
// ===== アニメの名前（Player.gltf の中の名前。Blender のアクションの名前）=====
constexpr std::string_view kClipIdle = "Idle";           // 待機
constexpr std::string_view kClipRun = "Run";             // 走り
constexpr std::string_view kClipHit = "Hit";             // のけぞり
constexpr std::string_view kClipDown = "Down";           // 倒れる
constexpr std::string_view kClipDodgeJump = "DodgeJump"; // 回避

// ===== 入力 =====
constexpr float kMoveInputDeadzone = 0.1f; // これより小さい入力は「止まっている」扱い（スティックを離したときの小さな揺れ）
constexpr float kRunAnimMinSpeed = 0.5f;   // スティックを少しだけ倒したときの走りのアニメの速さ（倒しきると 1）
} // namespace


//=============================================================================
// 初期化・更新
//=============================================================================
// ===== 初期化 =====
void PlayerController::Start() { 
	facingYaw_ = GetEulerAngles(transform).y;  // 置いたときの向きから始める
	AnimationSystem::Play(gameObject.handle, kClipIdle, true, 1.0f);
}

// ===== 更新 =====
void PlayerController::Update() { 
	const float dt = Time::GetDeltaTime();

	// --- 早期リターン ---
	if (dt <= 0.0f) {
		return; // ヒットストップ・ポーズ
	}
	CharacterControllerComponent* controller = GetComponent<CharacterControllerComponent>();
	if (controller == nullptr) {
		return; // CharacterController を付け忘れている
	}

	// --- 共通処理 ---
	dodgeCooldownTimer_ = (std::max)(0.0f, dodgeCooldownTimer_ - dt); // 回避のクールダウンを減らす

	// --- 状態ごとの処理 ---
	const Vector3 moveDirection = CalcMoveDirection();
	switch (state_) { 
	case PlayerState::Move:
		UpdateMove(*controller, moveDirection, dt);
		break;
	case PlayerState::Dodge:
		UpdateDodge(*controller, dt);
		break;
	case PlayerState::Hit:
		UpdateHit(*controller, dt);
		break;
	case PlayerState::Down:
		DampHorizontalVelocity(*controller, {0.0f, 0.0f, 0.0f}, dt); // 倒れた所で止まる（押し戻しの勢いだけ消していく）
		break;
	}
	UpdateFacing(moveDirection, dt); // 向き
	UpdateAnimation(moveDirection);  // 歩きと待機のアニメーション
	SyncInvincible();                // IsInvincible() を HealthのInvincible と同期する
}


//=============================================================================
// 状態ごとの処理
//=============================================================================
// ===== 歩く =====
void PlayerController::UpdateMove(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime) {
	// --- 横の速さ ---
	DampHorizontalVelocity(controller, moveDirection * moveSpeed_, deltaTime);

	// --- 回避 ---
	const bool isPressed = InputManager::IsKeyTriggered(DIK_SPACE) || InputManager::IsButtonTriggered(GamepadButton::A);
	if (isPressed && CanDodge()) {
		StartDodge(controller, moveDirection, deltaTime);
	}
}

// ==== 回避（Start）=====
void PlayerController::StartDodge(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime) { 
	const float gravity = GetCalcGravity(controller);
	// --- 早期リターン ---
	if (gravity <= 0.0f || dodgeHeight_ <= 0.0f) {
		return; // 重力なし・高さ0
	}

	// --- 共通処理 ---
	const float upSpeed = std::sqrtf(2.0f * gravity * dodgeHeight_); // 上に上がる速さ（v^2 = 2gh）
	dodgeAirTime_ = 2.0f * upSpeed / gravity;
	state_ = PlayerState::Dodge; // 回避状態に
	dodgeElapsedTime_ = 0.0f;    // 経過時間リセット
	dodgeDirection_ = (LengthSq(moveDirection) > 0.0f) ? Normalize(moveDirection) : GetFacingDirection(); // 入力の向きを反映
	controller.velocity = dodgeDirection_ * GetCalcDodgeHorizontalSpeed(0.0f, deltaTime) + Vector3{0.0f, upSpeed, 0.0f}; // 上の速さは最初のみ

	// --- アニメーション ---
	const float animSpeed = AnimationSystem::CalcSpeedToFit(gameObject.handle, kClipDodgeJump, dodgeAirTime_); // 着地するまでの時間に合わせる
	AnimationSystem::Play(gameObject.handle, kClipDodgeJump, false, animSpeed);
	AnimationSystem::Restart(gameObject.handle);

	// --- ジャスト回避 ---
	JustCandidate just;
	if (HitSystem::TryConsumeJust(gameObject.handle, just)) {
		if (JustCounter* counter = GetComponent<JustCounter>()) {
			counter->Begin(just);
		} else {
			LogManager::Warning("ジャスト回避したが、JustCounter が付いていない（プレイヤーの Entity に足してください）");
		}
	}
}

// ===== 回避（Update）=====
void PlayerController::UpdateDodge(CharacterControllerComponent& controller, float deltaTime) {
	// --- 着地 ---
	if (controller.IsGrounded()) {
		state_ = PlayerState::Move;
		dodgeCooldownTimer_ = dodgeCooldown_; // クールダウン発生
		return;
	}

	// --- 横の速さ --
	if (dodgeElapsedTime_ < dodgeAirTime_) {
		const Vector3 horizontal = dodgeDirection_ * GetCalcDodgeHorizontalSpeed(dodgeElapsedTime_, deltaTime);
		controller.velocity.x = horizontal.x;
		controller.velocity.z = horizontal.z;
	}
	dodgeElapsedTime_ += deltaTime;
}

// ===== のけぞり（Start）=====
void PlayerController::StartHit(const HitInfo& hit) { 
	state_ = PlayerState::Hit; // 状態変更
	hitElapsedTime_ = 0.0f;    // 経過時間リセット

	// --- 押し戻しの向き ---
	const Vector3 direction = {hit.direction.x, 0.0f, hit.direction.z};
	knockbackDirection_ = (LengthSq(direction) > 0.0001f) ? Normalize(direction) : GetFacingDirection() * -1.0f; // 真上・真下からの攻撃は向きが決まらないので、後ろへ

	// --- アニメーション ---
	const float animSpeed = AnimationSystem::CalcSpeedToFit(gameObject.handle, kClipHit, hitStunTime_); // のけぞりの時間に合わせる
	AnimationSystem::Play(gameObject.handle, kClipHit, false, animSpeed);
	AnimationSystem::Restart(gameObject.handle);
}

// ===== のけぞり（Update）=====
void PlayerController::UpdateHit(CharacterControllerComponent& controller, float deltaTime) {
	// --- のけぞりの横の速さ ---
	const Vector3 horizontal = knockbackDirection_ * GetCalcKnockbackSpeed(hitElapsedTime_, deltaTime);
	controller.velocity.x = horizontal.x;
	controller.velocity.z = horizontal.z;
	hitElapsedTime_ += deltaTime;
	if (hitElapsedTime_ >= hitStunTime_) {
		state_ = PlayerState::Move;
	}
}

// ===== 倒れる（Start）=====
void PlayerController::StartDown() {
	state_ = PlayerState::Down;
	AnimationSystem::Play(gameObject.handle, kClipDown, false, 1.0f);
	AnimationSystem::Restart(gameObject.handle);
}


//=============================================================================
// 攻撃を受けたとき
//=============================================================================
void PlayerController::OnHitTaken(const HitInfo& hit) {
	if (state_ == PlayerState::Down) {
		return; // 既に負け
	}
	LogManager::Log(std::format("被弾：{}（{} 減って、残り {}）", GameObject{hit.hitbox}.GetName(), hit.damage, hit.healthAfter));
	if (hit.isDefeated) {
		StartDown(); // HP0
	} else {
		StartHit(hit);
	}
}


//=============================================================================
// 毎フレーム（状態によらない）
//=============================================================================
// ===== 入力をカメラから見た向きに直す =====
Vector3 PlayerController::CalcMoveDirection() const { 
	const Vector2 input = InputManager::GetMoveAxis();
	// --- 早期リターン ---
	if (Length(input) < kMoveInputDeadzone) {
		return {0.0f, 0.0f, 0.0f}; // 入力なし
	}

	// --- カメラの前を地面に平らにした向き ---
	Vector3 forward = {0.0f, 0.0f, 1.0f};
	if (const Camera* camera = CameraSystem::GetMainCamera()) {
		const Matrix4x4& view = camera->GetViewMatrix();
		const Vector3 flatForward = {view.m[0][2], 0.0f, view.m[2][2]};
		// 真下を向いてるとき
		if (LengthSq(flatForward) > 0.0001f) {
			forward = Normalize(flatForward);
		}
	}
	const Vector3 right = {forward.z, 0.0f, -forward.x};
	return right * input.x + forward * input.y;
}

// ===== 向き =====
void PlayerController::UpdateFacing(const Vector3& moveDirection, float deltaTime) { 
	Vector3 target = {0.0f, 0.0f, 0.0f};
	if (state_ == PlayerState::Move) {
		target = moveDirection;
	} else if (state_ == PlayerState::Dodge) {
		target = dodgeDirection_;
	}
	// --- 今の角度から一番近い回り方で ---
	if (Length(target) > 0.0f) {
		const float targetYaw = MathUtility::DirectionToYaw(target);
		const float rate = IsDodging() ? 1.0f : MathUtility::CalcSmoothRate(deltaTime, turnSmoothTime_);
		facingYaw_ = std::remainder(facingYaw_ + MathUtility::DeltaAngle(facingYaw_, targetYaw) * rate, 360.0f);
	}
	SetEulerAngles(transform, {0.0f, facingYaw_, 0.0f});
}

// ===== 歩きと待機のアニメーション =====
void PlayerController::UpdateAnimation(const Vector3& moveDirection) { 
	// --- 早期リターン ---
	if (state_ != PlayerState::Move) {
		return; // 歩き状態のみ
	}
	
	// --- 倒し具合で再生速度を調整 ---
	const float moveAmount = Length(moveDirection); // スティックの倒し具合
	if (moveAmount > 0.0f) {
		AnimationSystem::Play(gameObject.handle, kClipRun, true, std::lerp(kRunAnimMinSpeed, 1.0f, moveAmount));
	} else {
		AnimationSystem::Play(gameObject.handle, kClipIdle, true, 1.0f);
	}
}

// ===== 無敵をHealthに写す =====
void PlayerController::SyncInvincible() {
	if (HealthComponent* health = GetComponent<HealthComponent>()) {
		health->invincible = IsInvincible();
	}
}


//=============================================================================
// 道具
//=============================================================================
// ===== 重力の強さ =====
float PlayerController::GetCalcGravity(const CharacterControllerComponent& controller) {
	if (!controller.useGravity) {
		return 0.0f;
	}
	return Length(PhysicsSystem::GetGravity()) * controller.gravityScale;
}

// ===== 回避で、 elapsed 秒から deltaTime 秒の間に横へ進む速さ =====
float PlayerController::GetCalcDodgeHorizontalSpeed(float elapsed, float deltaTime) const {
	// --- 早期リターン ---
	if (dodgeAirTime_ <= 0.0f || deltaTime <= 0.0f) {
		return 0.0f;
	}
	const Curve& curve = GetDodgeMoveCurve();
	const float from = curve.Evaluate(elapsed / dodgeAirTime_);
	const float to = curve.Evaluate((elapsed + deltaTime) / dodgeAirTime_);
	return dodgeDistance_ * (to - from) / deltaTime;
}

// ===== 回避の横の進み方のカーブ =====
const Curve& PlayerController::GetDodgeMoveCurve() const {
	if (const Curve* curve = CurveManager::Get(dodgeMoveCurve_)) {
		return *curve;
	}
	// 未選択
	static const Curve kDefaultCurve = Curve::FromEase(Ease::Type::InOutSine);
	return kDefaultCurve;
}

// ===== のけぞりで、elapsed 秒から deltaTime 秒の間に押される速さ =====
float PlayerController::GetCalcKnockbackSpeed(float elapsed, float deltaTime) const {
	if (hitStunTime_ <= 0.0f || deltaTime <= 0.0f) {
		return 0.0f; // スタン終了
	}
	const auto progress = [this](float time) { return Ease::Apply((std::min)(time / hitStunTime_, 1.0f), Ease::Type::OutQuad); };
	return hitKnockbackDistance_ * (progress(elapsed + deltaTime) - progress(elapsed)) / deltaTime;
}

// ===== 横の速さをtargetへ滑らかに近づける =====
void PlayerController::DampHorizontalVelocity(CharacterControllerComponent& controller, const Vector3& target, float deltaTime) const {
	const Vector3 current = {controller.velocity.x, 0.0f, controller.velocity.z};
	const Vector3 next = Lerp(current, {target.x, 0.0f, target.z}, MathUtility::CalcSmoothRate(deltaTime, moveSmoothTime_));
	controller.velocity.x = next.x;
	controller.velocity.z = next.z;
}


//=============================================================================
// ほかの Script から読む
//=============================================================================
// ===== 攻撃が当たらない =====
bool PlayerController::IsInvincible() const { return state_ == PlayerState::Dodge && dodgeElapsedTime_ < dodgeInvincibleTime_; }

// ===== 向いている向き =====
Vector3 PlayerController::GetFacingDirection() const { return MathUtility::YawToDirection(facingYaw_); }

// ===== 今、回避ジャンプを始められる =====
bool PlayerController::CanDodge() const {
	const CharacterControllerComponent* controller = GetComponent<CharacterControllerComponent>();
	return state_ == PlayerState::Move && controller != nullptr && controller->IsGrounded() && dodgeCooldownTimer_ <= 0.0f;
}

// ===== 回避の待ち時間の残りの割合 =====
float PlayerController::GetDodgeCooldownRatio() const { return (dodgeCooldown_ > 0.0f) ? dodgeCooldownTimer_ / dodgeCooldown_ : 0.0f; }


//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void PlayerController::Fields(ComponentUI& ui) {
	// --- State: Move ---
	ui.Field("Move Speed", moveSpeed_, AtLeast(0.0f).Tip("移動速度"));
	ui.Field("Move Smooth Time", moveSmoothTime_, AtLeast(0.0f).Tip("速さが入力に追いつくまでの時間"));
	ui.Field("Turn Smooth Time", turnSmoothTime_, AtLeast(0.0f).Tip("向きが入力に追いつくまでの時間"));
	// --- State: Dodge ---
	ui.Field("Dodge Distance", dodgeDistance_, AtLeast(0.0f).Tip("回避で横に進む距離"));
	ui.Field("Dodge Height", dodgeHeight_, AtLeast(0.0f).Tip("回避で上に上がる高さ"));
	ui.Field("Dodge Cooldown", dodgeCooldown_, AtLeast(0.0f).Tip("回避のクールタイム"));
	ui.AssetField("Dodge Move Curve", dodgeMoveCurve_, AssetType::Curve, Tip("回避の進み方の.curveの番号"));
	ui.Field("Dodge Invincible Time", dodgeInvincibleTime_, AtLeast(0.0f).Tip("回避の無敵時間"));
	// --- State: Hit ---
	ui.Field("Hit Stun Time", hitStunTime_, AtLeast(0.0f).Tip("スタンの時間"));
	ui.Field("Hit Knockback Distance", hitKnockbackDistance_, AtLeast(0.0f).Tip("ノックバックの距離"));
}