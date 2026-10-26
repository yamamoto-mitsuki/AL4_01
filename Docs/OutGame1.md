# 段階1：メニューの土台とタイトル（写経用）

設計：[OutGame.md](OutGame.md)（3章 MenuBase・4章 タイトル・7章 段階）

参考：
- エンジン：`MyEngine_v1/project/MyEngine/Systems/SpriteRenderSystem.h`（`CalcScreenQuad`・`IsInsideQuad`）・`Systems/TextRenderSystem.h`（`CalcTextSize`）・`Input/InputManager.h`・`UI/UIScreen.h`・`Scene/SceneTransition.h`・`Window/WindowManager.h`（`GetMainHWND`）・`String/ConvertString.h`・`Engine.h`（エディタは前回のシーンから始まる）
- 今の `Source/Scripts/UI/PauseMenu.*`（中身の大半を MenuBase へ移す）

## この段階でやること

| パート | ファイル | 新／変 | 中身 |
|---|---|---|---|
| A | `Source/Scripts/GameTitle.h` | 新 | ゲームの名前（1か所） |
| B | `Source/Scripts/UI/MenuBase.h / .cpp` | 新 | メニューの共通部分（PauseMenu から移す＋格子・マウス・スティック） |
| C | `Source/Scripts/UI/PauseMenu.h / .cpp` | **全部差し替え** | MenuBase の派生に。「タイトルへ」を作る |
| D | `Source/Scripts/UI/TitleMenu.h / .cpp` | 新 | タイトルのメニュー（はじめる・おわる） |
| E | `main.cpp` | 全部差し替え | ウィンドウの名前・最初のシーン |
| F | VS のプロジェクト | — | 新しいファイルを入れる |
| G | エディタ：`Title` シーン | 新 | 背景をぼかしたタイトル |
| H | エディタ：`Pause.uiscreen` | 確かめるだけ | Title の項目はもうある |

**写す順番**：A → B → C → F（ここで一度ビルドして、ポーズが今までどおり動くのを確かめる）→ D → E → F → G。

**エンジンは変えない**（全部ゲームのファイル）。

---

## パート A：`Source/Scripts/GameTitle.h`（新しいファイル）

ゲームの名前を1か所に置く。タイトルのロゴ（パート D）とウィンドウのタイトルバー（パート E）がここを読む。**名前を変えるときは、この1行だけ**。

```cpp
#pragma once
#include <string_view>

// ゲームの名前（タイトル画面のロゴの文字・ウィンドウのタイトルバー）。名前を変えるときは、ここだけ直す
// 中身は UTF-8（ソースは UTF-8 で保存する。/utf-8 でビルドしているので、日本語をそのまま書ける）
inline constexpr std::string_view kGameTitle = "霧氷ジャンプ";
```

- `std::string_view` は「文字列を持たずに、どこかにある文字を指すだけ」の型。`"…"` の文字はプログラムの中にずっとあるので、`constexpr` で置ける（`std::string` は `constexpr` の変数にできない）。
- 名前の候補は OutGame.md 2.1。

---

## パート B：`Source/Scripts/UI/MenuBase.h / .cpp`（新しいファイル）

今の PauseMenu の「項目を集める・選ぶ・見本の飾りを写す・決める」を、そのまま親クラスへ移した。足したのは4つ：
1. **格子**（Columns が 2 以上なら ←→ も。段階2 のステージ選択で使う）
2. **マウス**（Text の文字のかたまりに当たる。Image を置かなくてよい）
3. **スティック**（倒した瞬間だけ1つ動く）
4. **入力を受けない時**（最初の1フレーム・幕の間・派生クラスが止めている間）

### `MenuBase.h`

```cpp
#pragma once
#include <string>
#include <vector>

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
	void Start() final; // 入力を受ける準備（派生では書かない。最初に何かするなら OnStart）
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
	std::vector<Handle<Entity>> CollectItems() const;
	// 選ぶ項目を決める（OnStart で最初の項目を変えたいとき。OnSelect は次の Update で呼ぶ）
	void Select(int index);
	int GetSelected() const { return selected_; }
	// 横に並べる数（1：縦の並び。2 以上：格子）
	void SetColumns(int columns) { columns_ = columns; }

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

	// ===== Inspector に出す・保存する値 =====
	std::string styleName_ = "SelectedStyle"; // 選んだ項目の見本の Text の名前（一番上の直下の子。Text の Enabled は外して置く）
	int columns_ = 1;                         // 横に並べる数（1：縦の並び・端から反対の端へ回る。2 以上：格子・端で止まる）
	bool useMouse_ = true;                    // マウスでも選ぶ・決める
	float hitPadding_ = 12.0f;                // マウスの当たりを、文字のかたまりより広げる（ピクセル。Image の項目には効かない）

	// ===== 実行中だけの値 =====
	int selected_ = 0;                      // 選んでいる項目（上から 0, 1, …）
	int notifiedSelected_ = -1;             // 最後に OnSelect で知らせた項目（変わったときだけ知らせる）
	bool isReady_ = false;                  // 入力を受けてよい（最初の1フレームは受けない。この画面を開いた Enter で、そのまま決めないように）
	Vector2 previousStick_ = {0.0f, 0.0f};  // 前のフレームのスティック（倒した瞬間だけ動かす）
	std::vector<TextStyle> originalStyles_; // 項目の元の飾り（最初のフレームに覚える。外れた項目をこれに戻す）
};
```

