#include "PlayerController.h"

#include <cmath>

#include <MyEngine/Curve/Curve.h>
#include <MyEngine/Curve/CurveManager.h>
#include <MyEngine/Components/Animation/AnimatorComponent.h>
#include <MyEngine/Graphics/Animation/AnimationManager.h>
#include <MyEngine/Input/InputManager.h>
#include <MyEngine/Systems/Physics/PhysicsSystem.h>

SCRIPT(PlayerController, "Player");

namespace {
// --- アニメの番号（Player.gltf の中の並び。Blender のアクション名の頭の番号と同じ。足すときは後ろに足す）---
constexpr int kClipIdle = 0;       // 待機（ループ）
constexpr int kClipRun = 1;        // 走り（ループ。杖を前に構えたまま）
constexpr int kClipDodgeJump = 8;  // 回避ジャンプ（1回だけ。跳ぶ → 前に1回転 → 着地）

// --- 入力 ---
constexpr float kMoveInputDeadzone = 0.1f; // これより小さい入力は「止まっている」扱い（スティックを離したときの小さな揺れ）
constexpr float kRunAnimMinSpeed = 0.5f;   // スティックを少しだけ倒したときの走りのアニメの速さ（倒しきると 1）
} // namespace


//=============================================================================
// エンジンが呼ぶ関数
//=============================================================================
// ===== 始め =====
void PlayerController::Start() {
	// 置いたときの向きから始める（エディタで回して置いた向きを引き継ぐ）
	facingYaw_ = GetEulerAngles(transform).y;
	SetClip(kClipIdle, true, 1.0f);
}

// ===== 毎フレーム =====
void PlayerController::Update() {
	const float deltaTime = Time::GetDeltaTime();
	if (deltaTime <= 0.0f) {
		return; // 時間が止まっている（ヒットストップ・時間の倍率 0）
	}
	CharacterControllerComponent* controller = GetComponent<CharacterControllerComponent>();
	if (controller == nullptr) {
		return; // Character Controller を付け忘れている（動かせない）
	}

	// --- 入力 → 状態ごとの処理 → 向き → アニメ の順（向きとアニメは、このフレームに回避を始めたかを見るため後ろ）---
	const Vector3 moveDirection = CalcMoveDirection();
	switch (state_) {
	case PlayerState::Move:
		UpdateMove(*controller, moveDirection, deltaTime);
		break;
	case PlayerState::Dodge:
		UpdateDodge(*controller, deltaTime);
		break;
	}
	UpdateFacing(moveDirection, deltaTime);
	UpdateAnimation(moveDirection);
}

// ===== Inspector に出す・保存する値 =====
void PlayerController::Fields(ComponentUI& ui) {
	ui.Field("Move Speed", moveSpeed_, AtLeast(0.0f).Tip("歩く速さ（m/秒）"));
	ui.Field("Acceleration", acceleration_, AtLeast(0.0f).Tip("速さが入力に追いつく速さ。大きいほどキビキビ、小さいほど滑る（着地の後の勢いの残り方も）"));
	ui.Field("Turn Sharpness", turnSharpness_, AtLeast(0.0f).Tip("向きが入力に追いつく速さ。大きいほど素早く振り向く"));
	ui.Field("Dodge Distance", dodgeDistance_, AtLeast(0.0f).Tip("回避ジャンプで横に進む距離（m。平らな地面で、跳んでから着地するまで）"));
	ui.Field("Dodge Height", dodgeHeight_, AtLeast(0.0f).Tip("回避ジャンプの一番高い所（m。足元から）。跳んでいる時間は、これと Character Controller の重力で決まる"));
	ui.AssetField("Dodge Move Curve", dodgeMoveCurve_, AssetType::Curve,
	    Tip("回避ジャンプの横の進み方（.curve）。横軸＝跳んでからの時間の割合 0〜1、縦軸＝進んだ距離の割合 0〜1。未選択なら InOutSine（ゆっくり出て、ゆっくり止まる）"));
	ui.Field("Dodge Invincible Time", dodgeInvincibleTime_, AtLeast(0.0f).Tip("回避ジャンプを始めてから無敵の秒"));
	ui.Field("Dodge Cooldown", dodgeCooldown_, AtLeast(0.0f).Tip("着地してから、次の回避ジャンプができるまでの秒"));
}


