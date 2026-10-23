#pragma once
#include <vector>

#include <MyEngine/ECS/Script.h>
#include <MyEngine/Math/Geometry/PathCurve.h>
#include <MyEngine/Math/Vector3.h>
#include <MyEngine/Math/Vector4.h>
#include <MyEngine/Systems/WindZoneSystem.h>

// 前方宣言
class LaunchOrb;

/// <summary>
/// 氷のキューブ（プレイヤー）。Rigidbody ＋ Collider（Sphere）を付けた Entity に付ける
/// <para>Rigidbody は Freeze Position Z を付け、Collide With Rigidbodies を外す。Use Gravity・Gravity Scale は、ここが毎フレーム書く</para>
/// <para>見た目のキューブは子の Entity「Body」（Primitive Renderer の箱）。当たりは球で、キューブは見た目だけ（傾く・伸び縮みする）</para>
/// <para>左右移動とジャンプ（1回）。コヨーテタイム・先行入力・長押しで高さが変わる。ぶつかる・押し戻すのはエンジン（PhysicsSystem）の仕事で、
/// ここは「入力を読んで、Rigidbody の速さと重力の強さを書く」だけ</para>
/// <para>ギミック：水色の円（JumpOrb）で空中でもう一度跳ぶ・緑の円（LaunchOrb）の中でジャンプすると、矢印の線に沿って吹っ飛ぶ・緑の壁（StickyWall）にくっつく</para>
/// <para>HP・やられ（下に落ちる・Kill・HP 0）・クリア（GoalDoor から）。やられたら少し待って、レベルの最初から（SceneManager::ReloadScene）</para>
/// <para>トゲ（Spikes）で HP が減る・ひびが入る。HP 0 で砕ける、熱湯（HotWater）で溶ける。風（Wind Zone）には自分で流される（Rigidbody の Wind Scale は Start で 0 にする）</para>
/// </summary>
class IceCube : public Script {
public:
	/// <summary>
	/// やられ方（見た目が変わる）
	/// </summary>
	enum class DeathType {
		Shatter, // 砕ける（かけらが飛び散る。HP が 0 になった）
		Melt,    // 溶ける（つぶれながら沈む。熱湯）
		Fall,    // 落ちた（下の線より下。画面の外なので、見た目は何もしない）
	};

	void Start() override;
	void Update() override;
	void Fields(ComponentUI& ui) override;

	// ===== 当たり（足元の向き・緑の壁・水色の円を集める。キューブを地面の向きに傾ける）=====
	void OnCollisionEnter(const CollisionInfo& collision) override;
	void OnCollisionStay(const CollisionInfo& collision) override;
	void OnTriggerEnter(GameObject other) override;
	void OnTriggerStay(GameObject other) override;

	// ===== ほかの Script から使う =====
	// HP を amount だけ減らして、knockback の速さではねる（0 ならはねない）。0 になったら砕ける
	// 減らすのは次の Update の頭で1回だけ（同じフレームに何回呼ばれても、一番大きい amount と knockback の1回分）
	void Damage(int amount, const Vector3& knockback = {0.0f, 0.0f, 0.0f});
	// 一発アウト（熱湯・下に落ちたとき・HP 0）。how でやられ方の見た目が変わる
	void Kill(DeathType how);
	// ゴールに入った（GoalDoor から）。doorPosition へ吸い込まれる（次のレベルへ進むのは GoalDoor）
	void Clear(const Vector3& doorPosition);
	int GetHp() const { return hp_; }
	int GetMaxHp() const { return maxHp_; }
	// 操作できるか（やられ・クリアの最中でない）
	bool IsPlayable() const;

private:
	/// <summary>
	/// 今していること
	/// </summary>
	enum class State {
		Move,   // 動ける（地上・空中）
		Stick,  // 緑の壁にくっついている（止まる。ジャンプで壁と反対へ跳ぶ・離れる向きの入力で落ちる）
		Launch, // 緑の円で吹っ飛んでいる（線の始まりに寄ってから、線に沿って飛ぶ。重力なし・左右の入力は効かない）
		Dead,   // やられ（砕ける・溶ける。少し待ってから、レベルの最初から）
		Clear,  // クリア（扉へ吸い込まれる）
	};