### `MenuBase.cpp`

```cpp
#include "MenuBase.h"

#include <algorithm>

#include <MyEngine/ECS/EntityManager.h>
#include <MyEngine/Input/InputManager.h>
#include <MyEngine/Systems/SpriteRenderSystem.h>
#include <MyEngine/Systems/TextRenderSystem.h>

namespace {
// 上・下・左・右・決める・戻るのキー（キーボードとパッドのどちらでも）
bool IsUpTriggered() { return InputManager::IsKeyTriggered(DIK_UP) || InputManager::IsKeyTriggered(DIK_W) || InputManager::IsButtonTriggered(GamepadButton::DpadUp); }
bool IsDownTriggered() { return InputManager::IsKeyTriggered(DIK_DOWN) || InputManager::IsKeyTriggered(DIK_S) || InputManager::IsButtonTriggered(GamepadButton::DpadDown); }
bool IsLeftTriggered() { return InputManager::IsKeyTriggered(DIK_LEFT) || InputManager::IsKeyTriggered(DIK_A) || InputManager::IsButtonTriggered(GamepadButton::DpadLeft); }
bool IsRightTriggered() { return InputManager::IsKeyTriggered(DIK_RIGHT) || InputManager::IsKeyTriggered(DIK_D) || InputManager::IsButtonTriggered(GamepadButton::DpadRight); }
bool IsDecideTriggered() { return InputManager::IsKeyTriggered(DIK_RETURN) || InputManager::IsKeyTriggered(DIK_SPACE) || InputManager::IsButtonTriggered(GamepadButton::A); }
bool IsCancelTriggered() { return InputManager::IsKeyTriggered(DIK_ESCAPE) || InputManager::IsButtonTriggered(GamepadButton::Start) || InputManager::IsButtonTriggered(GamepadButton::B); }

constexpr float kStickThreshold = 0.5f; // スティックをこれより倒したら1つ動く（倒したままでは動き続けない）

// スティックを、その向きへ倒した瞬間か（前のフレームは倒していなかった）。sign は +1（右・上）か -1（左・下）
bool IsStickTriggered(float current, float previous, float sign) { return current * sign > kStickThreshold && previous * sign <= kStickThreshold; }
} // namespace

//=============================================================================
// Text の飾り
//=============================================================================
MenuBase::TextStyle MenuBase::TextStyle::From(const TextComponent& text) {
	return {
	    .color = text.color,
	    .outlineWidth = text.outlineWidth,
	    .outlineColor = text.outlineColor,
	    .shadow = text.shadow,
	    .shadowOffset = text.shadowOffset,
	    .shadowColor = text.shadowColor,
	    .glowWidth = text.glowWidth,
	    .glowColor = text.glowColor,
	    .shine = text.shine,
	    .shineColor = text.shineColor,
	    .shineWidth = text.shineWidth,
	    .shineDuration = text.shineDuration,
	    .shineInterval = text.shineInterval,
	    .motion = text.motion,
	    .motionAmount = text.motionAmount,
	    .motionSpeed = text.motionSpeed,
	    .motionSpread = text.motionSpread,
	};
}

void MenuBase::TextStyle::ApplyTo(TextComponent& text) const {
	text.color = color;
	text.outlineWidth = outlineWidth;
	text.outlineColor = outlineColor;
	text.shadow = shadow;
	text.shadowOffset = shadowOffset;
	text.shadowColor = shadowColor;
	text.glowWidth = glowWidth;
	text.glowColor = glowColor;
	text.shine = shine;
	text.shineColor = shineColor;
	text.shineWidth = shineWidth;
	text.shineDuration = shineDuration;
	text.shineInterval = shineInterval;
	text.motion = motion;
	text.motionAmount = motionAmount;
	text.motionSpeed = motionSpeed;
	text.motionSpread = motionSpread;
}

//=============================================================================
// Inspector に出す・保存する値（派生クラスの Fields から、最初に呼ぶ）
//=============================================================================
void MenuBase::Fields(ComponentUI& ui) {
	ui.Label("子の Items の子が項目（有効な物だけ。上から順）。選んだ項目には、見本の Text（Style Name の名前の、一番上の直下の子）の飾りを写す");
	ui.Label("↑↓（Columns が 2 以上なら ←→ も）・W A S D・十字・スティックで選ぶ / Enter・Space・A で決める / Esc・Start・B で戻る。マウス：乗せて選ぶ・左クリックで決める・右クリックで戻る");
	ui.Field("Style Name", styleName_, Tip("選んだ項目の見本の Text の名前。見本は Text の Enabled を外して置く（画面には出ない）"));
	ui.Field("Columns", columns_, AtLeast(1.0f).Tip("横に並べる数。1：縦の並び（端から反対の端へ回る）／2 以上：格子（←→ で1つ、↑↓ で Columns 個とぶ。端で止まる）"));
	ui.Field("Use Mouse", useMouse_, Tip("マウスでも選ぶ・決める"));
	ui.Field("Hit Padding", hitPadding_, AtLeast(0.0f).Tip("マウスの当たりを、文字のかたまりより上下左右に広げる（ピクセル）。項目に Image があれば、その形で当たるので効かない"));
}

//=============================================================================
// 毎フレーム（開いている UI Screen の中なら、ゲームが止まっていても呼ばれる）
//=============================================================================
void MenuBase::Start() {
	isReady_ = false; // 最初の Update では入力を受けない
	OnStart();
}

void MenuBase::Update() {
	OnUpdate();
	const std::vector<Handle<Entity>> items = CollectItems();
	const Vector2 stick = {InputManager::GetLeftStickX(), InputManager::GetLeftStickY()};
	if (items.empty()) {
		previousStick_ = stick;
		return;
	}
	const int count = static_cast<int>(items.size());
	selected_ = std::clamp(selected_, 0, count - 1); // 項目が減ったとき

	// --- 入力を受けるか（最初の1フレーム・決めた後・幕の間・派生クラスが止めている間は受けない）---
	const bool canInput = isReady_ && !isDecided_ && !SceneTransition::IsBusy() && !IsInputBlocked();
	isReady_ = true;
	if (canInput) {
		const int hovered = useMouse_ ? FindItemUnderMouse(items) : -1;
		const bool isMouseMoved = InputManager::GetMouseDeltaX() != 0 || InputManager::GetMouseDeltaY() != 0;
		if (IsCancelTriggered() || (useMouse_ && InputManager::IsMouseButtonTriggered(1))) {
			// --- 戻る ---
			OnCancel();
		} else if (useMouse_ && hovered >= 0 && InputManager::IsMouseButtonTriggered(0)) {
			// --- マウスで決める（クリックした項目を選んでから）---
			selected_ = hovered;
			TryDecide(items, selected_);
		} else {
			// --- 選ぶ（キー・スティック。マウスは動かしたときだけ。止まったマウスがキーで選んだ所を取り戻さないように）---
			if (!MoveByInput(count, stick) && hovered >= 0 && isMouseMoved) {
				selected_ = hovered;
			}
			// --- 決める ---
			if (IsDecideTriggered()) {
				TryDecide(items, selected_);
			}
		}
	}
	previousStick_ = stick;

	// --- 見た目・選んだ項目が変わったら知らせる ---
	ApplyLook(items);
	if (selected_ != notifiedSelected_) {
		notifiedSelected_ = selected_;
		OnSelect(selected_);
	}
}

//=============================================================================
// 選ぶ
//=============================================================================
bool MenuBase::MoveByInput(int count, const Vector2& stick) {
	const bool up = IsUpTriggered() || IsStickTriggered(stick.y, previousStick_.y, 1.0f);
	const bool down = IsDownTriggered() || IsStickTriggered(stick.y, previousStick_.y, -1.0f);
	const bool left = IsLeftTriggered() || IsStickTriggered(stick.x, previousStick_.x, -1.0f);
	const bool right = IsRightTriggered() || IsStickTriggered(stick.x, previousStick_.x, 1.0f);
	const int before = selected_;

	if (columns_ <= 1) {
		// --- 縦の並び：端から反対の端へ回る（% は負の数だと負になるので、count を足してから割る）---
		if (up) {
			selected_ = (selected_ - 1 + count) % count;
		}
		if (down) {
			selected_ = (selected_ + 1) % count;
		}
		return selected_ != before;
	}
	// --- 格子：端で止まる（5列の右端から次の段へとぶと、どこにいるか分からなくなるので）---
	const int column = selected_ % columns_;
	if (left && column > 0) {
		--selected_;
	}
	if (right && column < columns_ - 1 && selected_ + 1 < count) {
		++selected_;
	}
	if (up && selected_ - columns_ >= 0) {
		selected_ -= columns_;
	}
	if (down && selected_ + columns_ < count) {
		selected_ += columns_;
	}
	return selected_ != before;
}

void MenuBase::Select(int index) { selected_ = (std::max)(index, 0); } // 多すぎる番号は、次の Update で項目の数に収める

//=============================================================================
// マウス
//=============================================================================
int MenuBase::FindItemUnderMouse(const std::vector<Handle<Entity>>& items) const {
	if (!InputManager::IsMouseInGame()) {
		return -1;
	}
	for (size_t i = 0; i < items.size(); ++i) {
		if (IsMouseOverItem(items[i])) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

bool MenuBase::IsMouseOverItem(Handle<Entity> item) const {
	// --- Image があれば、描いた形のとおりに（エンジンの関数）---
	if (EntityManager::Get<ImageComponent>(item) != nullptr) {
		return SpriteRenderSystem::IsMouseOver(item);
	}
	// --- Text：文字のかたまりの四角を、Hit Padding だけ広げる（置き方は Image と同じなので、同じ関数で角を出せる）---
	const TextComponent* text = EntityManager::Get<TextComponent>(item);
	const TransformComponent* itemTransform = EntityManager::Get<TransformComponent>(item);
	if (text == nullptr || itemTransform == nullptr || !text->enabled || !EntityManager::IsActiveInHierarchy(item)) {
		return false;
	}
	const Vector2 textSize = TextRenderSystem::CalcTextSize(item);
	const Vector2 size = {textSize.x + hitPadding_ * 2.0f, textSize.y + hitPadding_ * 2.0f}; // Pivot が真ん中（0.5, 0.5）なら、上下左右に同じだけ広がる
	const ScreenQuad quad = SpriteRenderSystem::CalcScreenQuad(itemTransform->worldMatrix, text->anchor, text->pivot, size);
	return SpriteRenderSystem::IsInsideQuad(InputManager::GetMousePositionInGame(), quad);
}

//=============================================================================
// 決める
//=============================================================================
void MenuBase::TryDecide(const std::vector<Handle<Entity>>& items, int index) {
	if (!CanDecide(index)) {
		OnDecideRejected(index);
		return;
	}
	const Entity* item = EntityManager::Get(items[index]);
	if (item != nullptr) {
		Decide(index, item->name);
	}
}

//=============================================================================
// 項目
//=============================================================================
std::vector<Handle<Entity>> MenuBase::CollectItems() const {
	std::vector<Handle<Entity>> children;
	std::vector<Handle<Entity>> items;
	if (const GameObject list = gameObject.FindChild("Items")) {
		EntityManager::GetChildren(list.handle, children);
	}
	// 有効な子だけ（SetActive(false) にした項目は選べない。クリア画面の最後のステージの「つぎへ」など）
	for (const Handle<Entity> child : children) {
		if (EntityManager::IsActiveInHierarchy(child)) {
			items.push_back(child);
		}
	}
	return items;
}

//=============================================================================
// 見た目
//=============================================================================
void MenuBase::ApplyLook(const std::vector<Handle<Entity>>& items) {
	// --- 項目の元の飾りを覚える（最初のフレームと、項目の数が変わったとき。外れた項目を戻すのに使う）---
	if (originalStyles_.size() != items.size()) {
		originalStyles_.clear();
		for (const Handle<Entity> item : items) {
			const TextComponent* text = EntityManager::Get<TextComponent>(item);
			originalStyles_.push_back(text != nullptr ? TextStyle::From(*text) : TextStyle{});
		}
	}
	// --- 見本の飾り（見本が無ければ、選んだ項目も元のまま）---
	const GameObject styleObject = gameObject.FindChild(styleName_);
	const TextComponent* styleText = styleObject ? styleObject.GetComponent<TextComponent>() : nullptr;
	for (size_t i = 0; i < items.size(); ++i) {
		TextComponent* text = EntityManager::Get<TextComponent>(items[i]);
		if (text == nullptr) {
			continue; // Text の無い項目（格子のマスなど）は、派生クラスが OnSelect で見せる
		}
		const bool isSelected = (static_cast<int>(i) == selected_);
		if (isSelected && styleText != nullptr) {
			TextStyle::From(*styleText).ApplyTo(*text);
		} else {
			originalStyles_[i].ApplyTo(*text);
		}
	}
}
```

