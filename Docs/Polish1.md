# 仕上げ1：タイトルの選択・円の粒（吸い寄せ → 花火）・ゴールの粒（写経用）

前：[OutGame3.md](OutGame3.md)（段階3）。設計：[OutGame.md](OutGame.md)

参考：
- エンジン：`Systems/ParticleSystem.h`（`Play`・`Stop(entity, true)`：出ている粒も消す）・`Components/ParticleSystemComponent.h`（`converge`・`gravity`・`drag`・Shape の Sphere / Circle）
- C++：`std::exp`（`<cmath>`。なめらかに近づける）・`std::optional`

## この段階でやること

ユーザーの希望（2026-10-26）：
1. **タイトル画面で、選んでいる項目が分かりづらい**
2. **ゴールの粒が、最初の1回しか飛ばない**
3. **もう一度ジャンプできる円（JumpOrb）・矢印に飛ぶ円（LaunchOrb）**：ふだんは真ん中へ粒が吸い寄せられ、使ったらはじけ飛ぶ。**はじけ方は円形ではなく、花火のように落ちながら**

| パート | ファイル | 新／変 | 中身 |
|---|---|---|---|
| A | `Source/Scripts/UI/MenuBase.h / .cpp` | h は全部差し替え・cpp は4か所 | 選んだ項目を少し横へずらす（Selected Offset） |
| B | `Source/Scripts/Gimmicks/JumpRing.h / .cpp` | **全部差し替え** | 使ったら：吸い寄せの粒を消して、はじける粒を出す。出たら：吸い寄せをまた始める |
| C | `Source/Scripts/Gimmicks/JumpOrb.cpp`・`LaunchOrb.cpp` | 1行ずつ | Fields で JumpRing の値も出す |
| D | `Source/Scripts/GoalDoor.cpp` | 1か所 | 粒の子が見つからなければ Console に知らせる |
| E | エディタ | 変 | Title の見た目・円の粒・ゴールの粒の名前 |

**写す順番**：A → B → C → D（ここでビルド）→ E。新しいファイルは無い（VS のプロジェクトはそのまま）。

---

## 1. なぜそうなっていたか

### タイトルの選択が分かりづらい
Title シーンの **SelectedStyle が「白・Glow 0」**で、選んでいない Start・Quit（白・Glow 0）と**まったく同じ見た目**だった。選ぶと見本の飾りを写す作りなので、見本が同じだと何も変わらない。
→ 選んでいない項目を薄く（A 0.45）、SelectedStyle を白＋氷色の Glow にする。さらに、選んだ項目が**右へ 16px ゆっくりずれる**動きを MenuBase に足した（0章の「少しのスライド」の範囲）。

### ゴールの粒が最初の1回しか飛ばない
作業用コピーで「クリア → やり直し → もう一度クリア」を自動で起こして撮ったところ、**2回目も粒は昇った**。コードの問題ではなく、シーン側の置き方の問題だと考えられる。見つかったのは：
- **Goal.prefab の子の名前が `Clear Particle`（間にスペース）**。GoalDoor の Particle Name は `ClearParticle`（スペースなし）なので、**探しても見つからず、Play されない**。
- **Level01・Level02 のファイルの Goal は、まだ前のまま**（GoalDoor の値が Next Level Delay だけ・ClearParticle の子が無い）。エディタで直して保存していないか、Goal.prefab だけを直した状態。**Prefab を直しても、シーンに置いてある Goal は変わらない**（このエンジンのシーンは Prefab とつながっていない）。そのため、エディタで直したシーン（1つ目）では出て、ファイルから読んだシーン（Next で行く Level02・やり直し・タイトルから入り直し）では出ない、になる。

→ パート D で、子が見つからないときに Console に「GoalDoor：子に「ClearParticle」が無いので、粒は昇りません」と出るようにした。E-3 で名前を直して、**Level01・Level02 の両方に置いて保存**する。

