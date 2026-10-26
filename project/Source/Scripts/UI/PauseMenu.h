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