### 解説

**`Start`・`Update` を `final` にして、派生クラスには `OnStart`・`OnUpdate` を書かせる**
- `final` は「派生クラスでこれ以上 override できない」印。もし派生クラスが `Update` を override できると、`MenuBase::Update()` を呼び忘れたとき、選ぶ・決めるが全部止まる（エラーにならず、黙って動かない）。入口を別の名前（`OnUpdate`）にしておけば、呼び忘れようがない。
- `Fields` だけは override のまま：派生クラスの `Fields` の最初で `MenuBase::Fields(ui);` を呼ぶ（Inspector に共通の値を出すため）。

**入力の順番（`Update`）**
```
戻る（Esc・B・右クリック） → 左クリック（乗っている項目で決める） → それ以外（キー・スティックで選ぶ → 決めるキー）
```
- 1フレームに1つしか起きないように `if / else if / else` でつないだ。戻ると決めるが同じフレームに起きると、閉じた画面で決めることになる。
- **マウスは動かしたときだけ選ぶ**：止まったマウスが項目の上にあると、キーで選んだ所が毎フレームマウスの所へ戻ってしまう。`GetMouseDeltaX/Y`（前のフレームから動いた量）が 0 でないときだけ、乗っている項目を選ぶ。

**最初の1フレームは入力を受けない（`isReady_`）**
- タイトルで Enter を押してステージ選択を開くと、その Enter はまだ「押した瞬間」のまま。画面の Script はそのフレームか次のフレームに動き始めるので、何もしないと、開いた途端に1つ目のマスを決めてしまう。`Start` で `isReady_ = false` にして、1回目の `Update` の終わりで `true` にする。

