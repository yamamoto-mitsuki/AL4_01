#include "StageSelectMenu.h"

#include <algorithm>
#include <format>
#include <optional>

#include <MyEngine/Diagnostics/LogManager.h>

#include "Source/Scripts/Progress/GameProgress.h"
#include "Source/Scripts/Progress/OutGameState.h"
#include "Source/Scripts/Progress/StageCatalog.h"

SCRIPT(StageSelectMenu, "UI");

namespace {
// そのステージのシーンがあるか（一覧に書いたが、まだ作っていないステージは入れない）
bool HasStageScene(size_t index) { return index < kStageCount && SceneManager::HasScene(std::string(kStages[index].scene)); }
} // namespace

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void StageSelectMenu::Fields(ComponentUI& ui) {
	MenuBase::Fields(ui);
	ui.Label("マスは、ステージの一覧（StageCatalog.h）の数だけ、Cell Template を写して Items の下に作る（並べ方は Columns と Cell Spacing）");
	ui.Field("Screen Name", screenName_, Tip("この画面の名前（resources/ui/<名前>.uiscreen）。閉じるときに使う"));
	ui.Field("Cell Template", templateName_, Tip("マスの見本の Text の名前（一番上の直下の子）。Text の Enabled は外して置く。この位置が1つ目（左上）のマスになる"));
	ui.Field("Back Name", backName_, Tip("タイトルへ戻る項目の Text の名前（一番上の直下の子。Items の中には置かない）。マスの一番下の段で ↓ を押すと選べる。無ければ戻るキーだけ"));
	ui.Field("Cell Spacing", cellSpacing_, Tip("マスとマスの間（ピクセル）。x：右へ、y：下へ"));
	ui.Field("Cleared Color", clearedColor_, Tip("クリアしたマスの色"));
	ui.Field("Open Color", openColor_, Tip("遊べるが、まだクリアしていないマスの色"));
	ui.Field("Locked Color", lockedColor_, Tip("まだ遊べないマスの色"));
	ui.Field("Stage Name", stageNameName_, Tip("選んでいるステージの名前を書く Text の名前（一番上の直下の子）"));
	ui.Field("Best Time", bestTimeName_, Tip("選んでいるステージの1位のタイムを書く Text の名前（一番上の直下の子）"));
	ui.Field("Best Label", bestLabel_, Tip("タイムの前に付ける言葉"));
	ui.Field("No Record Text", noRecordText_, Tip("まだクリアしていないときに、タイムの所に出す文字"));
	ui.Field("Locked Text", lockedText_, Tip("まだ遊べないときに、タイムの代わりに出す言葉"));
	ui.Field("Coming Text", comingText_, Tip("シーンがまだ無いときに、タイムの代わりに出す言葉"));
	ui.Field("Locked Hint", lockedHint_, Tip("まだ遊べないマスを決めたときに、しばらく出す言葉"));
	ui.Field("Hint Time", hintTime_, AtLeast(0.0f).Tip("Locked Hint を出しておく秒数"));
}

//=============================================================================
// 最初
//=============================================================================
void StageSelectMenu::OnStart() {
	CreateCells();
	// --- 最初に選ぶマス：最後に遊んだステージ。まだ遊べなければ、遊べる中で一番後ろ ---
	size_t first = (gOutGame.lastStage < kStageCount) ? gOutGame.lastStage : 0;
	while (first > 0 && !GameProgress::IsUnlocked(first)) {
		--first;
	}
	Select(static_cast<int>(first));
}

void StageSelectMenu::CreateCells() {
	const GameObject items = gameObject.FindChild("Items");
	const GameObject cellTemplate = gameObject.FindChild(templateName_);
	const TextComponent* templateText = cellTemplate ? cellTemplate.GetComponent<TextComponent>() : nullptr;
	if (!items || templateText == nullptr) {
		LogManager::Warning("ステージ選択：子の Items か、Cell Template（Text）がありません");
		return;
	}
	const Vector3 origin = cellTemplate.GetTransform().translation; // 1つ目（左上）のマスの位置
	const int columns = (std::max)(GetColumns(), 1);

	for (size_t i = 0; i < kStageCount; ++i) {
		// --- 見本の Text を写して、文字と色だけ変える（大きさ・フォント・飾りは見本のまま）---
		TextComponent text = *templateText;
		text.enabled = true;
		text.text = std::format("{:02}", i + 1);
		if (GameProgress::IsCleared(i)) {
			text.color = clearedColor_;
		} else if (GameProgress::IsUnlocked(i) && HasStageScene(i)) {
			text.color = openColor_;
		} else {
			text.color = lockedColor_;
		}
		// --- 格子に並べる（左から右、端まで行ったら次の段。段は下へ）---
		const int column = static_cast<int>(i) % columns;
		const int row = static_cast<int>(i) / columns;
		GameObject cell = GameObject::Create(std::format("Stage{:02}", i + 1), items);
		cell.GetTransform().translation = {origin.x + cellSpacing_.x * column, origin.y - cellSpacing_.y * row, origin.z};
		cell.AddComponent(text); // 付くのは次のフレームの頭（MenuBase は、付いてから元の飾りを覚える）
	}
}

