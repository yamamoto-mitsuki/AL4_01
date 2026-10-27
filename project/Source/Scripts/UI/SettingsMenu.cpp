#include "SettingsMenu.h"

#include <algorithm>

#include <MyEngine/ECS/EntityManager.h>

#include "Source/Scripts/Audio/GameAudio.h"
#include "Source/Scripts/Progress/GameProgress.h"
#include "Source/Scripts/Progress/OutGameState.h"

SCRIPT(SettingsMenu, "UI");

namespace {
// 項目の名前（uiscreen の Items の子の名前と同じにする）
// 音の大きさ・全画面の項目は SettingItems が名前で見分ける
constexpr const char* kResetItem = "Reset"; // 記録を消す
constexpr const char* kBackItem = "Back";   // 閉じる
} // namespace

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void SettingsMenu::Fields(ComponentUI& ui) {
	MenuBase::Fields(ui);
	ui.Label("項目の名前で何をするか決める：Master・SE・BGM（全体・効果音・音楽の大きさ。←→）/ Fullscreen（←→・決める）/ Reset（2回決めると記録を消す）/ Back（閉じる）");
	ui.Label("値は、項目の直下の子の Value（Text）に書く");
	ui.Field("Screen Name", screenName_, Tip("この画面の名前（resources/ui/<名前>.uiscreen）。閉じるときに使う"));
	ui.Field("Value Name", valueTexts_.valueName, Tip("値を書く Text の名前（項目の直下の子）。色・光は項目の Text に合わせる"));
	ui.Field("On Text", valueTexts_.onText, Tip("全画面のときの値"));
	ui.Field("Off Text", valueTexts_.offText, Tip("全画面でないときの値"));
	ui.Field("Reset Text", resetText_, Tip("Reset の項目の言葉"));
	ui.Field("Reset Warning Name", resetWarningName_, Tip("Reset を1回決めたときに、確かめる言葉を書く Text の名前（一番上の直下の子）。色・大きさは、その Text の方で決める"));
	ui.Field("Reset Warning Text", resetWarningText_, Tip("Reset を1回決めたときの言葉（もう一度決めたら消す。ほかの項目へ移ったら、やめて消す）"));
	ui.Field("Reset Erasing Text", resetErasingText_, Tip("2回目を決めて記録を消したときに、確かめる言葉の Text に出す言葉。後ろに「.」「..」「...」が付いて動く"));
	ui.Field("Reset Erasing Time", resetErasingTime_, AtLeast(0.0f).Tip("Reset Erasing Text を出しておく秒数"));
	ui.Field("Dot Interval", dotInterval_, AtLeast(0.05f).Tip("「.」が1つ増えるまでの秒数（「...」の次は「.」に戻る）"));
}

//=============================================================================
// 最初・毎フレーム
//=============================================================================
void SettingsMenu::OnStart() { RefreshTexts(); }

void SettingsMenu::OnUpdate() {
	// --- Value の飾りを、項目の Text に合わせる（選んだ項目は値も同じ色で光る）---
	SettingItems::MatchValueLooks(CollectItems(), valueTexts_.valueName);
	// --- 「消去中です...」：点を動かし、出し終えたら消す（止めない時間で数える）---
	if (resetErasingTimer_ > 0.0f) {
		resetErasingTimer_ = (std::max)(resetErasingTimer_ - Time::GetUnscaleDeltaTime(), 0.0f);
		RefreshTexts();
	}
	// --- Alt + Enter で全画面を切り替えたときも、値を合わせる ---
	SettingItems::WriteValues(gameObject.FindChild("Items"), valueTexts_);
}

//=============================================================================
// 選ぶ・決める・変える・戻る
//=============================================================================
bool SettingsMenu::IsInputBlocked() const { return resetErasingTimer_ > 0.0f; }

void SettingsMenu::OnSelect(int /*index*/) {
	// Reset を1回決めた後に、ほかの項目へ移った：確かめるのをやめる（うっかり消さないように）
	if (isResetArmed_) {
		isResetArmed_ = false;
		RefreshTexts();
	}
}

void SettingsMenu::OnAdjust(int index, int direction) {
	const Entity* item = EntityManager::Get(CollectItems()[index]); // index は MenuBase が項目の数に収めてから渡す
	if (item != nullptr && SettingItems::Adjust(item->name, direction)) {
		GameAudio::PlaySe(Se::UiMove); // 鳴らした後に音量が変わるので、効果音の大きさを変えたときは、新しい大きさで聞こえる
		RefreshTexts();
	}
}

void SettingsMenu::Decide(int /*index*/, const std::string& itemName) {
	if (SettingItems::Decide(itemName)) {
		// 音の大きさ・全画面（値は下の RefreshTexts で書く）
	} else if (itemName == kResetItem) {
		if (!isResetArmed_) {
			// --- 1回目：確かめる言葉を出すだけ ---
			isResetArmed_ = true;
			resetErasingTimer_ = 0.0f;
		} else {
			// --- 2回目：記録を消す（設定は消さない）。ステージ選択は 01 から。「消去中です...」をしばらく出す ---
			GameProgress::ResetAll();
			gOutGame.lastStage = 0;
			isResetArmed_ = false;
			resetErasingTimer_ = resetErasingTime_;
		}
	} else if (itemName == kBackItem) {
		Close();
		return;
	}
	RefreshTexts();
}

void SettingsMenu::OnCancel() {
	GameAudio::PlaySe(Se::UiBack);
	Close();
}

void SettingsMenu::Close() {
	UIScreen::Close(screenName_); // 閉じると、下の画面（タイトル・ポーズ）の項目がまた動く
	isDecided_ = true;
}

//=============================================================================
// 文字
//=============================================================================
void SettingsMenu::RefreshTexts() const {
	SettingItems::WriteValues(gameObject.FindChild("Items"), valueTexts_);
	// Reset の項目はいつも同じ言葉。下の赤い Text に「確かめる言葉」→「消去中です...」を出す
	SetItemText(kResetItem, resetText_);
	const GameObject warning = gameObject.FindChild(resetWarningName_);
	TextComponent* component = warning ? warning.GetComponent<TextComponent>() : nullptr;
	if (component == nullptr) {
		return;
	}
	if (isResetArmed_) {
		component->text = resetWarningText_;
	} else if (resetErasingTimer_ > 0.0f) {
		// 点の数：出してからの秒数を Dot Interval で区切って 1・2・3・1・2・3…
		const float elapsed = resetErasingTime_ - resetErasingTimer_;
		const int dots = static_cast<int>(elapsed / (std::max)(dotInterval_, 0.05f)) % 3 + 1;
		component->text = resetErasingText_ + std::string(static_cast<size_t>(dots), '.');
	} else {
		component->text = std::string();
	}
}

void SettingsMenu::SetItemText(const std::string& name, const std::string& text) const {
	const GameObject items = gameObject.FindChild("Items");
	if (const GameObject item = items ? items.FindChild(name) : GameObject{}) {
		if (TextComponent* component = item.GetComponent<TextComponent>()) {
			component->text = text;
		}
	}
}