**縦の並びで端から回る：`(selected_ - 1 + count) % count`**
- C++ の `%` は、左が負の数だと答えも負になる（`-1 % 3 == -1`）。先に `count` を足して、0 以上にしてから割る（今の PauseMenu と同じ）。

**格子は端で止まる**
- 列の番号は `selected_ % columns_`。左は「列が 0 より大きいとき」、右は「列が最後でない・次の項目があるとき」だけ動く。上下は `columns_` 個とぶ（はみ出すなら動かない）。

**スティック：倒した瞬間だけ**
- 前のフレームの値（`previousStick_`）を覚えておき、「今は 0.5 より倒している・前は倒していなかった」ときだけ1つ動く。倒したままだと、毎フレーム動いて一瞬で端まで行くので。
- `IsStickTriggered(今, 前, 向き)` の向きは +1（右・上）か -1（左・下）。値に向きを掛けると、両方の向きを同じ式（`> 0.5`）で調べられる。

**マウスの当たり（`IsMouseOverItem`）**
- Text も Image も、画面への置き方（Anchor・Pivot・Transform）は同じ（エンジンの `TextRenderSystem.h` に書いてある）。だから **Image の当たりを作る関数（`CalcScreenQuad`）に、Text の大きさ（`CalcTextSize`）を渡せば、文字のかたまりの四角が出る**。
- その四角の中にマウスがあるかは、エンジンの `IsInsideQuad`（Image のボタンと同じ関数）。マウスの位置（`GetMousePositionInGame`）も四角の角も、同じ「画面の真ん中が 0、端が ±1」の座標なので、そのまま比べられる。
- 文字のかたまりぴったりだと狭いので、`Hit Padding`（ピクセル）だけ大きくする。Pivot が真ん中（0.5, 0.5。Text の初めの値）なら上下左右に同じだけ広がる。

