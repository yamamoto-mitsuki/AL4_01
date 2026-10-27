#pragma once
#include <string>

#include <MyEngine/ECS/Script.h>
#include <MyEngine/Math/Vector3.h>
#include <MyEngine/Math/Vector4.h>

/// <summary>
/// 隠し結晶（集める物）。Collider（Is Trigger）を付けた Entity に付ける（SecretCrystal.prefab）
/// <para>氷のキューブが触れたら取る。取った記録は LevelInfo が持ち、ゴールしたら進み具合（GameProgress）に保存する（取った後にやられたら、取っていない扱い。ただし中間地点に触れるまでに取った物は、中間地点から復活しても取った扱いのまま＝消えた姿で始まる）。
/// ステージの中では Id で見分けるので、同じステージに置く結晶は Id を別々にする</para>
/// <para>見た目（子）：Body（回る・浮く台。中に Gem＝結晶の箱 と Core＝芯の光）。いつも光って（ブルーム）、ゆっくり明るさが揺れ、
/// Glow Particle の粒がまとい続ける</para>
/// <para>取った：回りながら上へ昇り（だんだん速く回る・明るくなる・粒が増える）、昇りきったら Burst Particle・Spark Particle をはじけさせて、縮んで消える</para>
/// <para>前に取った（取ってクリアした）結晶：灰色の薄い姿（Ghost Color）で、灰色の薄い粒をまといながら置いておく（どこにあったか分かるように。触れても何も起きない）</para>
/// </summary>
class SecretCrystal : public Script {
public:
	void Start() override;
	void Update() override;
	void Fields(ComponentUI& ui) override;
	void OnTriggerEnter(GameObject other) override; // キューブが触れた → 取る
	void OnTriggerStay(GameObject other) override;  // 重なっている間も、取れるようになったら取る

	// ===== ほかの Script から使う =====
	// ステージの中で見分ける番号（LevelInfo が数を数える・同じ番号が無いか調べる）
	int GetId() const { return id_; }

private:
	/// <summary>
	/// 今どうなっているか
	/// </summary>
	enum class State {
		Idle,      // 取られるのを待っている（光って、回って、浮いている）
		Rising,    // 取った：回りながら昇っている
		Vanishing, // 昇りきった：はじけて、縮んでいる
		Gone,      // 消えた（何もしない）
		Ghost,     // 前に取った：灰色の薄い姿（灰色の薄い粒。触れても何も起きない）
	};

	void Collect();                                            // 取る（LevelInfo に覚えてもらって、昇り始める）
	void BecomeGhost();                                        // 前に取った結晶の姿にする（灰色の薄い姿・灰色の薄い粒）
	void Vanish();                                             // 昇りきった：はじけて、縮み始める
	void UpdateLook(float deltaTime);                          // 回る・浮く・昇る・明るさ
	void SetChildTranslation(const std::string& name, const Vector3& translation) const; // 直下の子を Body と同じ位置へ（粒を結晶に付いていかせる）
	void PlayChild(const std::string& name) const;             // 直下の子の Particle System を流し始める（無ければ何もしない）
	void StopChild(const std::string& name, bool clear) const; // 直下の子の Particle System を止める（clear：出ている粒も消す）

	// ===== Inspector に出す・保存する値 =====
	// --- 結晶 ---
	int id_ = 1; // ステージの中で見分ける番号（保存に使う。同じステージで別々に。置いた後に変えると、取った記録が別の結晶の物になる）
	// --- 光る ---
	float pulseAmount_ = 0.35f; // 明るさの揺れ（置いた明るさの何割を、増やしたり減らしたりするか）
	float pulseSpeed_ = 2.4f;   // 明るさの揺れる速さ（sin に掛ける。ラジアン/秒）
	// --- 待っている間 ---
	float idleSpinSpeed_ = 70.0f; // 回る速さ（度/秒。Y 軸のまわり）
	float bobHeight_ = 0.15f;     // 上下に浮く幅（m）
	float bobSpeed_ = 2.0f;       // 浮く速さ（sin に掛ける。ラジアン/秒）
	// --- 前に取った結晶 ---
	Vector4 ghostColor_ = {0.72f, 0.72f, 0.76f, 0.35f};         // Gem・Core の色（灰色。A で透ける）
	float ghostIntensity_ = 0.8f;                              // Gem・Core の明るさ（1 より小さく：ブルームで光らせない）
	Vector4 ghostParticleColor_ = {0.75f, 0.75f, 0.78f, 0.5f}; // Glow Particle の色（灰色。A で薄く）
	float ghostParticleIntensity_ = 1.2f;                      // Glow Particle の明るさ
	float ghostGlowRate_ = 0.6f;                               // Glow Particle の出る数に掛ける倍率
	// --- 取った：昇る ---
	float riseHeight_ = 1.8f;      // 昇る高さ（m）
	float riseTime_ = 1.0f;        // 昇りきるまでの秒数
	float riseSpinSpeed_ = 1440.0f; // 昇りきる時の回る速さ（度/秒。待っている間の速さから、だんだん速く）
	float riseScale_ = 1.3f;       // 昇りきる時の大きさの倍率
	float riseIntensity_ = 6.0f;   // 昇りきる時に足す明るさ（だんだん増える。ブルームで強く光る）
	float riseGlowRate_ = 4.0f;    // 昇っている間、Glow Particle の出る数に掛ける倍率
	// --- 取った：消える ---
	float vanishTime_ = 0.18f;  // はじけてから、縮んで消えるまでの秒数
	float rumblePower_ = 0.45f; // 取った瞬間・はじけた瞬間のコントローラーの振動の強さ（0〜1）
	float rumbleTime_ = 0.15f;  // 震える秒数

	// ===== 実行中だけの値（保存しない）=====
	State state_ = State::Idle;                    // 今どうなっているか
	int updateCount_ = 0;                          // Update が呼ばれた回数（粒は 2 回目から流す。1 回目はエンジンが捨てることがあるため）
	float timer_ = 0.0f;                           // 昇り始めてから・はじけてからの秒数
	float spinAngle_ = 0.0f;                       // 今の Body の回り（度。Y 軸のまわり）
	float spinSpeed_ = 0.0f;                       // 今の回る速さ（度/秒。昇り終わった後も、この速さのまま縮む）
	float bobTime_ = 0.0f;                         // 浮く・光が揺れる時間（秒）
	float riseOffset_ = 0.0f;                      // 今昇っている高さ（m）
	GameObject body_;                              // 子の Body（回る・浮く・昇る台）
	GameObject gem_;                               // Body の子の Gem（結晶）
	GameObject core_;                              // Body の子の Core（芯の光）
	Vector3 bodyPosition_ = {0.0f, 0.0f, 0.0f};    // Body の置いた位置
	Vector3 bodyScale_ = {1.0f, 1.0f, 1.0f};       // Body の置いた大きさ
	Vector4 gemColor_ = {1.0f, 1.0f, 1.0f, 1.0f};  // Gem の置いた色
	float gemIntensity_ = 1.0f;                    // Gem の置いた明るさ
	float coreIntensity_ = 1.0f;                   // Core の置いた明るさ
	float glowRate_ = 0.0f;                        // Glow Particle の置いた出る数（1秒あたり）
};
