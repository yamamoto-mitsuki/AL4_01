# 仕上げ2：やられたときの幕・足元の粒・風の粒（写経用）

前：[Polish1.md](Polish1.md)（仕上げ1）。設計：[OutGame.md](OutGame.md)

参考：
- エンジン：`Scene/SceneTransition.h`（`ReloadScene(fadeTime)`：幕を閉じてから読み直す）・`Systems/ParticleSystem.h`（`Emit`：今すぐ count 個出す）・`Components/ParticleSystemComponent.h`（`gravity`・`drag`・Shape の Box / Cone）・`Systems/WindZoneSystem.h`（`Sample`）・`ECS/EntityManager.h`（`Get(handle)`：Entity の親を読む）
- Unity の Particle System の「Rate over Distance」（進んだ長さで出す）
- エンジンの粒の回転：[ParticleRotation.md](../../MyEngine_v1/Docs/Tasks/ParticleRotation.md)（Start Rotation・Angular Velocity・Align To Velocity・Stretch。最初のフレームの Play の直しも）

## この段階でやること

ユーザーの希望（2026-10-26）：
1. **やられたときの演出が何も無い** → フェードしてやり直し（R キーのやり直しも同じ幕にそろえる）
2. **プレイヤーの足元から粒**（走ると足元から舞って、後ろに残る）
3. **風のゾーンに粒**（風の向き・速さで流れる）
4. 粒の png をそのまま使うと違和感がある → **回転が要る**（→ 0章。エンジンの ParticleRotation.md で足した。ここでは E-2 で値を使うだけ）

| パート | ファイル | 新／変 | 中身 |
|---|---|---|---|
| A | `Source/Scripts/IceCube.h / .cpp` | h は2か所・cpp は3か所 | やられたら、少し待って幕を閉じてやり直す（落ちたときは早く） |
| B | `Source/Scripts/LevelInfo.cpp` | 1行 | R のやり直しも幕を付ける |
| C | `Source/Scripts/Effects/DistanceTrail.h / .cpp` | **新しいファイル** | 進んだ長さで粒を出す（地面にいる間だけ） |
| D | `Source/Scripts/Effects/WindParticles.h / .cpp` | **新しいファイル** | Wind Zone の範囲に、風で流れる粒を出す |
| E | エディタ | 変 | Player に足元の粒・風の範囲に粒 |

**写す順番**：A → B → C → D（**C・D の4つは VS の「既存の項目を追加」で Source/Scripts/Effects に入れる**。入れ忘れるとリンクエラー）→ ビルド → E。
**E-2 の「筋」「葉」の値は、エンジンに [ParticleRotation.md](../../MyEngine_v1/Docs/Tasks/ParticleRotation.md) を写して、サブモジュールを進めた後**（それより前は Inspector に Rotation が無い。A〜D と E-1 は今のエンジンのままでよい）。

---

## 0. 粒の回転について（エンジンで足した）

粒の絵（`Resources/Textures/Effects/leaf.png` など）を回すのは、エンジンを変えるしかなかった（回転の値が無く、VS が world から位置と大きさしか取り出さない）。AL4_01 の CLAUDE.md は「Engine 層を変えない」なので、エンジンのチャット（外の `MyEngine_v1`）で [ParticleRotation.md](../../MyEngine_v1/Docs/Tasks/ParticleRotation.md) を作った。Particle System の Inspector に **Rotation** が増える：

| 項目 | 中身 |
|---|---|
| Start Rotation（Min, Max） | 初めの角度（度。**正で左回り**＝Transform の Rotation Z と同じ） |
| Angular Velocity（Min, Max） | 回る速さ（度/秒） |
| Random Spin Direction | 半分の粒を逆に（初めの角度も回る向きも） |
| Align To Velocity | 絵の右（+X）を、画面の上で進む向きに向ける |
| Stretch | 絵の横だけ伸ばす倍率（Align と合わせると、進む向きに細長い線） |

このページでの使い方：
- **風の粒（E-2）**：Align To Velocity ✓・Stretch 14 で、丸いぼかしを**風の向きの筋**にする（おすすめ）。または `leaf.png` を回しながら流す（**葉**）。どちらも作業用コピーで撮って値を決めた（下の「作業用コピーで確かめたこと」）。
- **足元の粒（E-1）**：Texture は空（丸いぼかし）のまま。今ある粒の絵は葉だけで、足元の氷の粉には合わない（丸い粒は回っても見た目が変わらないので、Rotation は使わない）。

