#include "PauseMenu.h"

#include <algorithm>

#include <MyEngine/Components/UI/AlphaGroupComponent.h>
#include <MyEngine/Diagnostics/LogManager.h>

#include "Source/Scripts/Audio/GameAudio.h"
#include "Source/Scripts/Effects/GameRumble.h"

SCRIPT(PauseMenu, "UI");

namespace {
const std::string kItemsName = "Items"; // 項目の入れ物（MenuBase と同じ。設定の画面を出している間は消す）
} // namespace

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void PauseMenu::Fields(ComponentUI& ui) {
	MenuBase::Fields(ui);
	ui.Label("項目の名前で何をするか決める：Resume（つづける）/ Restart（やりなおす）/ Settings（設定の画面を重ねる）/ Title（タイトルへ）。戻るキーでも閉じる");
	ui.Field("Screen Name", screenName_, Tip("この画面の名前（resources/ui/<名前>.uiscreen）。閉じるときに使う"));
	ui.Field("Title Scene", titleScene_, Tip("Title を選んだときに読むシーンの名前（resources/scenes の .scene.json の名前）"));
	ui.Field("Settings Screen", settingsScreen_, Tip("Settings で重ねる画面の名前（resources/ui の .uiscreen の名前。SettingsMenu を付けた画面）"));
	ui.Field("Header Name", headerName_, Tip("設定の画面を出している間に消す見出しの Text の名前（一番上の直下の子）。項目の Items も一緒に消す"));
	ui.Field("Fade Time", fadeTime_, AtLeast(0.01f).Tip("設定の画面を出した・閉じたとき、見出しと項目が消える・戻るまでの秒数"));
	// --- 後ろのゲームの絵のぼかし ---
	ui.Separator("Blur");
	ui.Field("Blur Time", blurTime_, AtLeast(0.0f).Tip("開いた・閉じたとき、ぼける・戻るまでの秒数。UI Screen の Hide Duration くらいにする（消えきったら、途中でも戻しきる）"));
	ui.Field("Blur Focus Distance", blurTo_.focusDistance, AtLeast(0.0f).Tip("ぼけている時のピントの奥行き（m）。小さいほど、遊ぶ面まで全部ぼける"));
	ui.Field("Blur Focus Range", blurTo_.focusRange, AtLeast(0.0f).Tip("ぼけている時の、ぼけない幅（m）"));
	ui.Field("Blur Far Transition", blurTo_.farTransition, AtLeast(0.0f).Tip("ぼけている時の、一番ぼけるまでの奥行き（m）。小さいほど、すぐ奥が強くぼける"));
	ui.Field("Blur Near Transition", blurTo_.nearTransition, AtLeast(0.0f).Tip("ぼけている時の、手前のぼけ（m）。0 なら手前はぼかさない"));
	ui.Field("Blur Max", blurTo_.maxBlur, AtLeast(0.0f).Tip("ぼけている時の、ぼけの半径（ピクセル。エンジンの上限は 32）"));
}

//=============================================================================
// 開いたとき・毎フレーム
//=============================================================================
void PauseMenu::OnStart() {
	// 円で跳んだ直後にポーズすると、止めている間は数える人（IceCube）が動かず、震えっぱなしになるので止める
	GameRumble::Stop();
	// --- 後ろのゲームの絵をぼかし始める（シーンに Depth Of Field が無ければ、ぼかさない）---
	blur_.Begin(blurTo_, blurTime_);
	// ゲームの中の Text は、ぼけずにくっきり残るので、ぼかしと一緒に消す（この画面の中の Text は除く）
	gameTexts_.Capture();
}

void PauseMenu::OnDestroy() {
	blur_.Finish();
	gameTexts_.SetAlpha(1.0f);
}

