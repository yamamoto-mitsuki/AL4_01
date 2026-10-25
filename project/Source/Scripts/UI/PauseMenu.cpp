#include "PauseMenu.h"

#include <vector>

#include <MyEngine/Components/TextComponent.h>
#include <MyEngine/ECS/EntityManager.h>
#include <MyEngine/Input/InputManager.h>

SCRIPT(PauseMenu, "UI");

namespace {
// 項目の並び（子の Items の子。Hierarchy の上から順）
std::vector<Handle<Entity>> CollectItems(const GameObject& owner) {
	std::vector<Handle<Entity>> items;
	if (const GameObject list = owner.FindChild("Items")) {
		EntityManager::GetChildren(list.handle, items);
	}
	return items;
}

// 上へ・下へ・決める・閉じるのキー（キーボードとパッドのどちらでも）
bool IsUpTriggered() { return InputManager::IsKeyTriggered(DIK_UP) || InputManager::IsKeyTriggered(DIK_W) || InputManager::IsButtonTriggered(GamepadButton::DpadUp); }
bool IsDownTriggered() { return InputManager::IsKeyTriggered(DIK_DOWN) || InputManager::IsKeyTriggered(DIK_S) || InputManager::IsButtonTriggered(GamepadButton::DpadDown); }
bool IsDecideTriggered() { return InputManager::IsKeyTriggered(DIK_RETURN) || InputManager::IsKeyTriggered(DIK_SPACE) || InputManager::IsButtonTriggered(GamepadButton::A); }
bool IsCloseTriggered() { return InputManager::IsKeyTriggered(DIK_ESCAPE) || InputManager::IsButtonTriggered(GamepadButton::Start) || InputManager::IsButtonTriggered(GamepadButton::B); }
} // namespace

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void PauseMenu::Fields(ComponentUI& ui) {
	ui.Label("子の Items の子の Text が項目（上から順）。名前で何をするか決める：Resume（つづける）/ Restart（やりなおす）");
	ui.Label("↑↓・W S・十字で選ぶ / Enter・Space・A で決める / Esc・Start・B で閉じる");
	ui.Field("Screen Name", screenName_, Tip("この画面の名前（resources/ui/<名前>.uiscreen）。閉じるときに使う"));
	ui.ColorField("Normal Color", normalColor_);
	ui.ColorField("Selected Color", selectedColor_);
	ui.Field("Selected Scale", selectedScale_, AtLeast(0.0f).Tip("選んでいる項目の大きさ（項目の Scale は 1 のままにしておく）"));
}

//=============================================================================
// 毎フレーム（ゲームが止まっていても呼ばれる）
//=============================================================================
void PauseMenu::Update() {
	const std::vector<Handle<Entity>> items = CollectItems(gameObject);
	if (items.empty() || isDecided_) {
		return;
	}
	const int count = static_cast<int>(items.size());
	// --- 閉じる ---
	if (IsCloseTriggered()) {
		UIScreen::Close(screenName_);
		isDecided_ = true;
		return;
	}
	// --- 選ぶ（端から反対の端へ回る。% は負の数だと負になるので、count を足してから割る）---
	if (IsUpTriggered()) {
		selected_ = (selected_ - 1 + count) % count;
	}
	if (IsDownTriggered()) {
		selected_ = (selected_ + 1) % count;
	}
	selected_ %= count; // 項目が減ったとき
	ApplyLook();
	// --- 決める ---
	if (IsDecideTriggered()) {
		const Entity* item = EntityManager::Get(items[selected_]);
		if (item != nullptr) {
			Decide(item->name);
		}
	}
}

//=============================================================================
// 見た目
//=============================================================================
void PauseMenu::ApplyLook() const {
	const std::vector<Handle<Entity>> items = CollectItems(gameObject);
	for (int i = 0; i < static_cast<int>(items.size()); ++i) {
		const bool isSelected = (i == selected_);
		if (TextComponent* text = EntityManager::Get<TextComponent>(items[i])) {
			text->color = isSelected ? selectedColor_ : normalColor_;
		}
		if (TransformComponent* itemTransform = EntityManager::Get<TransformComponent>(items[i])) {
			const float scale = isSelected ? selectedScale_ : 1.0f;
			itemTransform->scale = {scale, scale, 1.0f};
		}
	}
}

//=============================================================================
// 決める
//=============================================================================
void PauseMenu::Decide(const std::string& itemName) {
	if (itemName == "Resume") {
		UIScreen::Close(screenName_); // 消し始めたら、すぐゲームが動く
		isDecided_ = true;
	} else if (itemName == "Restart") {
		// ポーズのまま幕を閉じて、最初から（作り直すとポーズも画面も消える）
		isDecided_ = SceneTransition::ReloadScene();
	}
}