**`ApplyLook` は、Text の無い項目を飛ばす**
- 項目に Text が無ければ、見本の飾りを写せないので何もしない（今回は全部 Text で作る）。

---

## パート C：`Source/Scripts/UI/PauseMenu.h / .cpp`（全部差し替え）

中身の大半は MenuBase へ移ったので、残るのは「何をするか」だけ。

### `PauseMenu.h`

```cpp
#pragma once
#include <string>

#include "Source/Scripts/UI/MenuBase.h"

/// <summary>
/// ポーズ画面。ポーズ画面（resources/ui/Pause.uiscreen）の一番上の Entity に付ける
/// <para>項目の名前で何をするか決める：Resume（つづける）/ Restart（やりなおす）/ Title（タイトルへ）。選ぶ・飾りは MenuBase</para>
/// <para>ポーズ中（ゲームが止まっている間）も、開いている画面の中の Script なので呼ばれる</para>
/// </summary>
class PauseMenu : public MenuBase {
public:
	void Fields(ComponentUI& ui) override;

protected:
	void Decide(int index, const std::string& itemName) override;
	void OnCancel() override; // 閉じて、ゲームに戻る

private:
	// ===== Inspector に出す・保存する値 =====
	std::string screenName_ = "Pause"; // この画面の名前（閉じるときに UIScreen::Close に渡す）
	std::string titleScene_ = "Title"; // タイトルのシーンの名前（resources/scenes/<名前>.scene.json）
};
```

### `PauseMenu.cpp`

```cpp
#include "PauseMenu.h"

SCRIPT(PauseMenu, "UI");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void PauseMenu::Fields(ComponentUI& ui) {
	MenuBase::Fields(ui);
	ui.Label("項目の名前で何をするか決める：Resume（つづける）/ Restart（やりなおす）/ Title（タイトルへ）。戻るキーでも閉じる");
	ui.Field("Screen Name", screenName_, Tip("この画面の名前（resources/ui/<名前>.uiscreen）。閉じるときに使う"));
	ui.Field("Title Scene", titleScene_, Tip("Title を選んだときに読むシーンの名前（resources/scenes の .scene.json の名前）"));
}

//=============================================================================
// 決める・戻る
//=============================================================================
void PauseMenu::Decide(int /*index*/, const std::string& itemName) {
	if (itemName == "Resume") {
		OnCancel();
	} else if (itemName == "Restart") {
		// ポーズのまま幕を閉じて、最初から（作り直すとポーズも画面も消える）
		isDecided_ = SceneTransition::ReloadScene();
	} else if (itemName == "Title") {
		// ポーズのまま幕を閉じて、タイトルへ（シーンが替わるとポーズも画面も消える）
		isDecided_ = SceneTransition::LoadScene(titleScene_);
	}
}

void PauseMenu::OnCancel() {
	UIScreen::Close(screenName_); // 消し始めたら、すぐゲームが動く
	isDecided_ = true;
}
```