//=============================================================================
// ほかの Script から読む
//=============================================================================
// ===== 攻撃が当たらない =====
bool PlayerController::IsInvincible() const {
	return state_ == PlayerState::Dodge && dodgeElapsed_ < dodgeInvincibleTime_;
}

// ===== 向いている向き =====
Vector3 PlayerController::GetFacingDirection() const {
	const float radian = facingYaw_ * kDegToRad;
	return {std::sin(radian), 0.0f, std::cos(radian)};
}


//=============================================================================
// 状態ごとの処理
//=============================================================================
// ===== 歩く =====
// velocity の X・Z を毎フレーム書く（Y は Character Controller が重力で決める。地面にいる間は 0）
void PlayerController::UpdateMove(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime) {
	// --- 横の速さ：目標の速さへなめらかに近づける（着地の後は、回避の速さから歩きの速さへ少しずつ落ちる＝勢いが残る）---
	const Vector3 current = {controller.velocity.x, 0.0f, controller.velocity.z};
	const Vector3 next = Lerp(current, moveDirection * moveSpeed_, CalcDampFactor(acceleration_, deltaTime));
	controller.velocity.x = next.x;
	controller.velocity.z = next.z;

	// --- 回避ジャンプ：キーボードは Space、パッドは A。地面にいて、待ち時間が終わっているときだけ（空中では出ない＝1回だけ）---
	dodgeCooldownTimer_ = (std::max)(dodgeCooldownTimer_ - deltaTime, 0.0f);
	const bool isPressed = InputManager::IsKeyTriggered(DIK_SPACE) || InputManager::IsButtonTriggered(GamepadButton::A);
	if (isPressed && controller.IsGrounded() && dodgeCooldownTimer_ <= 0.0f) {
		StartDodge(controller, moveDirection, deltaTime);
	}
}

// ===== 回避ジャンプを始める =====
// 「どれだけ横に進むか（Dodge Distance）」と「どこまで高く上がるか（Dodge Height）」から、跳ぶ速さを逆算する
//   上向きの速さ vy ＝ √(2 × 重力 × 高さ)       … 投げ上げで、その高さで速さが 0 になる速さ
//   着地までの時間 T ＝ 2 × vy ÷ 重力            … 上がる時間と下りる時間は同じ
//   横は T の間に Dodge Distance だけ進む。どう進むか（出だし・終わりの速さ）は Dodge Move Curve（CalcDodgeHorizontalSpeed）
// 重力は Character Controller と同じ値を使うので、Gravity Scale を変えても距離と高さはそのまま（速く跳んで、速く落ちるだけ）
void PlayerController::StartDodge(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime) {
	const float gravity = CalcGravity(controller);
	if (gravity <= 0.0f || dodgeHeight_ <= 0.0f) {
		return; // 重力が無い（落ちてこない）・高さ 0 では跳べない
	}
	const float upSpeed = std::sqrt(2.0f * gravity * dodgeHeight_);
	dodgeAirTime_ = 2.0f * upSpeed / gravity;

	// 入力があればその向き、無ければ今向いている向きへ（止まったままでも避けられる）
	dodgeDirection_ = (LengthSq(moveDirection) > 0.0f) ? Normalize(moveDirection) : GetFacingDirection();
	state_ = PlayerState::Dodge;
	dodgeElapsed_ = 0.0f;

	// 上の速さは1回だけ書く（あとは Character Controller が重力で落とす）。横はこのフレームの分。次のフレームからは UpdateDodge が書く
	controller.velocity = dodgeDirection_ * CalcDodgeHorizontalSpeed(0.0f, deltaTime) + Vector3{0.0f, upSpeed, 0.0f};
}