**もう1つ見つけたエンジンの動き**（D の作りに関係）：ゲームが始まった最初のフレーム（製品版の起動・エディタの Play）に Script から `ParticleSystem::Play` を呼ぶと、その後の `ParticleSystem::Update` が「再生が始まった」として粒の状態を全部捨てるので、**その Play が消えていた**。これも ParticleRotation.md のパート A で直した（`AnimationSystem::Play` も同じ形だったので、パート D で直した）。このページの粒は `Emit` で毎フレーム出す作りのままにする（D の解説の最後）。

---

## パート A：`Source/Scripts/IceCube.h / .cpp`

### `IceCube.h`（2か所）

① 一番上の説明の1行（20行目あたり）
```cpp
// 前
/// <para>HP・やられ（下に落ちる・Kill・HP 0）・クリア（GoalDoor から）。やられたら少し待って、レベルの最初から（SceneManager::ReloadScene）</para>
// 後
/// <para>HP・やられ（下に落ちる・Kill・HP 0）・クリア（GoalDoor から）。やられたら少し待って、幕を閉じてレベルの最初から（SceneTransition::ReloadScene）</para>
```

② 「HP・やられ・クリア」の値（134行目あたり）
```cpp
// 前
	int maxHp_ = 3;             // HP
	float restartDelay_ = 1.0f; // やられてから、レベルの最初に戻るまでの秒数
	float clearTime_ = 0.6f;    // ゴールの扉へ吸い込まれる秒数
// 後
	int maxHp_ = 3;                 // HP
	float restartDelay_ = 1.0f;     // やられて（砕ける・溶ける）から、幕を閉じ始めるまでの秒数
	float fallRestartDelay_ = 0.2f; // 下に落ちてから、幕を閉じ始めるまでの秒数（画面の外で見る物が無いので短く）
	float restartFadeTime_ = 0.5f;  // やり直すときの幕を閉じる秒数（開くのも同じ秒数）
	float clearTime_ = 0.6f;        // ゴールの扉へ吸い込まれる秒数
```

### `IceCube.cpp`（3か所）

① `Fields` の Life（50行目あたり）
```cpp
// 前
	ui.Field("Restart Delay", restartDelay_, AtLeast(0.0f).Tip("やられてから、レベルの最初に戻るまでの秒数"));
// 後
	ui.Field("Restart Delay", restartDelay_, AtLeast(0.0f).Tip("砕けた・溶けてから、幕を閉じ始めるまでの秒数（幕が閉じきったら、レベルの最初から）"));
	ui.Field("Fall Restart Delay", fallRestartDelay_, AtLeast(0.0f).Tip("下に落ちてから、幕を閉じ始めるまでの秒数（画面の外なので短く）"));
	ui.Field("Restart Fade Time", restartFadeTime_, AtLeast(0.01f).Tip("やり直すときに、幕を閉じる秒数（開くのも同じ秒数）"));
```

② `UpdateDead`（見出しと、最後のやり直しの所）
```cpp
// 前
// ===== やられ（縮んで消えて、少し待ってから、レベルの最初から）=====
void IceCube::UpdateDead(float deltaTime) {
	（中は同じ）
	// --- 少し待ってから、レベルの最初から（1回だけ頼む）---
	if (stateTimer_ >= restartDelay_ && !isRestartRequested_) {
		isRestartRequested_ = true;
		SceneManager::ReloadScene();
	}
}
// 後
// ===== やられ（砕ける・溶ける。少し待ってから、幕を閉じてレベルの最初から）=====
void IceCube::UpdateDead(float deltaTime) {
	（中は同じ）
	// --- 少し待ってから、幕を閉じて、レベルの最初から（落ちたときは見る物が無いので、すぐ）---
	// 幕を閉じきったら SceneTransition がシーンを読み直して、幕を開ける（このスクリプトもシーンと一緒に作り直される）
	// 頼めなかったら（ポーズの「やり直す」などで、もう幕が動いている）、次のフレームにもう一度頼む
	const float delay = (deathType_ == DeathType::Fall) ? fallRestartDelay_ : restartDelay_;
	if (stateTimer_ >= delay && !isRestartRequested_) {
		isRestartRequested_ = SceneTransition::ReloadScene(restartFadeTime_);
	}
}
```