- 今の PauseMenu.h には `#pragma once` が無かった（足した）。
- **Inspector の値は引き継がれる**：保存の名前（`Screen Name`・`Style Name`）は前と同じなので、Pause.uiscreen の値はそのまま読める。増えた `Columns`・`Use Mouse`・`Hit Padding`・`Title Scene` は、初めの値（1・付ける・12・Title）になる。
- 「Resume」は戻る（閉じる）と同じことなので、`OnCancel()` を呼ぶ。
- 「Title」は今まで `return;` だけだった。`SceneTransition::LoadScene(titleScene_)` にした。ポーズ中でも幕は止めない時間で動くので、Restart と同じく、そのままタイトルへ行く。Title シーン（パート G）を作るまでは、「シーンが無い」の警告が出て何も起きない。

**ここで一度ビルドして（パート F で MenuBase を入れてから）、Level01 で Esc → つづける・やりなおす・マウスで選ぶ を確かめる。**

---

## パート D：`Source/Scripts/UI/TitleMenu.h / .cpp`（新しいファイル）

### `TitleMenu.h`

```cpp
#pragma once
#include <string>

#include "Source/Scripts/UI/MenuBase.h"

/// <summary>
/// タイトル画面のメニュー。Title シーンの TitleUI（空の Entity）に付ける
/// <para>項目の名前で何をするか決める：Start（ステージ選択の画面を出す。シーンは替えない）/ Quit（ゲームを終わる）。選ぶ・飾りは MenuBase</para>
/// <para>子の Logo（Text）があれば、ゲームの名前（GameTitle.h の kGameTitle）を書く</para>
/// <para>ステージ選択の画面が出ている間は、タイトルの項目は動かない</para>
/// </summary>
class TitleMenu : public MenuBase {
public:
	void Fields(ComponentUI& ui) override;

protected:
	void OnStart() override;
	void Decide(int index, const std::string& itemName) override;
	bool IsInputBlocked() const override; // ステージ選択の画面が出ている間

private:
	// ゲームを終わる（エディタでは終わらない）
	static void QuitGame();

	// ===== Inspector に出す・保存する値 =====
	std::string stageSelectScreen_ = "StageSelect"; // Start で出す画面の名前（resources/ui/<名前>.uiscreen）
	std::string logoName_ = "Logo";                 // ゲームの名前を書く Text の名前（一番上の直下の子。無ければ書かない）
};
```

### `TitleMenu.cpp`

```cpp
#include "TitleMenu.h"

#include <MyEngine/Diagnostics/LogManager.h>
#include <MyEngine/Engine.h>

#include "Source/Scripts/GameTitle.h"

SCRIPT(TitleMenu, "UI");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void TitleMenu::Fields(ComponentUI& ui) {
	MenuBase::Fields(ui);
	ui.Label("項目の名前で何をするか決める：Start（ステージ選択の画面を出す）/ Quit（ゲームを終わる。エディタでは終わらない）");
	ui.Field("Stage Select Screen", stageSelectScreen_, Tip("Start で出す画面の名前（resources/ui の .uiscreen の名前）"));
	ui.Field("Logo Name", logoName_, Tip("ゲームの名前（GameTitle.h の kGameTitle）を書く Text の名前（一番上の直下の子）。無ければ書かない"));
}

//=============================================================================
// 最初
//=============================================================================
void TitleMenu::OnStart() {
	// --- ロゴにゲームの名前を書く（名前は GameTitle.h の1か所だけで変える）---
	if (const GameObject logo = gameObject.FindChild(logoName_)) {
		if (TextComponent* text = logo.GetComponent<TextComponent>()) {
			text->text = kGameTitle;
		}
	}
}

//=============================================================================
// 決める
//=============================================================================
void TitleMenu::Decide(int /*index*/, const std::string& itemName) {
	if (itemName == "Start") {
		// 画面を重ねるだけ（シーンは替えない。背景はそのまま）。ファイルが無ければ出ない
		if (!UIScreen::Open(stageSelectScreen_)) {
			LogManager::Warning("ステージ選択の画面を出せませんでした（resources/ui/" + stageSelectScreen_ + ".uiscreen が無い？）");
		}
	} else if (itemName == "Quit") {
		QuitGame();
		isDecided_ = true;
	}
}

bool TitleMenu::IsInputBlocked() const { return UIScreen::IsOpen(stageSelectScreen_); }

//=============================================================================
// 終わる
//=============================================================================
void TitleMenu::QuitGame() {
#ifdef USE_IMGUI
	// エディタ：ウィンドウを閉じるとエディタごと終わるので、知らせるだけ（Play を止めるのは手で）
	LogManager::Log("Quit：エディタでは終了しません（製品版ではここでゲームが終わる）");
#else
	// 製品版：自分のウィンドウに「閉じて」を置く → 次の ProcessMessage でエンジンの WM_CLOSE の処理が DestroyWindow → main のループが終わる
	// PostMessage は置くだけなので、この Script の途中でウィンドウが消えることはない
	PostMessage(Engine::GetWindowManager()->GetMainHWND(), WM_CLOSE, 0, 0);
#endif
}
```

### 解説