	// ===== 毎フレーム =====
	void UpdateMove(RigidbodyComponent& rigidbody, float deltaTime);          // State::Move
	void UpdateStick(RigidbodyComponent& rigidbody, float deltaTime);         // State::Stick
	void UpdateLaunch(RigidbodyComponent& rigidbody, float deltaTime);        // State::Launch
	void UpdateDead(float deltaTime);                                         // State::Dead
	void UpdateClear(float deltaTime);                                        // State::Clear
	void UpdateGround(const RigidbodyComponent& rigidbody, float deltaTime);  // 地面にいるか・着地
	void UpdateRun(RigidbodyComponent& rigidbody, float deltaTime);           // 左右移動
	void UpdateJump(RigidbodyComponent& rigidbody, float deltaTime);          // ジャンプ（先行入力・コヨーテタイム・長押し・水色の円）
	void UpdateGravity(RigidbodyComponent& rigidbody) const;                  // 重力の強さ（上がる・落ちる）と、落ちる速さの上限
	void UpdateBody(const RigidbodyComponent& rigidbody, float deltaTime);    // 見た目（子の Body の傾き・伸び縮み・大きさ）
	void UpdateBodyMaterial();                                                // 見た目（子の Body のひび・ダメージで白く光る）
	void ApplyDamage(RigidbodyComponent& rigidbody);                          // 頼まれたダメージを1回分だけ受けて、はねる（Update の頭）
	void UpdateWindDrift(float deltaTime);                                    // 風で流される横の速さ（走る目標に足す）
	void ApplyWindLift(RigidbodyComponent& rigidbody, float deltaTime) const; // 風で、縦の速さを押す（上向きの風で浮く）
	void SpawnShards();                                                       // 砕けたかけらを作る
	void UpdateShards(float deltaTime);                                       // かけらを飛ばす
	void Jump(RigidbodyComponent& rigidbody);                                 // 跳ぶ
	void TakeOff();                                                           // 地面を離れる（地面のジャンプと先行入力を消す）
	bool ReadJumpRequest(float deltaTime);                                    // ジャンプを頼まれているか（今押した・先行入力で覚えている）
	bool UseLaunchOrb();                                                      // 触れている緑の円を1つ使って、吹っ飛ぶ（使えたら true）
	void Launch(const LaunchOrb& orb);                                        // 吹っ飛ぶ。orb の線の始まりへ寄ってから、線に沿って飛ぶ
	PathCurve::Sample CalcLaunchPoint(float distance) const;                  // 吹っ飛ぶ線の、distance（m）進んだ所（線の終わりより先は、まっすぐ伸ばす）
	bool UseJumpOrb();                                                        // 触れている水色の円を1つ使う（使えたら true）
	bool UseAirJump();                                                        // 緑の壁・緑の円で戻った自分のジャンプを使う（使えたら true）
	bool TryStick(RigidbodyComponent& rigidbody);                             // 横から緑の壁に触れていたら、くっつく（くっついたら true）
	void ChangeState(State next);                                             // 今していることを変える（壁から離れるときは、少しの間その壁にくっつかない）
	void BeginEnding(State next);                                             // やられ・クリアを始める（物理で動かすのを止める）
	void CollectContact(const CollisionInfo& collision);                      // 当たりから、足元・緑の壁・吹っ飛びの行き止まりを集める
	void CollectGround(const CollisionInfo& collision);                       // 足元の向きを集める
	// 地面のジャンプが使えるか（地面にいる・足場から落ちてコヨーテタイムの間）
	bool CanGroundJump() const { return isOnGround_ || groundJumpTimer_ > 0.0f; }
	// 足元の向きが分かる地面に立っているか（坂に沿って走る・重力を切る。このフレームに跳んだら false。上向きの風で浮くときも false）
	bool IsStanding() const { return isOnGround_ && groundNormal_.y >= kStandCos && !IsWindLifting(); }
	// 上向きの風で浮くか（風が上へ吹いていて、重力より強い）。立っている間の「重力を切って押し付ける」をやめて、浮けるようにする
	bool IsWindLifting() const { return wind_.velocity.y > 0.0f && wind_.strength > gravity_; }