③ `UpdateShards` の頭（かけらが、幕が閉じきるまで飛び続ける）
```cpp
// 前
// ===== かけらを飛ばす（重力で落ちながら回って、やり直すまでに小さくなって消える）=====
void IceCube::UpdateShards(float deltaTime) {
	const float life = (restartDelay_ > 0.0f) ? std::clamp(1.0f - stateTimer_ / restartDelay_, 0.0f, 1.0f) : 0.0f; // 1 → 0
// 後
// ===== かけらを飛ばす（重力で落ちながら回って、幕が閉じきるまでに小さくなって消える）=====
void IceCube::UpdateShards(float deltaTime) {
	// 幕を閉じている間も飛び続けて、閉じきったときにちょうど消える（Restart Delay ＋ Restart Fade Time で 1 → 0）
	const float lifeTime = restartDelay_ + restartFadeTime_;
	const float life = (lifeTime > 0.0f) ? std::clamp(1.0f - stateTimer_ / lifeTime, 0.0f, 1.0f) : 0.0f; // 1 → 0
```
（その下の `const float size = …` からは同じ）

### 解説
- **幕は `SceneTransition` が持つ**（Entity ではない）。シーンを読み直しても幕は消えず、閉じきったら読み直して、また開く。ポーズの「やり直す」・クリア画面の Next はもうこれを使っているので、**やられも同じ見た目の幕にそろう**。
- `ReloadScene` は「幕がもう動いている」と false を返す。`isRestartRequested_` に結果を入れているので、**頼めなかったら次のフレームにもう一度頼む**（ポーズの「やり直す」と同時に起きても、どちらかの幕で必ず読み直される）。
- 時間：砕ける・溶ける → **1.0 秒見せて**、0.5 秒で暗くなって、0.5 秒で明るくなる。落ちる → 画面の外なので **0.2 秒**で暗くなり始める。
- シーンに保存してある Player には新しい2つの値が無いが、読み込むと初めの値（0.2・0.5）になる。

---

## パート B：`Source/Scripts/LevelInfo.cpp`（1行）

`Update` の「やり直し」
```cpp
// 前
		SceneManager::ReloadScene();
// 後
		SceneTransition::ReloadScene();
```
（幕が動いている間に R を押しても false が返るだけで、二重にはならない）

---

## パート C：`Source/Scripts/Effects/DistanceTrail.h / .cpp`（新しいファイル）

### `DistanceTrail.h`
```cpp
#pragma once
#include <MyEngine/ECS/Script.h>
#include <MyEngine/Math/Vector3.h>

// 前方宣言
struct RigidbodyComponent;

/// <summary>
/// 動いた長さに合わせて、粒を残していく（通った跡）。Particle System を付けた Entity に付ける（氷のキューブの足元の子「FootTrail」など）
/// <para>Particle System は Play On Start を外し、Rate Over Time・Burst Count を 0 にする（出すのは全部ここ。ParticleSystem::Emit で出す）</para>
/// <para>時間ではなく「進んだ長さ」で出すので、止まっていると出ない・速いほど跡が長くなる（Unity の Rate over Distance と同じ考え方）</para>
/// <para>Particle System の Simulation Space は World にする（Local だと、粒が一緒に付いてきて跡にならない）</para>
/// <para>Ground Only：自分か親の Rigidbody が地面に乗っている（Is Grounded）間だけ出す（走ると足元から粒が舞う。跳んでいる間は出ない）</para>
/// </summary>
class DistanceTrail : public Script {
public:
	void Start() override;
	void Update() override;
	void Fields(ComponentUI& ui) override;

private:
	// 地面を見る Rigidbody（自分に無ければ親の。どちらにも無ければ nullptr）
	const RigidbodyComponent* FindRigidbody() const;

	// ===== Inspector に出す・保存する値 =====
	float perMeter_ = 8.0f;  // 1m 進むごとに出す粒の数
	float minSpeed_ = 1.0f;  // これより遅いときは出さない（m/s。止まりかけ・溶けて沈むときに出さない）
	float maxStep_ = 3.0f;   // 1フレームにこれより長く動いたら、出さない（m。やり直し・場所を飛んだときに、線のように粒が並ばないように）
	bool groundOnly_ = true; // 地面に乗っている間だけ出す（自分か親の Rigidbody の Is Grounded を見る）

	// ===== 実行中だけの値（保存しない）=====
	Vector3 previousPosition_ = {0.0f, 0.0f, 0.0f}; // 前のフレームの位置（ワールド）
	float carry_ = 0.0f;                            // 出しきれなかった端数（0〜1 個分。次のフレームへ持ち越す）
};
```