// ===== 回避ジャンプ中・着地 =====
void PlayerController::UpdateDodge(CharacterControllerComponent& controller, float deltaTime) {
	// --- 着地：地面に戻ったら歩きへ。始めたフレームは Character Controller がまだ動かしていないので、ここに来るのは次のフレームから ---
	// （跳んだ次のフレームには空中になっている。天井ですぐ止められたときも、落ちて地面に着けばここで終わる）
	if (controller.IsGrounded()) {
		state_ = PlayerState::Move;
		dodgeCooldownTimer_ = dodgeCooldown_;
		return;
	}
	// --- 横の速さ：カーブの進み方になるように、このフレームに進む分の速さを毎フレーム書く（縦は Character Controller の重力のまま）---
	// 壁に当たっても毎フレーム同じ向きへ書くが、壁へ向かう分は Character Controller が消すので、壁に沿うか止まる
	// 平らな地面で着地する時間（T）を過ぎてもまだ落ちている（台地の端から跳んだ）ときは書かない＝最後の速さのまま落ちる
	if (dodgeElapsed_ < dodgeAirTime_) {
		const Vector3 horizontal = dodgeDirection_ * CalcDodgeHorizontalSpeed(dodgeElapsed_, deltaTime);
		controller.velocity.x = horizontal.x;
		controller.velocity.z = horizontal.z;
	}
	dodgeElapsed_ += deltaTime;
}


//=============================================================================
// 毎フレーム（状態によらない）
//=============================================================================
// ===== 入力をカメラから見た向きに直す =====
// 上（W・スティックを奥）を押したら画面の奥へ、右を押したら画面の右へ動くようにする。長さは 0〜1（スティックの倒し具合）
Vector3 PlayerController::CalcMoveDirection() const {
	const Vector2 input = InputManager::GetMoveAxis();
	if (Length(input) < kMoveInputDeadzone) {
		return {0.0f, 0.0f, 0.0f};
	}

	// --- カメラの前を地面に平らにした向き（カメラが無いときはワールドの +Z を奥にする）---
	Vector3 forward = {0.0f, 0.0f, 1.0f};
	if (const Camera* camera = CameraSystem::GetMainCamera()) {
		// ビュー行列はカメラのワールド行列の逆。回転の部分は「逆＝転置」なので、ビュー行列の3列目がカメラの前（ワールドの向き）になる
		const Matrix4x4& view = camera->GetViewMatrix();
		const Vector3 flatForward = {view.m[0][2], 0.0f, view.m[2][2]}; // 見下ろしのカメラは下を向いているので、y を捨てて地面に平らにする
		if (LengthSq(flatForward) > 0.0001f) {                          // 真下を向いているときだけは前が決まらないので、+Z のまま
			forward = Normalize(flatForward);
		}
	}
	// 右は、前を Y のまわりに 90 度回した向き（(x, z) → (z, -x)。左手系で上から見て時計回り）
	const Vector3 right = {forward.z, 0.0f, -forward.x};
	return right * input.x + forward * input.y;
}

// ===== 向き =====
// 歩いている向きへ振り向く。回避ジャンプ中は跳んだ向きにすぐ向く（1回転はアニメがする）。止まっているときは最後に向いた向きのまま
void PlayerController::UpdateFacing(const Vector3& moveDirection, float deltaTime) {
	const Vector3 target = IsDodging() ? dodgeDirection_ : moveDirection;
	if (LengthSq(target) > 0.0f) {
		// 向きを角度（度）にする。atan2(x, z) は +Z で 0、+X で 90（SetEulerAngles の Y と同じ測り方）
		const float targetYaw = std::atan2(target.x, target.z) * kRadToDeg;
		// 今の角度から一番近い回り方の差（-180〜180）。std::remainder は「一番近い整数倍を引いた余り」なので、350 度の差は -10 度になる（大回りしない）
		const float difference = std::remainder(targetYaw - facingYaw_, 360.0f);
		const float t = IsDodging() ? 1.0f : CalcDampFactor(turnSharpness_, deltaTime);
		facingYaw_ = std::remainder(facingYaw_ + difference * t, 360.0f); // 何周しても -180〜180 に収める
	}
	SetEulerAngles(transform, {0.0f, facingYaw_, 0.0f});
}

// ===== アニメの切り替え =====
void PlayerController::UpdateAnimation(const Vector3& moveDirection) {
	switch (state_) {
	case PlayerState::Move: {
		const float moveAmount = Length(moveDirection); // スティックの倒し具合（0〜1）
		if (moveAmount > 0.0f) {
			// ゆっくり歩くときは足もゆっくり。std::lerp（C++20 の <cmath>）は a + (b - a) × t。エンジンの float の Lerp は MathUtility の中にあるので、STL の方を使う
			SetClip(kClipRun, true, std::lerp(kRunAnimMinSpeed, 1.0f, moveAmount));
		} else {
			SetClip(kClipIdle, true, 1.0f);
		}
		break;
	}
	case PlayerState::Dodge:
		// 1回転のアニメを、平らな地面で着地するまでの時間でちょうど流しきる（距離・高さ・重力を変えてもアニメが合う）
		SetClip(kClipDodgeJump, false, CalcSpeedToFit(kClipDodgeJump, dodgeAirTime_));
		break;
	}
}


