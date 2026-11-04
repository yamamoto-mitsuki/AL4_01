#pragma once
#include <MyEngine/Components/Physics/CharacterControllerComponent.h>
#include <MyEngine/ECS/Script.h>

// プレイヤーの操作（歩く・振り向く・回避ジャンプ・アニメの切り替え）。プレイヤーの Entity に付ける
// 同じ Entity に付ける物：Collider（Sphere か Capsule）・Character Controller・Model Renderer・Animator（Player.gltf）
// 動かすのは CharacterControllerSystem。この Script は「どの向きへどれだけの速さで動きたいか」を velocity に書くだけ
// （坂で止まる・段を上る・壁に沿う・地面に吸い付く・重力は Character Controller の仕事）

// 前方宣言
class Curve;

/// <summary>
/// プレイヤーの状態（被弾・反撃・必殺などは、ここに足していく）
/// </summary>
enum class PlayerState {
	Move,  // 地面を歩く（落ちている間も、入力で横に動ける）
	Dodge, // 回避ジャンプ（入力の向きへ斜め上に跳んで1回転。着地まで向きは変えない。横の進み方は Dodge Move Curve）
};


/// <summary>
/// プレイヤーの操作
/// </summary>
class PlayerController : public Script {
public:
	void Start() override;
	void Update() override;
	void Fields(ComponentUI& ui) override;

	// ===== ほかの Script から読む（ボスの攻撃・ジャスト回避の判定・カメラなど）=====
	PlayerState GetState() const { return state_; }                    // 今の状態
	bool IsDodging() const { return state_ == PlayerState::Dodge; }    // 回避ジャンプ中
	bool IsInvincible() const;                                         // 攻撃が当たらない（回避ジャンプの始めの Dodge Invincible Time 秒）
	Vector3 GetFacingDirection() const;                                // 向いている向き（地面に平ら・長さ1）

private:
	// ===== 状態ごとの処理 =====
	void UpdateMove(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime);  // 歩く・回避を始める
	void UpdateDodge(CharacterControllerComponent& controller, float deltaTime);                              // 回避ジャンプ中（横の速さ）・着地
	void StartDodge(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime); // 回避ジャンプを始める

	// ===== 毎フレーム（状態によらない）=====
	Vector3 CalcMoveDirection() const;                                  // 入力をカメラから見た向きに直す
	void UpdateFacing(const Vector3& moveDirection, float deltaTime);   // 向き
	void UpdateAnimation(const Vector3& moveDirection);                 // アニメの切り替え

	// ===== 道具 =====
	void SetClip(int clipIndex, bool loop, float speed);      // アニメを切り替える（同じアニメなら速さだけ変える）
	float CalcSpeedToFit(int clipIndex, float seconds) const; // アニメをちょうど seconds 秒で流しきる再生の速さ
	static float CalcGravity(const CharacterControllerComponent& controller); // このキャラにかかる重力の強さ（m/秒²。Character Controller と同じ値）
	float CalcDodgeHorizontalSpeed(float elapsed, float deltaTime) const; // 回避ジャンプで、elapsed 秒から deltaTime 秒の間に横へ進む速さ（カーブから）
	const Curve& GetDodgeMoveCurve() const;                              // 回避ジャンプの横の進み方のカーブ（未選択なら InOutSine）
	static float CalcDampFactor(float sharpness, float deltaTime);       // なめらかに近づける割合（フレームの速さに左右されない）

	// ===== Inspector で変える値 =====
	float moveSpeed_ = 6.0f;              // 歩く速さ（m/秒）
	float acceleration_ = 20.0f;          // 速さが入力に追いつく速さ（大きいほどキビキビ、小さいほど滑る。着地の後の勢いの残り方も決まる）
	float turnSharpness_ = 18.0f;         // 向きが入力に追いつく速さ（大きいほど素早く振り向く）
	float dodgeDistance_ = 4.0f;          // 回避ジャンプで横に進む距離（m。平らな地面で、跳んでから着地するまで）
	float dodgeHeight_ = 1.2f;            // 回避ジャンプの一番高い所（m。足元から）。空中で反撃射撃を撃つ高さになる
	uint32_t dodgeMoveCurve_ = 0;         // 回避ジャンプの横の進み方（.curve の番号。横軸＝跳んでからの時間の割合 0〜1、縦軸＝進んだ距離の割合 0〜1。0 なら InOutSine）
	float dodgeInvincibleTime_ = 0.35f;   // 回避ジャンプを始めてから無敵の秒（跳んでいる間ずっとではない。着地を狙われると当たる）
	float dodgeCooldown_ = 0.15f;         // 着地してから、次の回避ジャンプができるまでの秒

	// ===== 実行中だけの値（保存しない）=====
	PlayerState state_ = PlayerState::Move;       // 今の状態
	float facingYaw_ = 0.0f;                      // 向いている角度（度。Y のまわり。0 で +Z、90 で +X）
	Vector3 dodgeDirection_ = {0.0f, 0.0f, 1.0f}; // 回避ジャンプの向き（始めたときに決めて、着地まで変えない）
	float dodgeElapsed_ = 0.0f;                   // 回避ジャンプを始めてからの秒
	float dodgeAirTime_ = 0.0f;                   // 平らな地面で着地するまでの秒（アニメの速さを合わせるのに使う）
	float dodgeCooldownTimer_ = 0.0f;             // 次の回避ジャンプができるまでの残りの秒
};