### `DistanceTrail.cpp`
```cpp
#include "DistanceTrail.h"

#include <cmath>

#include <MyEngine/Systems/ParticleSystem.h>

SCRIPT(DistanceTrail, "Effects");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void DistanceTrail::Fields(ComponentUI& ui) {
	ui.Label("同じ Entity の Particle System を、進んだ長さに合わせて出す（Play On Start を外し、Rate Over Time・Burst Count を 0、Simulation Space を World に）");
	ui.Field("Per Meter", perMeter_, AtLeast(0.0f).Tip("1m 進むごとに出す粒の数（多いほど、跡が詰まって線のように見える）"));
	ui.Field("Min Speed", minSpeed_, AtLeast(0.0f).Tip("これより遅いときは出さない（m/s）。止まりかけ・溶けて沈むときに、粒がたまらないように"));
	ui.Field("Max Step", maxStep_, AtLeast(0.0f).Tip("1フレームにこれより長く動いたら出さない（m）。やり直し・場所を飛んだときに、飛んだ間に粒が並ばないように"));
	ui.Field("Ground Only", groundOnly_, Tip("地面に乗っている間だけ出す（自分か親の Rigidbody の Is Grounded を見る）。外すと、空中でも動けば出る"));
}

//=============================================================================
// 地面を見る Rigidbody
//=============================================================================
const RigidbodyComponent* DistanceTrail::FindRigidbody() const {
	// 自分に付いていれば、それ（Rigidbody と同じ Entity に付けたとき）
	if (const RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>()) {
		return rigidbody;
	}
	// 無ければ親の（キューブの子にして、足元へずらして置いたとき）
	// GameObject には親を返す関数が無いので、Entity（名前・親などの元の情報）を EntityManager から読む
	const Entity* self = EntityManager::Get(gameObject.handle);
	if (self == nullptr) {
		return nullptr;
	}
	return EntityManager::Get<RigidbodyComponent>(self->parent);
}

//=============================================================================
// 最初に1回 / 毎フレーム
//=============================================================================
// ===== 最初に1回 =====
void DistanceTrail::Start() {
	previousPosition_ = GetWorldPosition(transform);
}

// ===== 毎フレーム =====
void DistanceTrail::Update() {
	const float deltaTime = Time::GetDeltaTime();
	if (deltaTime <= 0.0f) {
		return; // ヒットストップ・ポーズ中（動いていないので出さない。位置も覚え直さない）
	}
	// --- このフレームに進んだ長さ ---
	const Vector3 position = GetWorldPosition(transform);
	const float step = Length(position - previousPosition_);
	previousPosition_ = position;
	// --- 早期リターン：場所を飛んだ（1フレームで Max Step より長い）・遅すぎる・地面にいない（Ground Only）---
	// 端数も捨てる（次に動き出したとき・着地したとき、前の端数で1つ余分に出ないように）
	// Is Grounded は前のフレームの物理の結果（1フレーム遅れるが、粒には分からない）
	const RigidbodyComponent* rigidbody = groundOnly_ ? FindRigidbody() : nullptr;
	const bool isAirborne = groundOnly_ && (rigidbody == nullptr || !rigidbody->isGrounded);
	if (step > maxStep_ || step / deltaTime < minSpeed_ || isAirborne) {
		carry_ = 0.0f;
		return;
	}
	// --- 進んだ長さ × Per Meter の数だけ出す（端数は次のフレームへ持ち越す。フレームの速さで出る数が変わらない）---
	// 例：Per Meter 8・1フレームに 0.1m 進んだ → 0.8 個 → このフレームは 0 個で、0.8 を持ち越す → 次のフレームで 1.6 → 1 個出して 0.6 を持ち越す
	carry_ += step * perMeter_;
	const float count = std::floor(carry_);
	carry_ -= count;
	if (count > 0.0f) {
		// Emit は Particle System の Shape の中から出す（Cone の Radius を足元の幅くらいにすると、ばらけて自然になる）
		ParticleSystem::Emit(gameObject.handle, static_cast<int>(count));
	}
}
```

### 解説
- **時間ではなく長さで出す**：Rate Over Time（1秒に何個）だと、止まっていても出続ける・速く走ると粒の間が空く。進んだ長さ × Per Meter なら、**粒の間かくがいつも同じ**で、止まると出ない。
- **端数の持ち越し**（`carry_`）：1フレームに 0.8 個のような半端が出るので、整数の分だけ出して残りは次へ。ParticleSystem の Rate Over Time の中と同じやり方（`std::floor`：小数点以下を切り捨てた数。`<cmath>`）。
- **親の Rigidbody を読む**：足元の粒は Player の子（足元へずらして置く）なので、自分には Rigidbody が無い。`GameObject` に親を返す関数が無いので、`EntityManager::Get(handle)` で `Entity`（name・parent などを持つ元の構造体）を読んで、`parent` の Rigidbody を取る。
- **Body の子にしない**：Body はキューブの見た目で、傾く・伸び縮みする・砕けると隠れる。Player の直下の子なら、向きは変わらず、いつも球の下（足元）にいる。
- やられた後は Player が止まる（溶けて沈むのは 0.6 m/s で Min Speed 1 より遅い）ので、粒は出ない。