### 円の粒
Level02 の JumpOrb には、もう吸い寄せの粒（Converge）があるが、出る円の半径が 0.3m（輪の半径は 1m）で小さく、真ん中の近くだけで動いていた。使ったときの粒は無かった。

---

## パート A：`Source/Scripts/UI/MenuBase.h / .cpp`

### `MenuBase.h`（全部差し替え）
```cpp
#pragma once
#include <string>
#include <vector>
#include <optional>

#include <MyEngine/Components/TextComponent.h>
#include <MyEngine/ECS/Script.h>

/// <summary>
/// メニュー（項目を選んで決める画面）の共通部分。これだけでは付けられない（SCRIPT で登録しない。JumpRing と同じ）
/// <para>項目は、子の Items の子（有効な物だけ。Hierarchy の上から順）。何をするかは派生クラスの Decide が、項目の名前で決める</para>
/// <para>選んだ項目には、見本の Text（一番上の直下の子の SelectedStyle）の飾り（色・縁取り・影・光・Shine・Motion）を写す。
/// 選んでいない項目は、元の飾りに戻す</para>
/// <para>キー：↑↓（Columns が 2 以上なら ←→ も）で選ぶ / 決める / 戻る。マウス：項目に乗ったら選ぶ・左クリックで決める・右クリックで戻る</para>
/// <para>開いている UI Screen の中に置けば、ポーズ中（ゲームが止まっている間）も呼ばれる</para>
/// </summary>
class MenuBase : public Script {
public:
	void Start() final;  // 入力を受ける準備（派生では書かない。最初に何かするなら OnStart）
	void Update() final; // 入力 → 選ぶ・決める・戻る（派生では書かない。何をするかは下の virtual で）
	void Fields(ComponentUI& ui) override;

protected:
	// ===== 派生クラスで書く =====
	// 最初の Update の前に1回（Start の代わり。最初に選ぶ項目を Select で決めるなど）
	virtual void OnStart() {}
	// 毎フレーム、入力を見る前に（演出を進めるなど。Update の代わり）
	virtual void OnUpdate() {}
	// 決めた。index は項目の番号（上から 0, 1, …）、itemName は項目の Entity の名前
	virtual void Decide(int index, const std::string& itemName) = 0;
	// 戻る（Esc・パッドの B・右クリック）。何もしない画面は書かない
	virtual void OnCancel() {}
	// index の項目を決めてよいか（開いていないステージなど）
	virtual bool CanDecide(int /*index*/) const { return true; }
	// 決められない項目で決めた（鍵を揺らすなど）
	virtual void OnDecideRejected(int /*index*/) {}
	// 選んでいる項目が変わった（最初の1回も呼ぶ）。説明を出す・カーソルを動かすなど
	virtual void OnSelect(int /*index*/) {}
	// 入力を受けない（上に別の画面が開いているなど）。幕の間は、これとは別に受けない
	virtual bool IsInputBlocked() const { return false; }

	// ===== 派生クラスから使う =====
	// 項目の並び（子の Items の、有効な子。Hierarchy の上から順）
	// Items の外の物も項目にしたい画面（ステージ選択の Back）は、派生クラスで書き換えて後ろに足す
	virtual std::vector<Handle<Entity>> CollectItems() const;
	// 選ぶ項目を決める（OnStart で最初の項目を変えたいとき。OnSelect は次の Update で呼ぶ）
	void Select(int index);
	int GetSelected() const { return selected_; }
	// 横に並べる数（1：縦の並び。2 以上：格子）
	void SetColumns(int columns) { columns_ = columns; }
	int GetColumns() const { return columns_; }

	bool isDecided_ = false; // もう決めた（消える動き・幕が閉じる間に、もう一度決めないように）

private:
	/// <summary>
	/// Text の飾りの部分（文字・フォント・大きさ・位置の合わせ方は入れない。選んだ・外したで入れ替える物だけ）
	/// </summary>
	struct TextStyle {
		Vector4 color;
		float outlineWidth = 0.0f;
		Vector4 outlineColor;
		bool shadow = false;
		Vector2 shadowOffset;
		Vector4 shadowColor;
		float glowWidth = 0.0f;
		Vector4 glowColor;
		bool shine = false;
		Vector4 shineColor;
		float shineWidth = 0.0f;
		float shineDuration = 0.0f;
		float shineInterval = 0.0f;
		TextMotion motion = TextMotion::None;
		float motionAmount = 0.0f;
		float motionSpeed = 0.0f;
		float motionSpread = 0.0f;

		// Text から飾りを取り出す・Text に飾りを書く
		static TextStyle From(const TextComponent& text);
		void ApplyTo(TextComponent& text) const;
	};

	// キー・パッド・スティックで、選ぶ項目を動かす（動かしたら true）
	bool MoveByInput(int count, const Vector2& stick);
	// マウスが乗っている項目（無ければ -1）
	int FindItemUnderMouse(const std::vector<Handle<Entity>>& items) const;
	// 項目の上にマウスがあるか（Image があれば、その形。無ければ Text の文字のかたまり＋Hit Padding）
	bool IsMouseOverItem(Handle<Entity> item) const;
	// 選んでいる項目を決める（決められなければ OnDecideRejected）
	void TryDecide(const std::vector<Handle<Entity>>& items, int index);
	// 選んでいる項目に見本の飾りを、ほかの項目に元の飾りを書く
	void ApplyLook(const std::vector<Handle<Entity>>& items);
	// 選んでいる項目を Selected Offset だけ、ゆっくりずらす（外れた項目は元の位置へ戻る）。Selected Offset が 0 なら何もしない
	void ApplySlide(const std::vector<Handle<Entity>>& items);

	// ===== Inspector に出す・保存する値 =====
	std::string styleName_ = "SelectedStyle";   // 選んだ項目の見本の Text の名前（一番上の直下の子。Text の Enabled は外して置く）
	int columns_ = 1;                           // 横に並べる数（1：縦の並び・端から反対の端へ回る。2 以上：格子・端で止まる）
	bool useMouse_ = true;                      // マウスでも選ぶ・決める
	float hitPadding_ = 12.0f;                  // マウスの当たりを、文字のかたまりより広げる（ピクセル。Image の項目には効かない）
	Vector2 selectedOffset_ = {0.0f, 0.0f};     // 選んだ項目をずらす量（ピクセル。右・上が＋。0 ならずらさない）
	float slideTime_ = 0.12f;                   // ずれる・戻るのにかかる秒数の目安（小さいほど、すばやく動く）

	// ===== 実行中だけの値 =====
	int selected_ = 0;                      // 選んでいる項目（上から 0, 1, …）
	int notifiedSelected_ = -1;             // 最後に OnSelect で知らせた項目（変わったときだけ知らせる）
	bool isReady_ = false;                  // 入力を受けてよい（最初の1フレームは受けない。この画面を開いた Enter で、そのまま決めないように）
	Vector2 previousStick_ = {0.0f, 0.0f};  // 前のフレームのスティック（倒した瞬間だけ動かす）
	std::vector<std::optional<TextStyle>> originalStyles_; // 項目の元の飾り（Text が付いている項目の分だけ覚える。外れた項目をこれに戻す）
	std::vector<std::optional<Vector3>> basePositions_;    // 項目の元の位置（ずらす前。最初のフレームに覚える）
	std::vector<float> slideAmounts_;                      // 項目のずれ具合（0：元の位置 〜 1：Selected Offset だけずれた）
};
```