	// ===== 入力 =====
	static float ReadMoveInput();  // 左右（-1〜1。左が−）
	static bool IsJumpTriggered(); // ジャンプを押した瞬間
	static bool IsJumpPressed();   // ジャンプを押している

	// ===== Inspector に出す・保存する値 =====
	// --- 移動 ---
	float runSpeed_ = 7.0f;     // 左右の一番速い速さ（m/s）
	float groundAccel_ = 60.0f; // 地上で、入力の向きへ速さを変える強さ（1秒に何 m/s 変えるか）
	float groundDecel_ = 70.0f; // 地上で入力が無いとき、止まる強さ
	float airAccel_ = 35.0f;    // 空中で、入力の向きへ速さを変える強さ
	float airDecel_ = 10.0f;    // 空中で入力が無いとき、横の速さを落とす強さ（小さいと、跳んだ勢いが残る）
	// --- ジャンプ ---
	float jumpHeight_ = 3.0f;            // 長押ししたときのジャンプの高さ（m）
	float gravity_ = 30.0f;              // 重力の強さ（m/s²。上がっている間）
	float fallGravityMultiplier_ = 1.5f; // 落ちる間の重力の倍率
	float maxFallSpeed_ = 20.0f;         // 落ちる速さの上限（m/s）
	float jumpCutRatio_ = 0.5f;          // 上がっている途中でジャンプを離したとき、上向きの速さに掛ける
	float coyoteTime_ = 0.1f;            // 足場から落ちても、この秒数はまだジャンプできる
	float jumpBufferTime_ = 0.1f;        // 着地のこの秒数前に押したジャンプは、着地した瞬間に跳ぶ
	// --- ギミック ---
	float wallJumpSpeed_ = 6.0f;     // 緑の壁から跳んだときの、壁と反対への横の速さ（m/s）
	float wallRegrabTime_ = 0.2f;    // 緑の壁から離れた後、この秒数は同じ壁にくっつかない
	float launchGatherTime_ = 0.1f;  // 吹っ飛び始めてから、真ん中に寄る秒数
	float launchJumpLock_ = 0.3f;    // 吹っ飛び始めてから、この秒数はジャンプできない（別の緑の円は使える）
	float launchJumpKeep_ = 0.5f;    // 吹っ飛びをやめて跳んだとき、横の速さに掛ける
	float launchSpinSpeed_ = 720.0f; // 吹っ飛び中に、見た目のキューブが回る速さ（度/秒）
	// --- HP・やられ・クリア ---
	int maxHp_ = 3;             // HP
	float restartDelay_ = 1.0f; // やられてから、レベルの最初に戻るまでの秒数
	float clearTime_ = 0.6f;    // ゴールの扉へ吸い込まれる秒数
	// --- ダメージ・やられの見た目 ---
	uint32_t crackSmallTexture_ = 0; // 1 回ダメージを受けたときの Body のテクスチャ（小さいひび。0 なら元のまま）
	uint32_t crackLargeTexture_ = 0; // 2 回以上ダメージを受けたときの Body のテクスチャ（大きいひび。0 なら小さいひびのまま）
	float hurtFlashTime_ = 0.15f;    // ダメージを受けたとき、白く光る秒数
	float hurtShakeAngle_ = 10.0f;   // ダメージを受けたとき、揺れる角度（度）
	float hurtShakeTime_ = 0.3f;     // ダメージを受けたとき、揺れる秒数
	int shardCount_ = 8;             // 砕けたときのかけらの数
	float shardSize_ = 0.3f;         // かけらの大きさ（キューブの大きさに掛ける）
	float shardSpeed_ = 7.0f;        // かけらが飛び散る速さ（m/s。一番速いかけら）
	float meltTime_ = 0.6f;          // 溶けて、つぶれきるまでの秒数
	float meltSinkSpeed_ = 0.6f;     // 溶けながら沈む速さ（m/s）
	// --- 風 ---
	float windWobbleAngle_ = 8.0f; // 風の中で、ぐらぐら揺れる角度（度）
	// --- 見た目（子の Body）---
	float cubeSize_ = 1.0f;     // 見た目のキューブの大きさ（m）
	float tiltAngle_ = 12.0f;   // 空中で、進む向きに傾く角度（度。走る速さのとき）
	float tiltSpeed_ = 12.0f;   // 傾きが変わる速さ（大きいほどすぐ）
	float jumpStretch_ = 0.2f;  // 跳んだとき、縦に伸びる割合
	float landSquash_ = 0.25f;  // 着地したとき、縦につぶれる割合（落ちる速さの上限で着地したとき）
	float stretchTime_ = 0.08f; // 伸び縮みが戻る時間の目安（秒）

