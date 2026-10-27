#include "MenuBase.h"

#include <cmath>
#include <algorithm>

#include <MyEngine/ECS/EntityManager.h>
#include <MyEngine/Input/InputManager.h>
#include <MyEngine/Systems/SpriteRenderSystem.h>
#include <MyEngine/Systems/TextRenderSystem.h>

#include "Source/Scripts/Audio/GameAudio.h"

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
	ui.Field("Selected Offset", selectedOffset_, Tip("選んだ項目を、この分だけゆっくりずらす（ピクセル。右・上が＋）。(0, 0) ならずらさない。例：(16, 0) で少し右へ"));
	ui.Field("Slide Time", slideTime_, AtLeast(0.01f).Tip("ずれる・戻るのにかかる秒数の目安。小さいほどすばやい"));
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
			const int before = selected_;
			if (!MoveByInput(count, stick) && hovered >= 0 && isMouseMoved) {
				selected_ = hovered;
			}
			// 選んだ項目が変わったときだけ鳴らす（端で止まった・同じ項目の上でマウスを動かしただけなら鳴らさない）
			if (selected_ != before) {
				GameAudio::PlaySe(Se::UiMove);
			}
			// --- 縦の並びの ←→：選んでいる項目の値を変える（格子では ←→ が選ぶのに使われるので呼ばない）---
			if (columns_ <= 1) {
				if (const int direction = ReadHorizontal(stick); direction != 0) {
					OnAdjust(selected_, direction);
				}
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
	ApplySlide(items);
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
	// --- 格子の下の項目（HasFooterItem。ステージ選択の Back）：最後の1つは格子に入れない ---
	const bool hasFooter = HasFooterItem() && count >= 2;
	const int gridCount = hasFooter ? count - 1 : count; // 格子に並んでいる項目の数
	const int footer = count - 1;                         // 格子の下の項目の番号（hasFooter のときだけ使う）
	if (selected_ < gridCount) {
		footerReturn_ = selected_; // 最後にいたマス（マウスで下の項目へ移ったときも、↑ でここへ戻る）
	}
	if (hasFooter && selected_ == footer) {
		// 格子の下の項目を選んでいる：↑ で、来たマスへ戻る（←→・↓ は何もしない）
		if (up) {
			selected_ = std::clamp(footerReturn_, 0, gridCount - 1);
		}
		return selected_ != before;
	}
	// --- 格子：端で止まる（5列の右端から次の段へとぶと、どこにいるか分からなくなるので）---
	const int column = selected_ % columns_;
	if (left && column > 0) {
		--selected_;
	}
	if (right && column < columns_ - 1 && selected_ + 1 < gridCount) {
		++selected_;
	}
	if (up && selected_ - columns_ >= 0) {
		selected_ -= columns_;
	}
	if (down) {
		const int lastRow = (gridCount - 1) / columns_;
		if (selected_ + columns_ < gridCount) {
			selected_ += columns_;
		} else if (selected_ / columns_ < lastRow) {
			// 真下に項目が無い（下の段が短い）：下の段の一番後ろへ
			selected_ = gridCount - 1;
		} else if (hasFooter) {
			// 一番下の段：格子の下の項目へ（どの列からでも行ける。↑ で戻るマスは footerReturn_ が覚えている）
			selected_ = footer;
		}
	}
	return selected_ != before;
}

int MenuBase::ReadHorizontal(const Vector2& stick) const {
	if (IsLeftTriggered() || IsStickTriggered(stick.x, previousStick_.x, -1.0f)) {
		return -1;
	}
	if (IsRightTriggered() || IsStickTriggered(stick.x, previousStick_.x, 1.0f)) {
		return 1;
	}
	return 0;
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
		GameAudio::PlaySe(Se::UiBack); // 決められない（まだ遊べないマスなど）：戻るの音で「入れない」と知らせる
		OnDecideRejected(index);
		return;
	}
	GameAudio::PlaySe(Se::UiDecide);
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
	// --- 項目の数が変わったら、覚えた飾りを忘れる ---
	if (originalStyles_.size() != items.size()) {
		originalStyles_.assign(items.size(), std::nullopt);
	}
	// --- 項目の元の飾りを覚える（Text が付いた最初のフレームに。外れた項目を戻すのに使う）---
	// コードで作った項目（ステージ選択のマス）は、Text が付くのが次のフレームなので、付くまで待つ
	for (size_t i = 0; i < items.size(); ++i) {
		const TextComponent* text = EntityManager::Get<TextComponent>(items[i]);
		if (!originalStyles_[i] && text != nullptr) {
			originalStyles_[i] = TextStyle::From(*text);
		}
	}
	// --- 見本の飾り（見本が無ければ、選んだ項目も元のまま）---
	const GameObject styleObject = gameObject.FindChild(styleName_);
	const TextComponent* styleText = styleObject ? styleObject.GetComponent<TextComponent>() : nullptr;
	for (size_t i = 0; i < items.size(); ++i) {
		TextComponent* text = EntityManager::Get<TextComponent>(items[i]);
		if (text == nullptr || !originalStyles_[i]) {
			continue; // Text の無い（まだ付いていない）項目は何もしない
		}
		const bool isSelected = (static_cast<int>(i) == selected_);
		if (isSelected && styleText != nullptr) {
			TextStyle::From(*styleText).ApplyTo(*text);
		} else {
			originalStyles_[i]->ApplyTo(*text);
		}
	}
}

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