### `MenuBase.cpp`（4か所）

① include に `<cmath>` を足す：
```cpp
#include <algorithm>
#include <cmath>
```

② `Fields` の Hit Padding の行の下に足す：
```cpp
	ui.Field("Selected Offset", selectedOffset_, Tip("選んだ項目を、この分だけゆっくりずらす（ピクセル。右・上が＋）。(0, 0) ならずらさない。例：(16, 0) で少し右へ"));
	ui.Field("Slide Time", slideTime_, AtLeast(0.01f).Tip("ずれる・戻るのにかかる秒数の目安。小さいほどすばやい"));
```

③ `Update` の最後の方：
前：
```cpp
	// --- 見た目・選んだ項目が変わったら知らせる ---
	ApplyLook(items);
```
後：
```cpp
	// --- 見た目・選んだ項目が変わったら知らせる ---
	ApplyLook(items);
	ApplySlide(items);
```

④ ファイルの一番下（`ApplyLook` の後）に足す：
```cpp
void MenuBase::ApplySlide(const std::vector<Handle<Entity>>& items) {
	// --- ずらさない画面（Selected Offset が 0）は、位置に触らない ---
	if (selectedOffset_.x == 0.0f && selectedOffset_.y == 0.0f) {
		return;
	}
	// --- 項目の数が変わったら、覚えた位置を忘れる（ApplyLook と同じ）---
	if (basePositions_.size() != items.size()) {
		basePositions_.assign(items.size(), std::nullopt);
		slideAmounts_.assign(items.size(), 0.0f);
	}
	// 1フレームで、目標までの残りのこの割合だけ近づく（指数の近づけ方。フレームの速さが変わっても、同じ秒数で同じだけ動く）
	// 止めない時間で数える（ポーズ画面でも動く）
	const float rate = 1.0f - std::exp(-Time::GetUnscaleDeltaTime() / slideTime_);
	for (size_t i = 0; i < items.size(); ++i) {
		TransformComponent* itemTransform = EntityManager::Get<TransformComponent>(items[i]);
		if (itemTransform == nullptr) {
			continue;
		}
		// --- 元の位置は、最初のフレームに覚える（ずらした位置を元と思わないように、ここ以外では覚えない）---
		if (!basePositions_[i]) {
			basePositions_[i] = itemTransform->translation;
		}
		// --- 選んだ項目は 1 へ、外れた項目は 0 へ近づけて、その割合だけずらす ---
		const float target = (static_cast<int>(i) == selected_) ? 1.0f : 0.0f;
		slideAmounts_[i] += (target - slideAmounts_[i]) * rate;
		const Vector3& base = *basePositions_[i];
		itemTransform->translation = {base.x + selectedOffset_.x * slideAmounts_[i], base.y + selectedOffset_.y * slideAmounts_[i], base.z};
	}
}
```