	// ===== 実行中だけの値（保存しない）=====
	State state_ = State::Move;                 // 今していること
	int hp_ = 0;                                // 今の HP（Start で Max HP にする）
	float killHeight_ = -10.0f;                 // これより下に落ちたら一発アウト（Start で LevelInfo から読む）
	bool isOnGround_ = false;                   // 地面にいる
	bool isJustLanded_ = false;                 // このフレームに着地した
	float groundJumpTimer_ = 0.0f;              // 足場から落ちた後、地面のジャンプがあと何秒使えるか（コヨーテタイムの残り）
	bool hasAirJump_ = false;                   // 緑の壁・緑の円で戻った自分のジャンプがある（使うか着地するまで残る）
	float jumpBufferTimer_ = 0.0f;              // 押したジャンプを、あと何秒覚えているか（先行入力の残り）
	float sinceJump_ = 1.0f;                    // 最後に跳んでからの秒数
	bool isJumpRising_ = false;                 // ジャンプで上がっている途中（長押しを見る）
	float previousVelocityX_ = 0.0f;            // 前のフレームの左右の速さ（坂に着地したときに使う）
	float previousVelocityY_ = 0.0f;            // 前のフレームの上下の速さ（着地の強さに使う）
	Vector3 groundNormal_ = {0.0f, 0.0f, 0.0f}; // このフレームの足元の向き（当たりで集める。乗っていなければ 0）
	float stateTimer_ = 0.0f;                   // 今していることになってからの秒数（吹っ飛び・やられ・クリアで使う）
	bool isRestartRequested_ = false;           // やり直しを頼んだ（何回も頼まないように）
	Vector3 clearStart_ = {0.0f, 0.0f, 0.0f};   // 吸い込まれ始めた位置
	Vector3 clearTarget_ = {0.0f, 0.0f, 0.0f};  // 扉の位置
	GameObject body_;                           // 子の「Body」（見た目のキューブ。Start で探す）
	float bodyAngle_ = 0.0f;                    // 今の傾き（度。Z 軸のまわりで、左回りが＋）
	float stretch_ = 0.0f;                      // 今の伸び縮み（＋で縦に伸びる、−でつぶれる）
	float bodyScale_ = 1.0f;                    // 全体の大きさの倍率（クリアで小さくなる）
	// --- ダメージ・やられ ---
	int pendingDamage_ = 0;                         // このフレームに頼まれたダメージ（次の Update の頭で受ける。一番大きい物）
	Vector3 pendingKnockback_ = {0.0f, 0.0f, 0.0f}; // そのとき、はねる速さ（一番速い物）
	float hurtTimer_ = 1.0f;                        // ダメージを受けてからの秒数（白く光る・揺れる）
	DeathType deathType_ = DeathType::Fall;         // やられ方
	float meltAmount_ = 0.0f;                       // 溶けた割合（0〜1。つぶれて横に広がる）
	Vector4 bodyColor_ = {1.0f, 1.0f, 1.0f, 1.0f};  // Body の元の色（Start で覚える。白く光らせた後に戻す）
	uint32_t bodyTexture_ = 0;                      // Body の元のテクスチャ（ひびが無いとき）
	/// <summary>
	/// 砕けたかけら1つ
	/// </summary>
	struct Shard {
		GameObject object;                     // かけらの Entity（やり直しでシーンを読み直すと消える）
		Vector3 velocity = {0.0f, 0.0f, 0.0f}; // 飛ぶ速さ（m/s）
		Vector3 spin = {0.0f, 0.0f, 0.0f};     // 回る速さ（度/秒。X・Y・Z）
	};
	std::vector<Shard> shards_; // 砕けたかけら
	// --- 風 ---
	WindZoneSystem::WindSample wind_; // このフレームの、自分の所の風（Update の頭で読む）
	float windDriftX_ = 0.0f;         // 風で流される横の速さ（m/s。走る目標の速さに足す）
	float windWobbleTime_ = 0.0f;     // 風の中で揺れる時間（秒）
	// --- 緑の壁 ---
	GameObject stickWall_;                     // くっついている壁
	Vector3 stickNormal_ = {0.0f, 0.0f, 0.0f}; // くっついている壁の向き（壁から外へ。当たりの normal）
	GameObject lastWall_;                      // 最後に離れた壁
	float wallRegrabTimer_ = 0.0f;             // 最後に離れた壁に、またくっつけるまでの残り秒数
	// --- 緑の円（吹っ飛び）---
	PathCurve launchPath_;                        // 飛ぶ線（ワールド座標。吹っ飛び始めに、緑の円から写す）
	Vector3 launchStart_ = {0.0f, 0.0f, 0.0f};    // 円に入った位置
	Vector3 launchCenter_ = {0.0f, 0.0f, 0.0f};   // 寄っていく、線の始まり（Z は自分の Z）
	Vector3 launchVelocity_ = {0.0f, 0.0f, 0.0f}; // 今の飛ぶ速さ（線の向き × 速さ。毎フレーム書き直す。行き止まり・やめて跳ぶ・回る向きに使う）
	float launchSpeed_ = 0.0f;                    // 飛ぶ速さ（m/s。線に沿って）
	float launchDistance_ = 0.0f;                 // 線に沿って進んだ長さ（m）
	float launchDuration_ = 0.0f;                 // 吹っ飛びの長さ（秒）
	bool isLaunchEndAtPathEnd_ = false;           // 線の終わりで吹っ飛びをやめる
	// --- このフレームの当たりで集める（groundNormal_ と同じく、Update の最後に空にする）---
	GameObject wallContact_;                  // 横から触れた緑の壁（無ければ空）
	Vector3 wallNormal_ = {0.0f, 0.0f, 0.0f}; // その壁の当たりの向き
	std::vector<GameObject> touchingOrbs_;    // 触れている輪（水色の円・緑の円。JumpRing）
	bool isLaunchBlocked_ = false;            // 吹っ飛ぶ向きの先にぶつかった