**はじめる：シーンを替えずに画面を重ねる**
- `UIScreen::Open("StageSelect")` は、`resources/ui/StageSelect.uiscreen` の中身を今のシーンに作るだけ。背景（ぼけた森）はそのまま。
- StageSelect.uiscreen は段階2 で作るので、**今は Console に警告が出るだけ**（落ちない）。
- `IsInputBlocked` で、ステージ選択が出ている間はタイトルの項目を動かさない。閉じたら（`IsOpen` が false になったら）また動く。

**おわる：`PostMessage(…, WM_CLOSE, …)`**
- エンジンに「ゲームを終わる」関数は無い。でも、ウィンドウの × を押したときと同じ `WM_CLOSE` を自分のウィンドウに送れば、エンジンの処理（`Win32Window.cpp` の `WM_CLOSE` → `DestroyWindow`）でウィンドウが消え、`main.cpp` の `while (Engine::ProcessMessage())` が false になってループが終わる。後片付け（`Engine::Finalize`）も普通に通る。
- `PostMessage` は「あとで処理してね」と置くだけ（`SendMessage` はその場で処理する）。Script の途中でウィンドウが消えると、その後の処理が消えたウィンドウを触るかもしれないので、置くだけの方を使う。
- **エディタ（`USE_IMGUI`）ではログだけ**：エディタのウィンドウを閉じると、エディタごと終わってしまう。
- ウィンドウは `Engine::GetWindowManager()->GetMainHWND()`（最初のウィンドウ）。

**ロゴの文字は `OnStart` で書く**
- `text->text = kGameTitle;`：Text の文字は `FixedString<256>` で、`std::string_view` をそのまま入れられる。
- Title シーンのロゴの Text に何を書いておいても、Play したらこの名前になる（エディタで見える文字は、置いた時の仮の文字）。

---

## パート E：`main.cpp`（全部差し替え）

```cpp
#include <string>

#include <MyEngine/Engine.h>
#include <MyEngine/String/ConvertString.h>

#include "Source/Scripts/GameTitle.h"

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	WindowConfig config;
	config.title = ConvertString(std::string(kGameTitle)); // タイトルバーもゲームの名前（UTF-8 → ウィンドウの文字）
	config.width = 1280;
	config.height = 720;
#ifdef USE_IMGUI
	config.isImGui = true;
#endif
	Engine::Initialize(config, "Title"); // 最初のシーン（resources/scenes/Title.scene.json）
	while (Engine::ProcessMessage()) {
		Engine::BeginFrame();
		Engine::GetWindowManager()->UpdateAll();
		Engine::GetWindowManager()->DrawAll();
		Engine::EndFrame();
	}
	Engine::Finalize();
	return 0;
}
```

- `ConvertString`（エンジンの `String/ConvertString.h`）は UTF-8 の `std::string` → ウィンドウ用の `std::wstring`。`kGameTitle` は `string_view` なので、一度 `std::string` にしてから渡す。
- **`"Title"` が効くのは製品版（Release）だけ**。エディタは前回最後に開いていたシーンから始まる（エンジンの `Engine::Initialize` の決まり）。だから、Title シーンを作る前に main.cpp を変えても、エディタは困らない。

---

## パート F：VS のプロジェクトに入れる

ソリューションエクスプローラーの AL4_01 プロジェクトで、フォルダを右クリック → **追加 → 既存の項目**（ファイルを先に作った場合）か **新しい項目**：

| フォルダ（フィルター） | 入れるファイル |
|---|---|
| Source/Scripts | `GameTitle.h` |
| Source/Scripts/UI | `MenuBase.h`・`MenuBase.cpp`・`TitleMenu.h`・`TitleMenu.cpp` |

- **.cpp を入れ忘れると**、リンクエラー（`MenuBase::…` が未解決）か、Add Component に Title Menu が出ない。

---

## パート G：エディタで Title シーンを作る

### G-1. Level01 を写す
1. Level01 を開く → **File → Save As…**（Ctrl+Shift+S）→ 名前 `Title`。
2. Hierarchy で、遊ぶための物を消す：**Player**（キューブ）・**Level**（LevelInfo）・**Tutorial**（説明の Text と Event）・**Goal**（扉。飾りに残してもよい）。
3. **Main Camera** の **Camera Follow** を外す（Remove Component。Target のキューブがいないので）。カメラの位置は、見せたい背景が入る所へ（Level01 のスタートの辺り）。

### G-2. 背景をぼかす（Depth Of Field）
Main Camera（Depth Of Field が付いている Entity）の Depth Of Field を：

| 値 | 入れる | 意味 |
|---|---|---|
| Focus | **Distance** | キューブがいないので、距離でピントを決める |
| Focus Distance | **2** | カメラのすぐ前にピント（＝足場も背景も全部その奥） |
| Focus Range | **0** | はっきり見える幅を無くす |
| Far Transition | **4** | すぐに一番ぼけまでいく |
| Near Transition | **0** | 手前はぼかさない（手前に物は無い） |
| Max Blur | 8.5（今のまま。もっとぼかすなら上げる。上げるほど重い） | |