### 解説
- **ずらし方**：項目ごとに「ずれ具合」（0〜1）を持ち、選んだ項目は 1 へ、外れた項目は 0 へ近づける。位置は「元の位置 ＋ Selected Offset × ずれ具合」。元の位置は最初のフレームに1回だけ覚える（ずらした後の位置を元と思うと、どんどんずれていくので）。
- **なめらかに近づける式**：`rate = 1 − exp(−経過秒 ÷ Slide Time)` を毎フレーム、残りに掛ける。Slide Time 秒で残りの約 63%、3倍の秒でほぼ全部近づく。フレームの速さが変わっても同じ秒数で同じだけ動く（`残り × 0.2` のように決まった割合で近づけると、速いパソコンほど早く着いてしまう）。
- **止めない時間**（`GetUnscaleDeltaTime`）で数えるので、ポーズ画面でも動く。
- **Selected Offset が (0, 0) の画面は位置に触らない**ので、ステージ選択・クリア・ポーズは今までどおり（使いたければ Inspector で入れる）。
- マウスの当たりは、ずれた後の位置で見る（文字と一緒に当たりも動く）。

---

## パート B：`Source/Scripts/Gimmicks/JumpRing.h / .cpp`（全部差し替え）

### `JumpRing.h`
```cpp
#pragma once
#include <string>

#include <MyEngine/ECS/Script.h>

/// <summary>
/// ジャンプで使う輪（水色の円 JumpOrb・緑の円 LaunchOrb の共通部分）。これだけでは付けられない（SCRIPT で登録しない）
/// <para>氷のキューブが輪に触れている間、ジャンプで使える。使ったら、キューブが輪から一度出るまで使えない（入り直すと、また使える）</para>
/// <para>何が起きるか（もう一度跳ぶ・吹っ飛ぶ）は IceCube が決める。ここは「使えるか」を輪ごとに覚えるだけ</para>
/// <para>IceCube は触れている輪を GetComponent&lt;JumpRing&gt;() で集める（JumpOrb・LaunchOrb のどちらも見つかる）</para>
/// <para>粒（どちらも無くてよい）：子の Gather Particle（ふだん真ん中へ吸い寄せられる粒。Converge・Play On Start）は、使ったら消えて、
/// キューブが輪から出たら、また流れ始める。子の Burst Particle（Play On Start を外す）は、使った瞬間に外へはじけ飛ぶ</para>
/// </summary>
class JumpRing : public Script {
public:
	void OnTriggerExit(GameObject other) override; // キューブが出た → また使える
	void Fields(ComponentUI& ui) override;          // 派生クラスの Fields から、最初に呼ぶ

	// ===== IceCube から使う =====
	// 使えるか（まだ使っていない）
	bool IsReady() const { return !isUsed_; }
	// 使う（キューブが輪から出るまで、使えない）。粒はここで、ほかの見た目は OnUse で
	void Use();

protected:
	// 使った瞬間に呼ぶ（輪が広がる・ふくらむなどの見た目を始める。派生クラスで書く）
	virtual void OnUse() {}
	// 使ったか（見た目を変えるのに使う）
	bool IsUsed() const { return isUsed_; }

private:
	// ===== Inspector に出す・保存する値 =====
	std::string gatherParticleName_ = "GatherParticle"; // ふだん吸い寄せられる粒の Particle System（この Entity の直下の子）
	std::string burstParticleName_ = "BurstParticle";   // 使ったときにはじけ飛ぶ粒の Particle System（この Entity の直下の子）

	// ===== 実行中だけの値 =====
	bool isUsed_ = false; // 使った（キューブが出るまで使えない）
};
```