	static constexpr float kGroundIgnoreAfterJump = 0.1f; // 跳んだ直後のこの秒数は、足場に触れていても地面にいることにしない
	static constexpr float kStandCos = 0.7f;              // 当たりの向き（normal）の上の成分がこれ以上なら足元（約45度。エンジンの地面の判定と同じ）
	static constexpr float kGroundStickSpeed = 2.0f;      // 立っている間、足元へ押し付ける速さ（m/s。物理が押し戻して消すので、動きには出ない）
	static constexpr float kWallNormalX = 0.5f;           // 当たりの向きの横の成分の大きさがこれより大きければ、横から当たった（斜めの壁も入る）
	static constexpr float kWallStickSpeed = 2.0f;        // くっついている間、壁へ押し付ける速さ（m/s。足元と同じく、物理が消す）
	static constexpr float kLetGoInput = 0.5f;            // 壁から離れる向きの入力がこれより大きければ、壁を離す
	static constexpr float kLaunchBlockCos = 0.5f;        // 当たりの向きと飛ぶ向きの内積が −0.5 より小さければ、行き止まり（正面から約60度の内）
	static constexpr float kLaunchCatchUp = 2.0f;         // 線より遅れたとき（床をこすった・少し引っかかった）、追いつく速さの上限（飛ぶ速さの何倍か）
	static constexpr float kHurtShakeSpeed = 50.0f;       // ダメージで揺れる速さ（sin に掛ける。ラジアン/秒。1秒に約8回）
	static constexpr float kWindWobbleSpeed = 9.0f;       // 風の中で揺れる速さ（sin に掛ける。ラジアン/秒。1秒に約1.4回）
	static constexpr float kMeltSquash = 0.9f;            // 溶けきったとき、縦がどれだけつぶれるか（1 なら平らになって見えなくなる）
	static constexpr float kMeltSpread = 0.6f;            // 溶けきったとき、横がどれだけ広がるか
};