---

## パート D：`Source/Scripts/Effects/WindParticles.h / .cpp`（新しいファイル）

### `WindParticles.h`
```cpp
#pragma once
#include <MyEngine/ECS/Script.h>
#include <MyEngine/Math/Vector2.h>

/// <summary>
/// 風の範囲（Wind Zone）の中に、風に流される粒を出す。Wind Zone と Particle System を両方付けた Entity に付ける
/// <para>Particle System は Play On Start を外す（出すのはここ。Rate Over Time の数だけ、毎秒 ParticleSystem::Emit で出す）</para>
/// <para>粒の「動き」はここが毎フレーム Particle System に書く：Shape を Box にして Wind Zone の Size に合わせる・風の速さで流れるように
/// Gravity と Drag を決める・範囲を流れ切るくらいの Lifetime にする。Inspector でそこを変えても、ここが上書きする</para>
/// <para>粒の「見た目」と量（Rate Over Time・Size・Color・End Color・Texture など）は、Particle System の Inspector で決める</para>
/// <para>風のゆれ（Gust）も粒の速さに入る（WindZoneSystem::Sample で、範囲の真ん中の風を読む）。Wind Zone の Enabled を外すと、出すのをやめる</para>
/// </summary>
class WindParticles : public Script {
public:
	void Update() override;
	void Fields(ComponentUI& ui) override;

private:
	// ===== Inspector に出す・保存する値 =====
	float followRate_ = 4.0f;         // 粒が風の速さに追いつく強さ（1/秒。大きいほど、出た瞬間から風の速さで流れる）
	Vector2 lifeRate_ = {0.4f, 0.8f}; // 粒の寿命（範囲を風の速さで流れ切る秒数に掛ける。Min〜Max）

	// ===== 実行中だけの値（保存しない）=====
	float carry_ = 0.0f; // 出しきれなかった端数（0〜1 個分。次のフレームへ持ち越す）
};
```

### `WindParticles.cpp`
```cpp
#include "WindParticles.h"

#include <algorithm>
#include <cmath>

#include <MyEngine/Components/ParticleSystemComponent.h>
#include <MyEngine/Components/WindZoneComponent.h>
#include <MyEngine/Systems/ParticleSystem.h>
#include <MyEngine/Systems/WindZoneSystem.h>

SCRIPT(WindParticles, "Effects");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void WindParticles::Fields(ComponentUI& ui) {
	ui.Label("同じ Entity の Particle System を、Wind Zone に合わせて流す（Play On Start を外す。数は Rate Over Time、見た目は Particle System の方で）");
	ui.Field("Follow Rate", followRate_, AtLeast(0.1f).Tip("粒が風の速さに追いつく強さ（1/秒）。大きいほど、出た瞬間から風の速さで流れる。小さいと、ふわっと動き出す"));
	ui.Field("Life Rate", lifeRate_, AtLeast(0.0f).Tip("粒の寿命（範囲の端から端まで、風の速さで流れる秒数に掛ける。Min〜Max）。1 なら範囲を流れ切るくらい"));
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
	carry_ += (std::max)(0.0f, particle->rateOverTime) * deltaTime;
	const float count = std::floor(carry_);
	carry_ -= count;
	if (count > 0.0f) {
		ParticleSystem::Emit(gameObject.handle, static_cast<int>(count));
	}
}
```

