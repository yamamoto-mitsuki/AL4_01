#pragma once
#include <string>
#include <vector>

#include <MyEngine/Math/Vector2.h>
#include <MyEngine/Math/Vector4.h>

#include "Source/Scripts/UI/MenuBase.h"

/// <summary>
/// ステージ選択の画面。resources/ui/StageSelect.uiscreen の一番上の Entity に付ける（タイトルの上に重ねる。シーンは替えない）
/// <para>マスは、ステージの一覧（StageCatalog.h の kStages）の数だけ、見本の Text（Cell Template）を写して子の Items の下に作る。
/// 一覧に行を足せば、マスも増える</para>
/// <para>マスの後ろに、タイトルへ戻る項目（一番上の直下の子の Back。Text）を1つ足す。マスの一番下の段で ↓ を押すと Back へ行く</para>
/// <para>下に、選んでいるステージの名前（Stage Name）と、記録の1行を出す：Best ＋ 1位のタイム・Clears ＋ クリアした回数（言葉と数は別の Text。
/// 数の色は uiscreen の Text の色）・隠し結晶の図形（Crystal Template の Image を、置いてある数だけ並べる。取った物は濃い色、まだの物は薄い色）</para>
/// <para>遊べない・シーンが無い・Back のときは、記録の1行を消して、Message の Text に言葉を出す</para>
/// <para>遊べるのは、1つ前をクリアしたステージだけ。決めたら、そのシーンへ（幕）。Back か戻るキーで閉じる（タイトルへ戻る）</para>
/// </summary>
class StageSelectMenu : public MenuBase {
public:
	void Fields(ComponentUI& ui) override;

protected:
	void OnStart() override;  // マスを作って、最初に選ぶマスを決める
	void OnUpdate() override; // 結晶の図形の色を合わせる・入れないマスを決めたときの言葉を、しばらくしたら戻す
	void Decide(int index, const std::string& itemName) override;
	void OnCancel() override;
	bool CanDecide(int index) const override;
	void OnDecideRejected(int index) override;
	void OnSelect(int index) override;
	std::vector<Handle<Entity>> CollectItems() const override; // マス（Items の子）の後ろに Back を足す
	bool HasFooterItem() const override;                       // Back があれば、マスの格子の外（下）の項目にする

private:
	// マスを作る（見本の Text を写して、文字・色・位置だけ変える）
	void CreateCells();
	// 結晶の図形を作る（見本の Image を写して、一番多く置いてあるステージの数だけ横に並べる。出す数と色は UpdateCrystalIcons）
	void CreateCrystalIcons();
	// 結晶の図形を、選んでいるステージの数だけ出し、取った物を濃い色にする（作った図形の Image が付くのは次のフレームなので、毎フレーム合わせる）
	void UpdateCrystalIcons() const;
	// 記録の1行（Best・Clears の言葉と数・結晶の図形）を出す / 消す
	void ShowRecord(size_t stage);
	void HideRecord();
	// 記録の1行を消して、Message に言葉を出す
	void ShowMessage(const std::string& text);
	// 子の name の Text に文字を書く（無ければ何もしない）
	void SetText(const std::string& name, const std::string& text) const;
	// 閉じてタイトルへ戻る（Back を決めたときは、決めた音だけにしたいので、戻るの音は鳴らさない）
	void Close();
	// index の項目が Back か（マスは一覧の数だけあるので、それより後ろは Back）
	static bool IsBackIndex(int index);

	// ===== Inspector に出す・保存する値 =====
	std::string screenName_ = "StageSelect";              // この画面の名前（閉じるときに UIScreen::Close に渡す）
	std::string templateName_ = "CellTemplate";           // マスの見本の Text（一番上の直下の子。Text の Enabled を外して置く。この位置が1つ目のマス）
	std::string backName_ = "Back";                       // タイトルへ戻る項目の Text（一番上の直下の子。無ければ戻るキーだけ）
	Vector2 cellSpacing_ = {120.0f, 96.0f};               // マスとマスの間（ピクセル。横・縦。2段目は下へ）
	bool centerLastRow_ = true;                           // 数が足りない最後の段を、真ん中に寄せる
	Vector4 clearedColor_ = {1.0f, 1.0f, 1.0f, 0.68f};    // クリアしたマスの色（選んだマスより少し薄く。選んだマスは SelectedStyle の色で光る）
	Vector4 openColor_ = {1.0f, 1.0f, 1.0f, 0.42f};       // 遊べるがまだクリアしていないマスの色
	Vector4 lockedColor_ = {1.0f, 1.0f, 1.0f, 0.15f};     // まだ遊べないマスの色
	std::string stageNameName_ = "StageName";             // ステージの名前を書く Text（一番上の直下の子）
	std::string messageName_ = "Message";                 // 記録の代わりに言葉（Locked・Coming soon・ヒント）を書く Text（一番上の直下の子）
	// --- 記録の1行（どれも一番上の直下の子）---
	std::string bestLabelName_ = "BestLabel";             // 「Best」を書く Text
	std::string bestValueName_ = "BestValue";             // 1位のタイムを書く Text（色文字。色は uiscreen の Text の色）
	std::string clearsLabelName_ = "ClearsLabel";         // 「Clears」を書く Text
	std::string clearsValueName_ = "ClearsValue";         // クリアした回数を書く Text（色文字）
	std::string crystalTemplateName_ = "CrystalTemplate"; // 結晶の図形の見本の Image（Image の Enabled を外して置く。この位置が1つ目の図形）
	float crystalSpacing_ = 34.0f;                        // 結晶の図形と図形の間（ピクセル。右へ）
	Vector4 crystalOnColor_ = {0.82f, 0.66f, 1.0f, 1.0f}; // 取った結晶の図形の色
	Vector4 crystalOffColor_ = {1.0f, 1.0f, 1.0f, 0.2f};  // まだ取っていない結晶の図形の色
	std::string bestLabel_ = "Best";                      // タイムの前に付ける言葉
	std::string clearsLabel_ = "Clears";                  // クリアした回数の前に付ける言葉（タイムの横に出す）
	std::string noRecordText_ = "--:--.--";               // まだクリアしていないときのタイム
	std::string lockedText_ = "Locked";                   // まだ遊べないときに、タイムの代わりに出す言葉
	std::string comingText_ = "Coming soon";              // シーンがまだ無いときに、タイムの代わりに出す言葉
	std::string lockedHint_ = "Clear the previous stage"; // まだ遊べないマスを決めたときに、しばらく出す言葉
	float hintTime_ = 1.5f;                               // 上の言葉を出しておく秒数

	// ===== 実行中だけの値 =====
	float hintTimer_ = 0.0f;               // Locked Hint を出しておく残り秒（0 なら出していない）
	std::vector<GameObject> crystalIcons_; // 作った結晶の図形（左から）
	int crystalShown_ = 0;                 // 出す結晶の図形の数（選んでいるステージに置いてある数。記録を出さないときは 0）
	int crystalCollected_ = 0;             // そのうち濃い色にする数（取った数。左から）
};