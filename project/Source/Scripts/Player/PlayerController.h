#pragma once
#include <MyEngine/Components/Physics/CharacterControllerComponent.h>
#include <MyEngine/ECS/Script.h>

//=============================================================================
// プレイヤーの操作（歩く・振り向く・回避・被弾・倒れる・アニメーションの切り替え）
//=============================================================================

// 前方宣言
class Curve;

// ===== プレイヤーの状態 =====
enum class PlayerState {
	Move,  // 移動
	Dodge, // 回避
	Hit,   // ダメージを受けた
	Down,  // 倒れた
};


/// <summary>
/// プレイヤーの操作
/// </summary>
class PlayerController : public Script {
public:
	void Start() override;
	void Update() override;
	void OnHitTaken(const HitInfo& hitInfo) override; // 攻撃を受けたとき
	void Fields(ComponentUI& ui) override;

	// ===== ゲッター =====
	PlayerState GetState() const { return state_; }                 // 現在の状態
	Vector3 GetFacingDirection() const;                             // 向いている向き
	float GetDodgeCooldownRatio() const;                            // 回避の残りクールタイムの割合
	bool IsDodging() const { return state_ == PlayerState::Dodge; } // 回避中か
	bool IsInvincible() const;                                      // 無敵中か
	bool CanDodge() const;                                          // 回避を始められるか

private:
	// ===== 状態ごとの処理 =====
	void StartHit(const HitInfo& hit);
	void StartDown();
	void UpdateMove(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime);
	void UpdateDodge(CharacterControllerComponent& controller, float deltaTime);
	void UpdateHit(CharacterControllerComponent& controller, float deltaTime);
	void StartDodge(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime);

	// ===== 毎フレーム =====
	void UpdateFacing(const Vector3& moveDirection, float deltaTime); // 向き
	void UpdateAnimation(const Vector3& moveDirection);               // 歩きと待機のアニメーション
	void SyncInvincible();                                            // IsInvincible() を HealthのInvincible と同期する
	Vector3 CalcMoveDirection() const;                                // 入力をカメラから見た向きに直す

	// ===== 道具 =====
	static float GetCalcGravity(const CharacterControllerComponent& controller); // 重力の強さ
	float GetCalcDodgeHorizontalSpeed(float elapsed, float deltaTime) const;     // 回避で、elapsed 秒から deltaTime 秒の間に横へ進む速さ
	const Curve& GetDodgeMoveCurve() const;                                      // 回避ジャンプの横の進み方のカーブ
	float GetCalcKnockbackSpeed(float elapsed, float deltaTime) const;           // のけぞりで、elapsed 秒から deltaTime 秒の間に押される速さ
	void DampHorizontalVelocity(CharacterControllerComponent& controller, const Vector3& target, float deltaTime) const; // 横の速さを target へなめらかに近づける

	// ===== Inspectorで変える値 =====
	// --- State: Move ---
	float moveSpeed_ = 3.0f;            // 移動速度
	float moveSmoothTime_ = 0.08f;      // 速さが入力に追いつくまでの時間
	float turnSmoothTime_ = 0.06f;      // 向きが入力に追いつくまでの時間
	// --- State: Dodge ---
	float dodgeDistance_ = 4.0f;        // 回避で横に進む距離
	float dodgeHeight_ = 1.2f;          // 回避で上に上がる高さ
	float dodgeCooldown_ = 1.0f;        // 回避のクールタイム
	uint32_t dodgeMoveCurve_ = 0;       // 回避の進み方の.curveの番号
	float dodgeInvincibleTime_ = 0.35f; // 回避の無敵時間
	// --- State: Hit ---
	float hitStunTime_ = 0.4f;          // スタンの時間
	float hitKnockbackDistance_ = 1.0f; // ノックバックの距離

	// ===== 実行中のみ使用 =====
	float facingYaw_ = 0.0f;                      // 向きの角度（度。0のとき ＋Z, 90で ＋X）
	Vector3 dodgeDirection_ = {0.0f, 0.0f, 0.0f}; // 回避の向き
	float dodgeElapsedTime_ = 0.0f;               // 回避を始めてからの時間
	float dodgeAirTime_ = 0.0f;                   // 平らな地面で着地するまでの時間
	float dodgeCooldownTimer_ = 0.0f;             // 次の回避ジャンプができるまでの残りの時間
	float hitElapsedTime_ = 0.0f;                 // のけぞりを始めてからの時間
	Vector3 knockbackDirection_ = {0.0f, 0.0f, -1.0f}; // のけぞりのときの押される向き
	PlayerState state_ = PlayerState::Move;       // 状態
};