//=============================================================================
// 毎フレーム
//=============================================================================
void StageSelectMenu::OnUpdate() {
	if (hintTimer_ <= 0.0f) {
		return;
	}
	// --- Locked Hint を出し終えたら、いつもの言葉に戻す（止めない時間で数える）---
	hintTimer_ -= Time::GetUnscaleDeltaTime();
	if (hintTimer_ <= 0.0f) {
		OnSelect(GetSelected());
	}
}

//=============================================================================
// 選ぶ・決める・戻る
//=============================================================================
void StageSelectMenu::OnSelect(int index) {
	hintTimer_ = 0.0f;
	// --- Back：ステージの2行は消しておく（どのステージも選んでいないので）---
	if (IsBackIndex(index)) {
		SetText(stageNameName_, "");
		SetText(bestTimeName_, "");
		return;
	}
	const size_t stage = static_cast<size_t>(index);
	// --- 名前 ---
	SetText(stageNameName_, std::string(kStages[stage].title));
	// --- タイム（遊べない・シーンが無いときは、その言葉）---
	if (!GameProgress::IsUnlocked(stage)) {
		SetText(bestTimeName_, lockedText_);
	} else if (!HasStageScene(stage)) {
		SetText(bestTimeName_, comingText_);
	} else {
		const std::optional<float> best = GameProgress::GetBestTime(stage);
		SetText(bestTimeName_, std::format("{}  {}", bestLabel_, best ? FormatTime(*best) : noRecordText_));
	}
}

bool StageSelectMenu::CanDecide(int index) const {
	if (IsBackIndex(index)) {
		return true; // Back はいつでも
	}
	const size_t stage = static_cast<size_t>(index);
	return GameProgress::IsUnlocked(stage) && HasStageScene(stage);
}

void StageSelectMenu::OnDecideRejected(int index) {
	// まだ遊べないマス：しばらく、開け方を出す（シーンが無いマスは、もう Coming Text を出している）
	if (!GameProgress::IsUnlocked(static_cast<size_t>(index))) {
		SetText(bestTimeName_, lockedHint_);
		hintTimer_ = hintTime_;
	}
}

void StageSelectMenu::Decide(int index, const std::string& /*itemName*/) {
	// --- Back：戻るキーと同じ（閉じてタイトルへ）---
	if (IsBackIndex(index)) {
		OnCancel();
		return;
	}
	const size_t stage = static_cast<size_t>(index);
	gOutGame.lastStage = stage;
	isDecided_ = SceneTransition::LoadScene(std::string(kStages[stage].scene));
}

void StageSelectMenu::OnCancel() {
	UIScreen::Close(screenName_); // 閉じると、タイトルの項目がまた動く
	isDecided_ = true;
}

//=============================================================================
// 項目（マスの後ろに Back）
//=============================================================================
std::vector<Handle<Entity>> StageSelectMenu::CollectItems() const {
	std::vector<Handle<Entity>> items = MenuBase::CollectItems(); // マス（Items の子。一覧の数だけ）
	// Back は Items の外に置く（Items の中だと、コードで作ったマスより前に並んで、番号がずれるので）
	if (const GameObject back = gameObject.FindChild(backName_); back && back.IsActiveInHierarchy()) {
		items.push_back(back.handle);
	}
	return items;
}

bool StageSelectMenu::IsBackIndex(int index) { return static_cast<size_t>(index) >= kStageCount; }

//=============================================================================
// 文字
//=============================================================================
void StageSelectMenu::SetText(const std::string& name, const std::string& text) const {
	if (const GameObject target = gameObject.FindChild(name)) {
		if (TextComponent* component = target.GetComponent<TextComponent>()) {
			component->text = text;
		}
	}
}