void PauseMenu::OnUpdate() {
	// --- ぼかし・ゲームの中の Text を進める（ポーズ中はゲームの時間が止まっているので、止めない時間で）---
	const float deltaTime = Time::GetUnscaleDeltaTime();
	blur_.Update(deltaTime);
	const float textStep = (blurTime_ > 0.0f) ? deltaTime / blurTime_ : 1.0f;
	gameTextAlpha_ = isClosing_ ? (std::min)(gameTextAlpha_ + textStep, 1.0f) : (std::max)(gameTextAlpha_ - textStep, 0.0f);
	gameTexts_.SetAlpha(gameTextAlpha_);
	// --- 入力を止める：設定の画面が出ている間と、閉じた次のフレーム ---
	// 設定の画面を戻るキーで閉じたフレームに、こちらも同じキーで閉じてしまわないように、1フレーム余分に止める
	// （どちらの Script が先に呼ばれても、閉じたフレームとその次のフレームは止まる）
	const bool isSubOpen = UIScreen::IsOpen(settingsScreen_);
	isSubBlocking_ = isSubOpen || wasSubOpen_;
	wasSubOpen_ = isSubOpen;
	// --- 設定の画面が出ている間は、見出しと項目を消す・閉じたら戻す（止めない時間で、一定の速さで近づける）---
	// Alpha Group は UIScreen が画面の直下の子に付けて、画面を出す・消す動きでも書く。その動きとぶつからないように、
	// 書くのは、消えている・戻っている間だけ（濃さが 1 に戻りきったら、もう書かない）
	const float target = isSubOpen ? 0.0f : 1.0f;
	const float step = Time::GetUnscaleDeltaTime() / fadeTime_;
	const float alpha = (contentAlpha_ < target) ? (std::min)(contentAlpha_ + step, target) : (std::max)(contentAlpha_ - step, target);
	if (alpha < 1.0f || contentAlpha_ < 1.0f) {
		SetChildAlpha(headerName_, alpha);
		SetChildAlpha(kItemsName, alpha);
	}
	contentAlpha_ = alpha;
}

bool PauseMenu::IsInputBlocked() const { return isSubBlocking_; }

void PauseMenu::SetChildAlpha(const std::string& name, float alpha) const {
	const GameObject child = gameObject.FindChild(name);
	if (AlphaGroupComponent* group = child ? child.GetComponent<AlphaGroupComponent>() : nullptr) {
		group->alpha = alpha;
	}
}

//=============================================================================
// 決める・戻る
//=============================================================================
void PauseMenu::Decide(int /*index*/, const std::string& itemName) {
	if (itemName == "Resume") {
		Close();
	} else if (itemName == "Restart") {
		// ポーズのまま幕を閉じて、最初から（作り直すとポーズも画面も消える）
		isDecided_ = SceneTransition::ReloadScene();
		// BGM を低く・小さくする（幕が閉じきるまでに。次にレベルが始まったら LevelInfo が戻す）
		if (isDecided_) {
			GameAudio::DownBgm(SceneTransition::kDefaultFadeTime);
		}
	} else if (itemName == "Settings") {
		// 設定の画面を重ねるだけ（ポーズのまま。閉じたらこの画面に戻る）
		if (!UIScreen::Open(settingsScreen_)) {
			LogManager::Warning("設定の画面を出せませんでした（resources/ui/" + settingsScreen_ + ".uiscreen が無い？）");
		}
	} else if (itemName == "Title") {
		// ポーズのまま幕を閉じて、タイトルへ（シーンが替わるとポーズも画面も消える）
		isDecided_ = SceneTransition::LoadScene(titleScene_);
	}
}

void PauseMenu::OnCancel() {
	GameAudio::PlaySe(Se::UiBack);
	Close();
}

void PauseMenu::Close() {
	blur_.Restore(blurTime_);     // ぼかしと Text を戻し始める（画面が消える間も、この Script は呼ばれる）
	isClosing_ = true;
	UIScreen::Close(screenName_); // 消し始めたら、すぐゲームが動く
	isDecided_ = true;
}