### `JumpRing.cpp`
```cpp
#include "JumpRing.h"

#include <MyEngine/Systems/ParticleSystem.h>

#include "Source/Scripts/IceCube.h"

//=============================================================================
// Inspector に出す・保存する値（派生クラスの Fields から、最初に呼ぶ）
//=============================================================================
void JumpRing::Fields(ComponentUI& ui) {
	ui.Field("Gather Particle", gatherParticleName_, Tip("ふだん真ん中へ吸い寄せられる粒の Particle System の名前（直下の子。Converge を付けて Play On Start）。使ったら消えて、キューブが出たらまた流れる。無ければ何もしない"));
	ui.Field("Burst Particle", burstParticleName_, Tip("使ったときに外へはじけ飛ぶ粒の Particle System の名前（直下の子。Play On Start を外して、Burst で出す）。無ければ何もしない"));
}

//=============================================================================
// 使う・戻る
//=============================================================================
void JumpRing::Use() {
	isUsed_ = true;
	// --- 粒：吸い寄せられていた粒を消して、外へはじけ飛ばす ---
	if (const GameObject gather = gameObject.FindChild(gatherParticleName_)) {
		ParticleSystem::Stop(gather.handle, true); // true：出ている粒も消す（集まっていた粒が、はじけた粒に入れ替わる）
	}
	if (const GameObject burst = gameObject.FindChild(burstParticleName_)) {
		ParticleSystem::Play(burst.handle); // Burst Count の数を一度に出す
	}
	OnUse(); // ほかの見た目（派生クラス）
}

void JumpRing::OnTriggerExit(GameObject other) {
	// キューブが輪から出たら、また使える（入り直すと使える）
	if (other.GetComponent<IceCube>() == nullptr || !isUsed_) {
		return;
	}
	isUsed_ = false;
	// --- 粒：また吸い寄せ始める ---
	if (const GameObject gather = gameObject.FindChild(gatherParticleName_)) {
		ParticleSystem::Play(gather.handle);
	}
}
```

