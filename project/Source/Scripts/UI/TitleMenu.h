#pragma once
#include <string>

#include "Source/Scripts/UI/MenuBase.h"

/// <summary>
/// タイトル画面のメニュー。Title シーンの TitleUI（空の Entity）に付ける
/// <para>項目の名前で何をするか決める：Start（ステージ選択の画面を出す。シーンは替えない）/ Quit（ゲームを終わる）。選ぶ・飾りは MenuBase</para>
/// <para>子の Logo（Text）があれば、ゲームの名前（GameTitle.h の kGameTitle）を書く</para>
/// <para>ステージ選択の画面が出ている間は、タイトルの項目は動かず、タイトルの文字（この Entity と子）は静かに消える（Alpha Group）</para>
/// <para>エディタだけ：F1 で全部のステージを開く・F2 で記録を全部消す</para>
/// </summary>
class TitleMenu : public MenuBase {
public:
	void Fields(ComponentUI& ui) override;

protected:
	void OnStart() override;
	void OnUpdate() override; // タイトルの文字を消す・戻す。デバッグのキー
	void Decide(int index, const std::string& itemName) override;
	bool IsInputBlocked() const override; // ステージ選択の画面が出ている間

private:
	// ゲームを終わる（エディタでは終わらない）
	static void QuitGame();

	// ===== Inspector に出す・保存する値 =====
	std::string stageSelectScreen_ = "StageSelect"; // Start で出す画面の名前（resources/ui/<名前>.uiscreen）
	std::string logoName_ = "Logo";                 // ゲームの名前を書く Text の名前（一番上の直下の子。無ければ書かない）
	float fadeTime_ = 0.5f;                         // ステージ選択を出した・閉じたとき、タイトルの文字が消える・戻るまでの秒数
};