### 解説
- **Box の Speed は使えない**：Particle System の Box は「箱の真ん中から外へ」飛ばすので、風の向きとは関係ない向きに散る。そこで Speed を 0 にして、**Gravity（加速）を風の向きに向ける**。
- **Gravity ＝ 風の速さ × Drag**：Drag は「1秒に速さの Drag 倍だけ減らす」。加速と減速がつり合う速さは Gravity ÷ Drag なので、こう決めると**粒は風の速さで止まる**（いつまでも速くならない）。Follow Rate 4 なら、約 0.25 秒で風の速さに近づく。
- **Wind Zone も Particle System も、Entity の Scale・Rotation を同じように掛ける**ので、Box Size に Wind Zone の Size をそのまま入れれば範囲がそろう。Rotation の Z を 90 にした上向きの風でも、粒は上へ流れる。
- **Gust**：`WindZoneSystem::Sample` は「ゆれ込みの風」を返すので、粒も強くなったり弱くなったりする。
- `(std::max)`：Windows.h の max マクロとぶつからないように、かっこで囲む（CLAUDE.md）。
- **Emit で出す理由**：初めは「最初のフレームの Play が消える」エンジンの動きを避けるためだった（0章）。エンジンで直した後も、風の Enabled に合わせて出す・止めるのが一番短く書けるので、Emit のままにする。
- **Rotation の値（Start Rotation・Align To Velocity・Stretch など）は上書きしない**ので、E-2 の「筋」「葉」は Particle System の Inspector で決める。Align To Velocity の粒は出た瞬間は止まっているが、Emit したフレームのうちに Gravity で動き出すので、横向きのまま一瞬見える、ということは無い。

---

## パート E：エディタ

### E-1. Player に足元の粒（Player.prefab と、各 Level の Player）
**Prefab を直しても、シーンに置いてある Player は変わらない**（仕上げ1 の Goal と同じ）。Player.prefab を直してから、**Level01〜07 の Player にも同じ子を置いて Ctrl+S**（Ctrl+C / V はシーンをまたいで使える）。

Player の**直下**（Body の子にはしない）に Entity「**FootTrail**」を作る。

| 見出し | 値 |
|---|---|
| Transform | 位置 **(0, -0.45, 0)**（当たりの球の半径 0.5 の少し上＝足元）・Scale (1, 1, 1) |
| 再生 | **Play On Start を外す**・Looping は外す・Duration 1 |
| 出す量 | **Rate Over Time 0**・**Burst Count 0**・Max Particles 256 |
| Shape | **Cone**・Radius **0.2**・Cone Angle **35**（上へ、少し開いて舞い上がる） |
| 初めの値 | Lifetime **0.35〜0.6**・Speed **0.8〜1.6**・Size **0.06〜0.14**・Color A **(0.8, 0.93, 1, 0.8)**・Color B **(1, 1, 1, 0.6)**・Intensity **1.3** |
| 生きている間 | **Gravity (0, -4, 0)**（舞ってから落ちる）・**Drag 1.5**・End Size Rate **0.3**・End Color **(1, 1, 1, 0)**・Ease **OutQuad**・Converge は外す |
| 回転（Rotation。エンジンに ParticleRotation.md を写した後に出る） | 全部初めの値のまま（0・外す・Stretch 1）。丸い粒は回しても見た目が変わらない |
| 見た目 | **Texture は空**（丸いぼかし。0章）・Blend **Add**・Space **World**（必ず。Local だと跡にならない） |

同じ FootTrail に **Add Component → Effects → DistanceTrail**：
| 値 | |
|---|---|
| Per Meter | **8**（走る 7 m/s で、1秒に約 56 個） |
| Min Speed | 1 |
| Max Step | 3 |
| Ground Only | ✓（跳んでいる間は出ない。空中でも跡を出すなら外す） |

- 粒をもっと目立たせる：Per Meter・Size・Intensity を上げる。もっと静かに：Color の A を下げる。
- 着地したときにまとめて出す（土ぼこり）は、今回は入れていない（ほしければ別の段階で）。

### E-2. 風の範囲に粒（Wind Zone を置くとき）
今の Level01〜07 には Wind Zone がまだ無い。置くときに、**同じ Entity** に次の2つを足す。

粒の見た目は3つから1つ選ぶ（WindParticles は同じ Entity の Wind Zone を読み、1つの Entity に Particle System は1つなので、1つの風に1種類。Wind Zone を重ねて2種類にすると、Rigidbody を押す力も2倍になるのでやらない）：
- **筋（おすすめ）**：細い光の線が風の向きに流れる。風の向き・強さ（Gust）がひと目で分かる。上向きの風なら縦の線
- **葉**：`leaf.png` がくるくる回りながら流れる。森のステージ向き
- **粒**（前の値）：丸い小さな粒。エンジンに ParticleRotation.md を写す前はこれだけ

