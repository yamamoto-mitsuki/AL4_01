#include "SettingItems.h"

#include <algorithm>
#include <format>
#include <optional>

#include <externals/magic_enum/magic_enum.hpp>

#include <MyEngine/Components/UI/TextComponent.h>
#include <MyEngine/ECS/EntityManager.h>

#include "Source/Scripts/Progress/GameSettings.h"

namespace {
// 全画面の項目の名前（uiscreen の Items の子の名前と同じにする）
// 音の大きさの項目は、つまみ（VolumeKind）の名前と同じ "Master"・"BGM"・"SE"
constexpr const char* kFullscreenItem = "Fullscreen";

// 音の大きさの項目なら、そのつまみ（違えば空）
std::optional<VolumeKind> FindVolumeKind(const std::string& itemName) { return magic_enum::enum_cast<VolumeKind>(itemName); }

// 音の大きさの値の文字（「<  8  >」。←→ で変えられると分かるように、両側に < > を付ける）
std::string FormatVolume(VolumeKind kind) { return std::format("<  {}  >", GameSettings::GetVolumeStep(kind)); }

// items の下の項目 itemName の Value に text を書く（無ければ何もしない）
void SetValueText(const GameObject& items, const std::string& itemName, const std::string& valueName, const std::string& text) {
	const GameObject item = items ? items.FindChild(itemName) : GameObject{};
	const GameObject value = item ? item.FindChild(valueName) : GameObject{};
	if (TextComponent* component = value ? value.GetComponent<TextComponent>() : nullptr) {
		component->text = text;
	}
}
} // namespace

namespace SettingItems {
//=============================================================================
// 値を変える
//=============================================================================
bool IsSettingItem(const std::string& itemName) { return FindVolumeKind(itemName).has_value() || itemName == kFullscreenItem; }

bool Adjust(const std::string& itemName, int direction) {
	// --- 音の大きさ：端で止まる ---
	if (const std::optional<VolumeKind> kind = FindVolumeKind(itemName)) {
		const int before = GameSettings::GetVolumeStep(*kind);
		const int after = std::clamp(before + direction, 0, GameSettings::kVolumeSteps);
		if (after == before) {
			return false;
		}
		GameSettings::SetVolumeStep(*kind, after);
		return true;
	}
	// --- 全画面：←→ のどちらでも切り替える（2つしか無いので）---
	if (itemName == kFullscreenItem) {
		GameSettings::SetFullscreen(!GameSettings::IsFullscreen());
		return true;
	}
	return false;
}

bool Decide(const std::string& itemName) {
	// --- 音の大きさ：1段上げる。一番上の次は 0 へ ---
	if (const std::optional<VolumeKind> kind = FindVolumeKind(itemName)) {
		const int next = (GameSettings::GetVolumeStep(*kind) + 1) % (GameSettings::kVolumeSteps + 1);
		GameSettings::SetVolumeStep(*kind, next);
		return true;
	}
	// --- 全画面：切り替える ---
	return Adjust(itemName, 1);
}

//=============================================================================
// 文字・見た目
//=============================================================================
void WriteValues(const GameObject& items, const ValueTexts& texts) {
	for (const VolumeKind kind : magic_enum::enum_values<VolumeKind>()) {
		SetValueText(items, std::string(magic_enum::enum_name(kind)), texts.valueName, FormatVolume(kind));
	}
	// Alt + Enter で全画面を切り替えたときも合うように、毎フレーム書いてよい（今のウィンドウを見る）
	SetValueText(items, kFullscreenItem, texts.valueName, GameSettings::IsFullscreen() ? texts.onText : texts.offText);
}

void MatchValueLooks(const std::vector<Handle<Entity>>& items, const std::string& valueName) {
	// MenuBase は項目の Text だけを変えるので、子の Value はここで写す（1フレーム遅れるが、見て分からない）
	for (const Handle<Entity> item : items) {
		const TextComponent* itemText = EntityManager::Get<TextComponent>(item);
		const GameObject value = GameObject{item}.FindChild(valueName);
		TextComponent* valueText = value ? value.GetComponent<TextComponent>() : nullptr;
		if (itemText == nullptr || valueText == nullptr) {
			continue;
		}
		valueText->color = itemText->color;
		valueText->glowWidth = itemText->glowWidth;
		valueText->glowColor = itemText->glowColor;
	}
}
} // namespace SettingItems