### 解説
- **吸い寄せの粒（Gather Particle）**：Particle System の **Converge**（出た所から Entity の真ん中へ、寿命の終わりにちょうど着く）を使う。コードは何もしない（Play On Start で流れ続ける）。
- **使った瞬間**：`ParticleSystem::Stop(gather, true)`。2つ目の `true` は「出ている粒も消す」。吸い寄せられていた粒がパッと消えて、はじける粒（Burst Particle）に入れ替わる。はじける粒は `Play` で Burst Count の数が一度に出る。
- **キューブが輪から出たら**：使える状態に戻るのと一緒に、吸い寄せを `Play` で始め直す。使っていないのに出ただけ（通り抜けた）のときは何もしない。
- 2つの円で同じなので、共通の親（JumpRing）に書いた。輪がふくらむ・広がるなどの円ごとの見た目は今までどおり `OnUse`。
- 子が無ければ何もしない（粒を置かない円もそのまま動く）。

---

## パート C：`JumpOrb.cpp`・`LaunchOrb.cpp`（1行ずつ）

それぞれの `Fields` の最初の行に足す（JumpRing の Gather Particle・Burst Particle を Inspector に出して保存する。MenuBase の Fields と同じ書き方）：

`JumpOrb.cpp`
```cpp
void JumpOrb::Fields(ComponentUI& ui) {
	JumpRing::Fields(ui);
	ui.Label("キューブが触れている間、空中でもう一度跳べる。使ったら、円から一度出るまで使えない");
```

`LaunchOrb.cpp`
```cpp
void LaunchOrb::Fields(ComponentUI& ui) {
	JumpRing::Fields(ui);
	ui.Label("キューブが輪の中でジャンプすると、線の始まりに寄ってから、子の Arrows（Arrow Strip）の線に沿って吹っ飛ぶ。Arrows が無ければ、この Entity の右（Rotation の Z で回る）へまっすぐ");
```

---

## パート D：`Source/Scripts/GoalDoor.cpp`（1か所）

前：
```cpp
	if (const GameObject particle = gameObject.FindChild(particleName_)) {
		ParticleSystem::Play(particle.handle);
	}
```
後：
```cpp
	if (const GameObject particle = gameObject.FindChild(particleName_)) {
		ParticleSystem::Play(particle.handle);
	} else {
		LogManager::Warning("GoalDoor：子に「" + particleName_ + "」が無いので、粒は昇りません（子の名前と Particle Name を同じにする）");
	}
```

---

## パート E：エディタ

### E-1. Title：選んだ項目を分かるように（Title シーン）
| Entity | 変える値 |
|---|---|
| Items の **Start**・**Quit** | Text の Color の **A を 0.45**（選んでいないときは薄い） |
| **SelectedStyle** | Color (1, 1, 1, **1**)・**Glow Width 10**・**Glow Color (0.75, 0.93, 1.0, 0.6)**（氷色）。ほかは無し |
| TitleUI の **Title Menu** | **Selected Offset (16, 0)**・Slide Time 0.12 |

→ 選んだ項目だけが白く明るく、うっすら氷色に光って、右へ 16px すっと出る。ほかは薄く元の位置へ戻る。
- ポーズ画面（Pause.uiscreen）も同じにするなら、PauseMenu の Selected Offset と Items の A を同じように。