**Particle System**（WindParticles が上書きする所は「—」。「筋」「葉」の Rotation は、エンジンに ParticleRotation.md を写した後に出る）
| 見出し | 筋（おすすめ） | 葉 | 粒（前の値） |
|---|---|---|---|
| 再生 | **Play On Start を外す**（外さないと、エンジンとここで2倍出る）・Looping・Duration はどれでもよい | ← 同じ | ← 同じ |
| 出す量 | **Rate Over Time 40**（範囲 8×4m のとき。広さに合わせて増やす）・Burst Count 0・Max Particles 256 | **Rate Over Time 10**（8×4m のとき） | Rate Over Time 30（16×5m のとき） |
| Shape | —（Box・Wind Zone の Size になる） | — | — |
| 初めの値 | Lifetime —・Speed —・Size **0.03〜0.05**・Color A **(1, 1, 1, 0.7)**・Color B **(0.8, 0.95, 1, 0.5)**・Intensity 1 | Size **0.5〜0.8**・Color A **(1, 1, 1, 1)**・Color B **(0.85, 0.95, 0.75, 1)**（少し黄緑の葉が混ざる）・Intensity 1 | Size 0.06〜0.12・Color A (1, 1, 1, 0.5)・Color B (0.8, 0.95, 1, 0.35)・Intensity 1 |
| 生きている間 | Gravity —・Drag —・End Size Rate **1**（線の長さを保つ）・End Color **(1, 1, 1, 0)**・Ease Linear | End Size Rate 1・End Color (1, 1, 1, 0)・Ease **InQuad**（終わりぎわに消える） | End Size Rate 0.6・End Color (1, 1, 1, 0)・Ease Linear |
| Rotation | Start Rotation 0・Angular Velocity 0・**Align To Velocity ✓**・**Stretch 14** | **Start Rotation (-180, 180)**・**Angular Velocity (90, 240)**・**Random Spin Direction ✓**・Align は外す・Stretch 1 | 初めの値のまま |
| 見た目 | **Texture は空**（丸いぼかしを伸ばす）・Blend **Add**・Space —（World になる）・Use Fog ✓ | **Texture `resources/Textures/Effects/leaf.png`**・Blend **Normal**・Use Fog ✓ | Texture は空・Blend Add |

（どれも、奥の層の風なら Use Depth Of Field ✓）

**Add Component → Effects → WindParticles**：Follow Rate **4**・Life Rate **0.4〜0.8**（初めの値のまま。3つとも同じ）。

- **筋の長さ**＝ Size × Stretch（0.03〜0.05 × 14 で 0.4〜0.7m）。太さは Size だけで決まるので、細く長くしたいときは Size を下げて Stretch を上げる。
- **筋は風の速さで伸びない**（Stretch は決まった倍率。Gust で速くなっても長さは同じ）。流れる速さで強さが分かる。
- **葉は Normal で重ねる**（Add だと葉が重なった所が白っぽく光る）。並びは奥から順ではないので、たくさん重なると前後がおかしく見えることがある → Rate を上げすぎない。
- Wind Zone の **Line Amount**（画面の風の線）は、カメラが追う物が範囲に入ったときだけ出る。粒はいつも出るので、線は 0 にしてもよい（好みで。筋と線が両方あると、線が多すぎに見える）。
- 粒はふっと現れる（エンジンの粒は「初めの色 → 最後の色」だけで、出始めのフェードインが無い）。目立つときは Color の A を下げる。

### E-3. Player の新しい値（見るだけ）
IceCube の Life に **Fall Restart Delay 0.2**・**Restart Fade Time 0.5** が増える。ファイルに無い値なので初めの値になる。変えたら Ctrl+S。

---

## 確かめること（写した後）

| 何をする | こうなればよい |
|---|---|
| トゲで HP 0 にする | かけらが飛び散って1秒後に暗くなり始め、かけらが消えるころ真っ暗 → レベルの最初から明るくなる |
| 熱湯に入る | 溶けて沈みながら暗くなる → 最初から |
| 穴に落ちる | 0.2 秒ほどで暗くなり始める → 最初から |
| R を押す | すぐ消えずに、幕が閉じて開く |
| ポーズの「やり直す」 | 今までどおり |
| 地面を走る | 足元から水色〜白の小さな粒が舞い上がって落ち、後ろに残る。止まると出ない |
| ジャンプする | 空中では出ない（Ground Only ✓）。着地して走ると、また出る |
| Wind Zone を置いて再生 | 範囲の中に小さな粒が現れて、風の向きに流れて消える。Rotation Z 90 の風なら上へ流れる |
| 筋の値にする（エンジンに ParticleRotation.md を写した後） | 細い線が風の向き（横）に並んで流れる。Rotation Z 90 の風なら縦の線が上へ |
| 葉の値にする（同じ） | 葉がばらばらの向きで、右回り・左回りに回りながら流れ、終わりぎわに消える |

