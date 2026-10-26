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