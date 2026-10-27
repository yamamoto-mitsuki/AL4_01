#include "TitleMenu.h"

#include <algorithm>

#include <MyEngine/Components/AlphaGroupComponent.h>
#include <MyEngine/Diagnostics/LogManager.h>
#include <MyEngine/Engine.h>
#include <MyEngine/Input/InputManager.h>

#include "Source/Scripts/Audio/GameAudio.h"
#include "Source/Scripts/GameTitle.h"
#include "Source/Scripts/Progress/GameProgress.h"

SCRIPT(TitleMenu, "UI");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void TitleMenu::Fields(ComponentUI& ui) {
	MenuBase::Fields(ui);
	ui.Label("項目の名前で何をするか決める：Start（ステージ選択の画面を出す）/ Settings（設定の画面を出す）/ Quit（ゲームを終わる。エディタでは終わらない）");
	ui.Field("Stage Select Screen", stageSelectScreen_, Tip("Start で出す画面の名前（resources/ui の .uiscreen の名前）"));
	ui.Field("Settings Screen", settingsScreen_, Tip("Settings で出す画面の名前（resources/ui の .uiscreen の名前）"));
	ui.Field("Logo Name", logoName_, Tip("ゲームの名前（GameTitle.h の kGameTitle）を書く Text の名前（一番上の直下の子）。無ければ書かない"));
	ui.Field("Fade Time", fadeTime_, AtLeast(0.01f).Tip("ステージ選択・設定を出した・閉じたとき、タイトルの文字（この Entity と子）が消える・戻るまでの秒数"));
	ui.Label("エディタだけ：F1 で全部のステージを開く・F2 で記録を全部消す（ステージ選択を開く前に押す）");
}

//=============================================================================
// 最初・毎フレーム
//=============================================================================
void TitleMenu::OnStart() {
	// --- BGM：鳴っていなければ鳴らす（起動したとき）。ゴールで小さくしていたら戻す（クリア画面から戻ったとき）---
	GameAudio::StartBgm();
	GameAudio::MuteWind(); // ポーズのまま風の中からタイトルへ戻ったとき、風の音が残らないように
	// --- ロゴにゲームの名前を書く（名前は GameTitle.h の1か所だけで変える）---
	if (const GameObject logo = gameObject.FindChild(logoName_)) {
		if (TextComponent* text = logo.GetComponent<TextComponent>()) {
			text->text = kGameTitle;
		}
	}
	// --- タイトルの文字をまとめて消すための Alpha Group（無ければ付ける。付くのは次のフレーム）---
	if (GetComponent<AlphaGroupComponent>() == nullptr) {
		gameObject.AddComponent(AlphaGroupComponent{});
	}
}

void TitleMenu::OnUpdate() {
	// --- ステージ選択・設定が出ている間は消す・閉じたら戻す（止めない時間で、一定の速さで近づける）---
	if (AlphaGroupComponent* group = GetComponent<AlphaGroupComponent>()) {
		const float target = IsSubScreenOpen() ? 0.0f : 1.0f;
		const float step = Time::GetUnscaleDeltaTime() / fadeTime_;
		group->alpha = (group->alpha < target) ? (std::min)(group->alpha + step, target) : (std::max)(group->alpha - step, target);
	}
#ifdef USE_IMGUI
	// --- デバッグ：進み具合を変える（ステージ選択は開くたびにマスを作るので、開く前に押す）---
	if (InputManager::IsKeyTriggered(DIK_F1)) {
		GameProgress::UnlockAll();
	}
	if (InputManager::IsKeyTriggered(DIK_F2)) {
		GameProgress::ResetAll();
	}
#endif
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
	} else if (itemName == "Settings") {
		// ステージ選択と同じく、画面を重ねるだけ
		if (!UIScreen::Open(settingsScreen_)) {
			LogManager::Warning("設定の画面を出せませんでした（resources/ui/" + settingsScreen_ + ".uiscreen が無い？）");
		}
	} else if (itemName == "Quit") {
		QuitGame();
		isDecided_ = true;
	}
}

bool TitleMenu::IsInputBlocked() const { return IsSubScreenOpen(); }

bool TitleMenu::IsSubScreenOpen() const { return UIScreen::IsOpen(stageSelectScreen_) || UIScreen::IsOpen(settingsScreen_); }

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