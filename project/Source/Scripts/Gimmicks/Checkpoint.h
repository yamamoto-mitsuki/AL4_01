#pragma once
#include <string>

#include <MyEngine/ECS/Script.h>
#include <MyEngine/Math/Vector3.h>
#include <MyEngine/Math/Vector4.h>

/// <summary>
/// 中間地点。Collider（Is Trigger）を付けた、親の無い Entity に付ける（Checkpoint.prefab）
/// <para>氷のキューブが触れたら取る：ここを覚えて（gCheckpoint）、やられた・R でやり直すと、ここから始まる（時間は続きから）。
/// 触れるたびに、それまでに取った隠し結晶も覚えてもらう（LevelInfo::KeepCrystals。復活しても取り直さなくてよい）</para>
/// <para>見た目（子）：Crystal（回る・浮く台。中に Gem＝宝石の箱 と Core＝芯の光）。取る前は Idle Color の淡い氷色で少し光り、芯もうっすら光って、明るさがゆっくり揺れる（夜でも明るい背景でも、ここにあると分かるように）。
/// 取ると Lit Color の明るい色（ブルームで光る）に変わり、Aura Particle の粒がまとい続ける。
/// 取る前も、ここに中間地点があると分かるように、Aura Particle の粒を少なめ（Idle Aura Rate）に流しておく</para>
/// <para>取った瞬間：Burst Particle・Shock Particle がはじけ、宝石が大きく跳ねて白く光り、速く回る。ヒットストップと振動</para>
/// <para>復活した瞬間：読み直した後、幕が開くまで Charge Particle の粒が吸い込まれ、幕が開いたら同じようにはじけて、キューブが出てくる（IceCube::Appear）</para>
/// <para>スタート地点（Is Start）：はじめから取った見た目で、ステージが始まる・中間地点を取らずにやり直すと、ここからキューブが出てくる
/// （始まりの演出があれば、ぼかしが消えてから。やり直しなら幕が開いてから）。触れても何も起きない（覚えない）</para>
/// <para>親の無い Entity に置く（読み直した後、どの中間地点か Transform の位置で見分ける。最初のフレームはワールドの位置がまだ無いので）</para>
/// </summary>
class Checkpoint : public Script {
public:
	void Start() override;
	void Update() override;
	void Fields(ComponentUI& ui) override;
	void OnTriggerEnter(GameObject other) override; // キューブが触れた → 取る

	// ===== ほかの Script から使う =====
	// スタート地点の中間地点か（Is Start）
	bool IsStart() const { return isStart_; }
	// ここから出てくる位置（ワールド。親の無い Entity なので、Transform の位置がそのままワールドの位置。最初のフレームの Start からも使える）
	Vector3 GetSpawnPosition() const { return transform.translation + spawnOffset_; }

private:
	void Activate();                                  // 取る（覚えて、はじけて、光り始める）
	void PlayRespawn();                               // 幕が開いた：はじけて、キューブを出す
	void Burst();                                     // はじける（取った・復活で同じ。粒・宝石が跳ねて光る・振動）
	void UpdateLook(float deltaTime);                 // 宝石が回る・浮く・色が変わる
	void SetAuraRate(float scale) const;              // Aura Particle の1秒に出す数を、置いた数の scale 倍にする（取る前は少なめ）
	void PlayChild(const std::string& name) const;             // 直下の子の Particle System を流し始める（無ければ何もしない）
	void StopChild(const std::string& name, bool clear) const; // 直下の子の Particle System を止める（clear：出ている粒も消す）