//=============================================================================
// 道具
//=============================================================================
// ===== アニメを切り替える =====
// Animator の clipIndex を変えると、AnimationSystem が次のフレームにそのアニメを最初から流す。同じ番号を入れ続けても最初には戻らない
void PlayerController::SetClip(int clipIndex, bool loop, float speed) {
	AnimatorComponent* animator = GetComponent<AnimatorComponent>();
	if (animator == nullptr) {
		return; // Animator を付けていない（アニメなしで動かして確かめるとき）
	}
	animator->speed = speed;
	if (animator->clipIndex != clipIndex) {
		animator->clipIndex = clipIndex;
		animator->loop = loop; // loop は切り替えたときに読まれるので、番号と一緒に変える
	}
}

// ===== アニメをちょうど seconds 秒で流しきる再生の速さ =====
// アニメの長さ ÷ 流したい秒。1.2 秒のアニメを 0.6 秒で流すなら 2 倍速（アニメが見つからなければ等速）
float PlayerController::CalcSpeedToFit(int clipIndex, float seconds) const {
	const AnimatorComponent* animator = GetComponent<AnimatorComponent>();
	if (animator == nullptr || seconds <= 0.0f) {
		return 1.0f;
	}
	const Animation* clip = AnimationManager::GetAnimation(animator->animationHandle, static_cast<uint32_t>(clipIndex));
	return (clip != nullptr && clip->duration > 0.0f) ? clip->duration / seconds : 1.0f;
}

// ===== このキャラにかかる重力の強さ =====
// Character Controller は「Settings の Physics → World → Gravity × Gravity Scale」で落とす。それと同じ値（向きは真下とみなして、長さだけ）
float PlayerController::CalcGravity(const CharacterControllerComponent& controller) {
	if (!controller.useGravity) {
		return 0.0f;
	}
	return Length(PhysicsSystem::GetGravity()) * controller.gravityScale;
}

// ===== 回避ジャンプで、elapsed 秒から deltaTime 秒の間に横へ進む速さ =====
// カーブは「時間の割合 → 進んだ距離の割合」。2つの時刻の割合の差 × 距離 が、この間に進む距離。それを時間で割ると速さになる
// 速さではなく「進んだ割合」をカーブにするのは、カーブの形をどう変えても、最後に進む距離がちょうど Dodge Distance になるため
// （速さのカーブだと、形を変えるたびに面積＝距離が変わってしまう）
float PlayerController::CalcDodgeHorizontalSpeed(float elapsed, float deltaTime) const {
	if (dodgeAirTime_ <= 0.0f || deltaTime <= 0.0f) {
		return 0.0f;
	}
	const Curve& curve = GetDodgeMoveCurve();
	const float from = curve.Evaluate(elapsed / dodgeAirTime_);
	const float to = curve.Evaluate((elapsed + deltaTime) / dodgeAirTime_); // 1 を過ぎたら最後の値（Evaluate がそう返す）
	return dodgeDistance_ * (to - from) / deltaTime;
}

// ===== 回避ジャンプの横の進み方のカーブ =====
const Curve& PlayerController::GetDodgeMoveCurve() const {
	if (const Curve* curve = CurveManager::Get(dodgeMoveCurve_)) {
		return *curve;
	}
	// 未選択：ゆっくり出て、ゆっくり止まる形（関数の中の static は、最初に通ったときに1回だけ作られる）
	static const Curve kDefaultCurve = Curve::FromEase(Ease::Type::InOutSine);
	return kDefaultCurve;
}

// ===== なめらかに近づける割合 =====
// 毎フレーム「残りの差 × t」だけ近づける。t を 1 - e^(-sharpness × 時間) にすると、60fps でも 144fps でも同じ速さで近づく
// （t を固定の 0.2 などにすると、フレームが多いほど速く近づいてしまう）
float PlayerController::CalcDampFactor(float sharpness, float deltaTime) {
	return 1.0f - std::exp(-sharpness * deltaTime);
}