## 作業用コピーで確かめたこと（2026-10-26）
- 今の AL4_01（「Stage7まで」のコミット）を写して、このページのコードを入れた。**Debug・Release ともビルドが通った**（エラー 0。ゲームのファイルから警告 0）。
- **幕**：Level01 を写して、キューブを高い所から始め、Kill Height を上にしたシーンを製品版で撮った。落ちてすぐ暗くなり、レベルの最初から明るくなって、またくり返した（落ちる → 幕 → 読み直し が毎回動く）。砕ける・溶けるは同じ `UpdateDead` を通るが、撮ってはいない。
- **足元の粒**：同じシーンで E-1 の値の FootTrail を Player の子に置き、キューブを Wind Zone で右へ流して撮った。地面を流れている間、足元の後ろに小さな粒が舞い、落ちる間は出なかった。
- **風の粒**：初めは `ParticleSystem::Play` で流す作りにしたが、製品版で粒が1つも出なかった。ログで調べると `IsAlive` が false のまま → 0章の「最初のフレームの Play が消える」動きだった（後でエンジン側で直した）。Emit で出す今の作りに変えて、範囲の中に粒が流れるのを撮った。
- キー・パッドで本当に走る・トゲ・熱湯は試していない（ユーザーの PC に本物の入力は送らないため）。上の表で確かめてほしい。

### 2026-10-27 追加：風の「筋」「葉」の値
- 上の作業用コピーに、エンジンの [ParticleRotation.md](../../MyEngine_v1/Docs/Tasks/ParticleRotation.md)（A〜D）を入れたもので確かめた。このページのコード（WindParticles など）はそのまま。
- Level01 を写して、カメラの見える所（高さ 5m）に 8×4m の Wind Zone（Speed 5・Gust 0.3）を2つ置き、左に「筋」、右に「葉」の値を入れて製品版で撮った。
  - 1回目：筋 Size 0.04〜0.07・Stretch 8、葉 Size 0.25〜0.4・Rate 8 → 筋が短く点に近い、葉が小さくて何か分からない。
  - 2回目：筋 Size 0.03〜0.05・Stretch 14・Color A の A 0.7 → 風の線らしく横に流れた。葉 Size 0.5〜0.8・Rate 10 → 葉の形が分かり、ばらばらの向きで回って流れた（**E-2 の表の値**）。
  - 筋の Wind Zone を Rotation Z 90（上向きの風）にすると、筋が縦になって上へ流れた（Align To Velocity が WindParticles の Gravity の向きに沿う）。
- Gust で筋の流れる速さが変わるのは、止めた絵では確かめていない。実際のレベルの広さ（16×5m など）での Rate も試していない（8×4m の数を広さに合わせて増やす）。

## 困ったとき

| 症状 | 原因 |
|---|---|
| リンクエラー（DistanceTrail・WindParticles） | VS のプロジェクトに .cpp を入れていない（既存の項目を追加） |
| `SceneTransition` が見つからない | 綴り。ClearMenu.cpp と同じく、include は要らない（Script.h から来る） |
| やられても暗くならない | パート A ②を写していない（`SceneManager::ReloadScene` のまま） |
| 足元の粒が出ない | Ground Only ✓ なのに FootTrail が Player の直下でない（親に Rigidbody が無い）・Play On Start が付いている／Rate Over Time が 0 でない（これは出すぎる方）・Min Speed が大きい |
| 粒がキューブに付いてくる・跡にならない | Space が Local |
| 足元の粒が空中でも出る | Ground Only を外している |
| 風の粒が出ない | Play On Start は関係ない（外す）。Rate Over Time が 0・Wind Zone の Enabled が外れている・Speed 0 |
| 風の粒が2倍出る・止まってから出る | Play On Start が付いている（外す） |
| 風の粒が範囲の外まで流れる | Life Rate を下げる（1 で、範囲の端から出た粒がちょうど反対の端まで） |
| Inspector に Rotation が無い | エンジンに ParticleRotation.md を写していない・AL4_01 のサブモジュールを進めていない |
| 筋が横向きのまま・点のまま | Align To Velocity が外れている（Stretch だけだと、いつも画面の横に伸びる）・Stretch が 1 |
| 筋が太い・ぼやける | Size が大きい（太さは Size で決まる。長さは Size × Stretch） |
| 葉が白っぽく光る・重なると明るい | Blend が Add（Normal にする） |
| 葉の代わりに丸いぼかし | Texture が空（`resources/Textures/Effects/leaf.png` を選ぶ） |
| Release で粒が崩れる（エンジンを写した後） | Release が読む `project/MyEngine/Shader` の写しが古い。エンジンをビルドし直す（ParticleRotation.md の6章） |