→ 足場も背景も全部ぼけて、色と光だけが残る（作業用コピーでこの値の見た目を確かめた）。円のパーティクル・霧はそのまま動く。

### G-3. タイトルの文字（Hierarchy の ＋）

| 作る物 | 名前 | 親 | Component・値 |
|---|---|---|---|
| ＋ → Create Empty | **TitleUI** | （一番上） | Add Component → Gameplay → UI → **Title Menu** |
| ＋ → Text | **Logo** | TitleUI | Font Size 96・位置 (0, 150)・Outline 4（濃い青緑）・Glow 12（氷色 #BFEFFF・A 0.6）・Shine を付ける |
| ＋ → Create Empty | **Items** | TitleUI | — |
| ＋ → Text | **Start** | Items | 文字「はじめる」・Font Size 44・位置 (0, -60)・Outline 3 |
| ＋ → Text | **Quit** | Items | 文字「おわる」・Font Size 44・位置 (0, -140)・Outline 3 |
| ＋ → Text | **SelectedStyle** | TitleUI | 選んだ項目の見本（下）。**Text の Enabled を外す** |

- 名前（TitleUI 以外の Start・Quit・Items・Logo・SelectedStyle）はコードが探すので、そのとおりに（大文字・小文字も）。画面に出る文字は日本語で自由に。
- SelectedStyle のおすすめ：Color 白・Outline 3（氷色）・Glow 14（氷色・A 0.8）・Shine を付ける（Interval 1.2）・Motion **Bounce**（Amount 4・Speed 1.5・Spread 0.08）。選ぶと、文字が光ってぴょこぴょこ跳ねる。
- 選んでいない項目（Start・Quit）は Color の A を 0.75 くらいにすると、選んだ方が目立つ。
- 位置・大きさは見ながら直してよい（Text の Pivot は真ん中のままにする。マウスの当たりが上下左右に同じだけ広がる）。

### G-4. 保存
Ctrl+S。`resources/scenes/Title.scene.json` ができる。

---

## パート H：Pause.uiscreen（確かめるだけ）

Pause.uiscreen の Items には、もう **Resume・Restart・Title** の3つがある（今の作業中のファイル）。名前がこのとおりなら、何も変えなくてよい。Title の画面の文字は「タイトルへ」にしておくと分かりやすい。

---

## 確かめること（写した後）

| 何をする | こうなればよい |
|---|---|
| Level01 を Play → Esc | ポーズが出る。↑↓・マウスで選べる・選んだ項目が光る（今までどおり） |
| つづける / Esc / 右クリック | 閉じてゲームが動く |
| やりなおす | 幕が閉じて最初から |
| タイトルへ | 幕が閉じて Title シーン（ポーズは消える） |
| Title を Play | ロゴが「霧氷ジャンプ」になる。背景はぼけたまま。↑↓・マウスで選べる |
| はじめる | Console に「ステージ選択の画面を出せませんでした」（段階2 で画面ができるまで） |
| おわる（エディタ） | Console に「Quit：エディタでは終了しません」 |
| おわる（Release） | ゲームが閉じる |
| Release を起動 | タイトルバーが「霧氷ジャンプ」・最初が Title |

## 作業用コピーで確かめたこと（2026-10-26）
- このページのコードを AL4_01 の写し（サブモジュールのエンジン 2f9a621 込み）に入れて、**Debug・Release ともビルドが通った**（エラー 0。ゲームのファイルから警告 0。前からある C4865・C4005 だけ）。
- Release を仮の Title シーン（Level01 から作った物）で起動：ロゴに `kGameTitle` が書かれる・タイトルバーが「霧氷ジャンプ」・G-2 の値で背景が全部ぼける・ログにエラー無し。
- 一時的に `OnStart` で `QuitGame()` を呼んで Release を起動 → **ウィンドウが閉じて終了コード 0**（後片付けのログも最後まで出た）。
- 一時的に `OnStart` で `UIScreen::Open("Pause")` → 新しい PauseMenu（MenuBase の派生）で Pause.uiscreen が開いた。
- Debug（エディタ）が起動して、ログに警告・エラー無し。
- キー・マウスで実際に選ぶところは確かめていない（ユーザーの PC に本物の入力は送らないため）。写したら上の表で確かめてほしい。

## 困ったとき

| 症状 | 原因 |
|---|---|
| `MenuBase` が未解決（リンクエラー） | MenuBase.cpp をプロジェクトに入れていない（パート F） |
| `final と宣言された関数を override できません` | 派生クラスに `Update`・`Start` を書いた。`OnUpdate`・`OnStart` にする |
| ポーズで何も選べない | Items の名前が違う・項目が無効（SetActive が false）・最初の1フレームは受けない（2フレーム目から） |
| マウスが当たらない | Use Mouse が外れている・Text の Enabled が外れている・Game ビューの外 |
| ロゴが「Text」のまま | Logo の名前が違う（Logo Name と同じにする）・Title Menu が TitleUI に付いていない |
| タイトルへで何も起きない | Title シーンがまだ無い（Console に警告）・Title Scene の名前が違う |