### E-2. 円の粒（JumpOrb.prefab・LaunchOrb.prefab・シーンに置いた円）
円の子に2つの Particle System を置く。**名前はコードが探すので、このとおりに**。
Level02 の JumpOrb にある「Particle System」は、名前を **GatherParticle** に変えて値を直せばよい。

**GatherParticle**（ふだん、まわりから真ん中へ吸い寄せられる）
| 見出し | 値 |
|---|---|
| Transform | 位置 (0, 0, 0)・Scale (1, 1, 1) |
| 再生 | **Play On Start ✓**・Looping ✓・Duration 1 |
| 出す量 | Rate Over Time **40**・Burst Count 0 |
| Shape | **Circle**・Radius **1.5**（輪の半径 1m より外から）・**From Surface ✓**（輪の線の上から出る） |
| 初めの値 | Lifetime 0.9〜1.3・Size **0.08〜0.16**・Color A **(0.75, 0.95, 1, 1)**・Color B (1, 1, 1, 1)・Intensity **2.5** |
| 生きている間 | End Size Rate **0.4**・End Color **(1, 1, 1, 0.3)**・**Converge ✓**・Converge Ease **InQuad**（だんだん速く吸い込まれる） |
| 見た目 | Blend Add・Space World・**Use Fog を外す** |

**BurstParticle**（使ったとき、花火のようにはじけて落ちる）
| 見出し | 値 |
|---|---|
| Transform | 位置 (0, 0, 0)・Scale (1, 1, 1) |
| 再生 | **Play On Start を外す**・**Looping を外す**・Duration 1.5 |
| 出す量 | Rate Over Time **0**・**Burst Count 45** |
| Shape | **Sphere**・Radius **0.15**・From Surface は外す（真ん中から、全部の向きへ） |
| 初めの値 | Lifetime **0.9〜1.4**・Speed **3〜6**・Size 0.07〜0.16・Color A (0.75, 0.95, 1, 1)・Color B (1, 1, 1, 1)・Intensity **3** |
| 生きている間 | **Gravity (0, -7, 0)**（下へ落ちる）・**Drag 1.2**・End Size Rate 0.3・End Color (1, 1, 1, 0)・Ease **InQuad**・Converge は外す |
| 見た目 | Blend Add・Space **World**（円が動いても、粒はその場で落ちる）・Use Fog を外す |

- **LaunchOrb**（緑の円）は、両方の Color A を緑っぽく **(0.6, 1.0, 0.7, 1)** にすると、円の色と合う。
- 円の Inspector の **Gather Particle**・**Burst Particle** が、子の名前と同じか見る（初めの値のままなら同じ）。
- **Prefab を直しても、シーンに置いてある円は変わらない**。Level02 の JumpOrb にも同じ2つを置く（または、置き直す）。

**花火に見える仕組み**：
- **Circle ではなく Sphere**：Circle は画面の面（XY）の輪の外向きに飛ぶので、きれいな輪に広がる（＝円形に見える）。Sphere は奥・手前も含めた全部の向きへ飛ぶので、画面では**中まで詰まった、ばらばらな広がり**になる（花火の玉）。
- **Gravity (0, -7, 0)**：飛び出した直後は速さが勝って丸く広がり、Drag で横の勢いが落ちるにつれて、**重力で下へ垂れながら消える**（しだれ花火の形）。
- **Ease InQuad**：明るさ・大きさを寿命の後ろの方まで保って、最後にすっと消える。
- もっと大きく開く：Speed を上げる。もっと垂れる：Gravity を強く（-10 など）・Lifetime を長く。

### E-3. ゴールの粒の名前（Goal.prefab・Level01・Level02）
- Goal.prefab の子 **`Clear Particle` → `ClearParticle`**（スペースを消す）。または GoalDoor の Particle Name を `Clear Particle` にする（どちらかを合わせる）。
- **Level01・Level02 の Goal にも、ClearParticle の子と GoalDoor の値（OutGame3.md の H-3・H-4）を入れて、Ctrl+S**。Prefab だけを直しても、シーンに置いた Goal は変わらない。
- 遊んで Console に「GoalDoor：子に「ClearParticle」が無いので」が出たら、そのシーンの Goal に子が無い・名前が違う。