	// ===== Inspector に出す・保存する値 =====
	// --- 中間地点 ---
	bool isStart_ = false;                       // スタート地点（はじめから取った見た目。ステージが始まると、始まりの演出が終わってから、ここからキューブが出てくる。覚えはしない）
	int order_ = 1;                              // 順番（ステージの中で、先の物ほど大きく。もっと先の中間地点を取った後に、手前の物を取っても戻らない）
	Vector3 spawnOffset_ = {0.0f, 0.6f, 0.0f};   // 復活する位置（この Entity の位置からのずれ。置く位置は地面の上の面）
	// --- 取った後の色 ---
	Vector4 litColor_ = {0.62f, 0.86f, 1.0f, 0.9f}; // 取った後の Gem の色（氷の水色）
	float litIntensity_ = 3.0f;                     // 取った後の Gem・Core の明るさ（1 より大きいとブルームで光る）
	float colorTime_ = 0.6f;                        // 取ってから、取った後の色に変わりきるまでの秒数
	// --- 取る前の色（夜でも・明るい背景でも、ここにあると分かるように、少し光らせて、明るさをゆっくり揺らす）---
	Vector4 idleColor_ = {0.72f, 0.86f, 1.0f, 0.9f}; // 取る前の Gem の色（淡い氷色。取った後の Lit Color より淡く）
	float idleIntensity_ = 2.2f;                     // 取る前の Gem の明るさ（1 より大きいと、うっすらブルームで光る）
	float idleCoreGlow_ = 0.45f;                     // 取る前の芯の光（取った後の何倍か。0 なら取る前は芯を光らせない）
	float idlePulse_ = 0.35f;                        // 取る前に明るさを揺らす幅（1 ± この割合）
	float idlePulseSpeed_ = 2.5f;                    // 揺らす速さ（sin に掛ける。ラジアン/秒）
	float idleAuraRate_ = 0.6f;                      // 取る前に流す Aura Particle の量（置いた Rate Over Time に掛ける。0 なら取る前は流さない）
	// --- はじける ---
	float flashIntensity_ = 12.0f; // はじけた瞬間に足す明るさ（だんだん 0 へ）
	float popScale_ = 1.8f;        // はじけた瞬間の Crystal の大きさの倍率（だんだん 1 へ）
	float popTime_ = 0.5f;         // 跳ねた大きさ・光・速い回りが落ち着くまでの秒数
	float hitStopTime_ = 0.07f;    // 取った瞬間に時間を止める秒数（0 なら止めない）
	float rumblePower_ = 1.0f;     // はじけたときのコントローラーの振動の強さ（0〜1）
	float rumbleTime_ = 0.35f;     // 震える秒数
	// --- 回る・浮く ---
	float idleSpinSpeed_ = 25.0f; // 取る前に回る速さ（度/秒）
	float litSpinSpeed_ = 80.0f;  // 取った後に回る速さ（度/秒）
	float spinBoost_ = 8.0f;      // はじけた瞬間、回る速さに掛ける倍率（Pop Time でだんだん 1 へ）
	float bobHeight_ = 0.12f;     // 取った後、上下に浮く幅（m）
	float bobSpeed_ = 2.0f;       // 浮く速さ（sin に掛ける。ラジアン/秒）

	// ===== 実行中だけの値（保存しない）=====
	bool isActivated_ = false;                      // 取った（このシーンで取った・この中間地点から復活した）
	bool isRespawnPoint_ = false;                   // この中間地点から復活するところ（幕が開くのを待っている）
	int updateCount_ = 0;                           // Update が呼ばれた回数（粒は 2 回目から流す。1 回目はエンジンが捨てることがあるため）
	float glow_ = 0.0f;                             // 取った後の色への変わり具合（0：取る前 〜 1：取った後）
	float popTimer_ = 1.0e6f;                       // はじけてからの秒数（大きい数＝はじけていない）
	float spinAngle_ = 0.0f;                        // 今の Crystal の回り（度。Y 軸のまわり）
	float bobTime_ = 0.0f;                          // 浮く時間（秒）
	GameObject crystal_;                            // 子の Crystal（回る・浮く台）
	GameObject gem_;                                // Crystal の子の Gem（宝石。色が変わる）
	GameObject core_;                               // Crystal の子の Core（芯の光。取る前は見えない）
	Vector3 crystalPosition_ = {0.0f, 0.0f, 0.0f};  // Crystal の置いた位置
	Vector3 crystalScale_ = {1.0f, 1.0f, 1.0f};     // Crystal の置いた大きさ
	Vector4 coreColor_ = {1.0f, 1.0f, 1.0f, 1.0f};  // Core の置いた色（取った後に、この色で光る）
	float auraRate_ = 0.0f;                         // Aura Particle の置いた Rate Over Time（取った後はこの数）
};