---

## 確かめること（写した後）

| 何をする | こうなればよい |
|---|---|
| Title を開く | Start だけ白く明るく光って、少し右へ出ている。Quit は薄い |
| ↓・マウスで Quit へ | Quit がすっと右へ出て光り、Start は薄くなって左へ戻る |
| Level02 の円の近く | 輪のまわりから、白〜水色の粒が真ん中へ吸い込まれ続ける |
| 円の中でジャンプ（使う） | 吸い込まれていた粒が消え、真ん中から粒がばらばらに飛び散って、下へ垂れながら消える |
| 円から出る | また吸い込みが始まる |
| 円を使わずに通り抜ける | 吸い込みは止まらない |
| Level01 をゴール → Next → Level02 をゴール | どちらも粒が昇る（Console に GoalDoor の警告が出ない） |
| やり直し（R）・タイトルから入り直してゴール | 毎回粒が昇る |

## 作業用コピーで確かめたこと（2026-10-26）
- ユーザーが段階3 を写した今の AL4_01 を写して、このページのコードを入れた。**Debug・Release ともビルドが通った**（エラー 0。ゲームのファイルから警告 0）。
- **ゴールの粒**：キューブをゴールの中から始めるシーンで、一時的なコードで5秒ごとに ReloadScene した。**2回目のクリアでも粒が昇り、クリア画面も出た**（コードは毎回動く）。
- **タイトル**：E-1 の値を入れて製品版で撮った。Start が白く光って右へ出て、Quit が薄い。一時的なコードで Quit を選ぶと、Quit が光って右へ出て、Start が薄く左へ戻った。
- **円**：Level02 を写して E-2 の値の2つの粒を JumpOrb に置き、一時的なコードで3秒後に `Use`・4.5秒後に `OnTriggerExit` を呼んで、0.1 秒ごとに撮った：使う前は輪のまわりから粒が吸い込まれ、使った瞬間に真ん中から全部の向きへはじけ、0.1〜0.6 秒で下へ垂れながら地面のあたりで消えた。出た後は、また吸い込みが始まった。初めに試した Circle（輪）のはじけ方は、円形に広がるだけだったので Sphere＋下向きの Gravity に変えた。
- キー・マウス・本当のジャンプで円を使う所は試していない（ユーザーの PC に本物の入力は送らないため）。上の表で確かめてほしい。

## 困ったとき

| 症状 | 原因 |
|---|---|
| `JumpRing::Fields` で「override するものが無い」 | パート B の JumpRing.h を写していない |
| 円の Inspector に Gather Particle・Burst Particle が出ない | パート C の `JumpRing::Fields(ui);` を書き忘れた |
| 選んでもずれない | Title Menu の Selected Offset が (0, 0)・パート A ③の `ApplySlide(items);` を書き忘れた |
| 選んだ項目がだんだん遠くへずれていく | ApplySlide の「元の位置は最初のフレームだけ覚える」（`if (!basePositions_[i])`）を書き忘れた |
| 選んでも見た目が変わらない | SelectedStyle が項目と同じ色・Glow 0 のまま（E-1） |
| 円で粒がはじけない | 子の名前が BurstParticle でない・Play On Start が付いている（最初に1回出て終わる）・Burst Count が 0 |
| はじけた粒が輪の形に広がる | Shape が Circle のまま（Sphere にする） |
| はじけた後、吸い込みが戻らない | 子の名前が GatherParticle でない |
| 吸い込みが最初から止まっている | GatherParticle の Play On Start が外れている |
| ゴールで粒が出ない | Console の GoalDoor の警告を見る（E-3） |
