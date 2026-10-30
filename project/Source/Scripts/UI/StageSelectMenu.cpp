#include "StageSelectMenu.h"

#include <algorithm>
#include <format>
#include <optional>

#include <MyEngine/Components/UI/ImageComponent.h>
#include <MyEngine/Diagnostics/LogManager.h>

#include "Source/Scripts/Audio/GameAudio.h"
#include "Source/Scripts/Progress/GameProgress.h"
#include "Source/Scripts/Progress/OutGameState.h"
#include "Source/Scripts/Progress/StageCatalog.h"

SCRIPT(StageSelectMenu, "UI");

namespace {
// 結晶の図形を入れる入れ物（一番上の直下の子。画面の出し入れのフェードが、入れ物ごと図形に掛かるように、この下に作る）
const std::string kCrystalIconsName = "CrystalIcons";

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
	ui.Field("Center Last Row", centerLastRow_, Tip("数が足りない最後の段を、真ん中に寄せる（Columns が 7 でステージが 13 なら、下の 6 個を半マス右へ）"));
	ui.Field("Cleared Color", clearedColor_, Tip("クリアしたマスの色"));
	ui.Field("Open Color", openColor_, Tip("遊べるが、まだクリアしていないマスの色"));
	ui.Field("Locked Color", lockedColor_, Tip("まだ遊べないマスの色"));
	ui.Field("Stage Name", stageNameName_, Tip("選んでいるステージの名前を書く Text の名前（一番上の直下の子）"));
	ui.Field("Message", messageName_, Tip("記録の代わりに言葉（Locked・Coming soon・ヒント）を書く Text の名前（一番上の直下の子）"));
	// --- 記録の1行 ---
	ui.Separator("Record");
	ui.Field("Best Label Name", bestLabelName_, Tip("「Best」を書く Text の名前（一番上の直下の子。Pivot X 1 で右寄せにして、数の左に置く）"));
	ui.Field("Best Value Name", bestValueName_, Tip("1位のタイムを書く Text の名前（Pivot X 0 で左寄せ。色文字：色はその Text の色）"));
	ui.Field("Clears Label Name", clearsLabelName_, Tip("「Clears」を書く Text の名前（右寄せ）"));
	ui.Field("Clears Value Name", clearsValueName_, Tip("クリアした回数を書く Text の名前（左寄せ。色文字）"));
	ui.Field("Crystal Template", crystalTemplateName_, Tip("隠し結晶の図形の見本の Image の名前（一番上の直下の子）。Image の Enabled は外して置く。この位置が1つ目の図形"));
	ui.Field("Crystal Spacing", crystalSpacing_, Tip("結晶の図形と図形の間（ピクセル。右へ並ぶ）"));
	ui.ColorField("Crystal On Color", crystalOnColor_);
	ui.ColorField("Crystal Off Color", crystalOffColor_);
	ui.Label("結晶の図形：置いてある数だけ並べ、取った数だけ左から On の色（まだの物は Off の色）。一度遊んで、結晶のあるステージだけ出る");
	ui.Field("Best Label", bestLabel_, Tip("タイムの前に付ける言葉"));
	ui.Field("Clears Label", clearsLabel_, Tip("クリアした回数の前に付ける言葉（タイムの横に出す）"));
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
	CreateCrystalIcons();
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
		// 数が足りない最後の段は、真ん中に寄せる（Center Last Row。上 7・下 6 なら、下の段を半マス右へ）
		const int rowStart = row * columns;
		const int countInRow = (std::min)(columns, static_cast<int>(kStageCount) - rowStart);
		const float rowShift = centerLastRow_ ? cellSpacing_.x * static_cast<float>(columns - countInRow) * 0.5f : 0.0f;
		GameObject cell = GameObject::Create(std::format("Stage{:02}", i + 1), items);
		cell.GetTransform().translation = {origin.x + cellSpacing_.x * column + rowShift, origin.y - cellSpacing_.y * row, origin.z};
		cell.AddComponent(text); // 付くのは次のフレームの頭（MenuBase は、付いてから元の飾りを覚える）
	}
}

void StageSelectMenu::CreateCrystalIcons() {
	const GameObject holder = gameObject.FindChild(kCrystalIconsName);
	const GameObject iconTemplate = gameObject.FindChild(crystalTemplateName_);
	const ImageComponent* templateImage = iconTemplate ? iconTemplate.GetComponent<ImageComponent>() : nullptr;
	if (!holder || templateImage == nullptr) {
		return; // 図形を出さない画面（入れ物か見本が無い）
	}
	// --- 作る数：一番多く置いてあるステージの数（選ぶたびに作り直さず、出す数だけ変える）---
	int maxTotal = 0;
	for (size_t i = 0; i < kStageCount; ++i) {
		maxTotal = (std::max)(maxTotal, GameProgress::GetCrystalTotal(i));
	}
	const Vector3 origin = iconTemplate.GetTransform().translation; // 1つ目の図形の位置
	for (int i = 0; i < maxTotal; ++i) {
		// 見本の Image を写す（大きさ・絵は見本のまま。出すかどうかと色は UpdateCrystalIcons が毎フレーム決める）
		ImageComponent image = *templateImage;
		image.enabled = false;
		GameObject icon = GameObject::Create(std::format("Crystal{}", i + 1), holder);
		icon.GetTransform().translation = {origin.x + crystalSpacing_ * static_cast<float>(i), origin.y, origin.z};
		icon.AddComponent(image); // 付くのは次のフレームの頭
		crystalIcons_.push_back(icon);
	}
}

//=============================================================================
// 毎フレーム
//=============================================================================
void StageSelectMenu::OnUpdate() {
	UpdateCrystalIcons();
	if (hintTimer_ <= 0.0f) {
		return;
	}
	// --- Locked Hint を出し終えたら、いつもの言葉に戻す（止めない時間で数える）---
	hintTimer_ -= Time::GetUnscaleDeltaTime();
	if (hintTimer_ <= 0.0f) {
		OnSelect(GetSelected());
	}
}

void StageSelectMenu::UpdateCrystalIcons() const {
	// 左から、置いてある数だけ出す。取った数までは On の色、残りは Off の色
	for (size_t i = 0; i < crystalIcons_.size(); ++i) {
		ImageComponent* image = crystalIcons_[i].GetComponent<ImageComponent>();
		if (image == nullptr) {
			continue; // まだ付いていない（作った次のフレームまで）
		}
		const int number = static_cast<int>(i);
		image->enabled = (number < crystalShown_);
		image->color = (number < crystalCollected_) ? crystalOnColor_ : crystalOffColor_;
	}
}

//=============================================================================
// 選ぶ・決める・戻る
//=============================================================================
void StageSelectMenu::OnSelect(int index) {
	hintTimer_ = 0.0f;
	// --- Back：ステージの名前と記録は消しておく（どのステージも選んでいないので）---
	if (IsBackIndex(index)) {
		SetText(stageNameName_, "");
		ShowMessage("");
		return;
	}
	const size_t stage = static_cast<size_t>(index);
	// --- 名前 ---
	SetText(stageNameName_, std::string(kStages[stage].title));
	// --- 記録（遊べない・シーンが無いときは、その言葉）---
	if (!GameProgress::IsUnlocked(stage)) {
		ShowMessage(lockedText_);
	} else if (!HasStageScene(stage)) {
		ShowMessage(comingText_);
	} else {
		ShowRecord(stage);
	}
}

//=============================================================================
// 記録の1行
//=============================================================================
void StageSelectMenu::ShowRecord(size_t stage) {
	// 「Best 0:32.45   Clears 3   ◆◆◇」。言葉は白、数は色文字（色は uiscreen の Text の色）
	SetText(messageName_, "");
	const std::optional<float> best = GameProgress::GetBestTime(stage);
	SetText(bestLabelName_, bestLabel_);
	SetText(bestValueName_, best ? FormatTime(*best) : noRecordText_);
	SetText(clearsLabelName_, clearsLabel_);
	SetText(clearsValueName_, std::format("{}", GameProgress::GetClearCount(stage)));
	// --- 結晶の図形：置いてある数が分かっている（一度遊んだ・結晶のある）ステージだけ ---
	crystalShown_ = (std::min)(GameProgress::GetCrystalTotal(stage), static_cast<int>(crystalIcons_.size()));
	crystalCollected_ = GameProgress::GetCrystalCount(stage);
}

void StageSelectMenu::HideRecord() {
	SetText(bestLabelName_, "");
	SetText(bestValueName_, "");
	SetText(clearsLabelName_, "");
	SetText(clearsValueName_, "");
	crystalShown_ = 0;
	crystalCollected_ = 0;
}

void StageSelectMenu::ShowMessage(const std::string& text) {
	HideRecord();
	SetText(messageName_, text);
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
		ShowMessage(lockedHint_);
		hintTimer_ = hintTime_;
	}
}

void StageSelectMenu::Decide(int index, const std::string& /*itemName*/) {
	// --- Back：戻るキーと同じ（閉じてタイトルへ）---
	if (IsBackIndex(index)) {
		Close();
		return;
	}
	const size_t stage = static_cast<size_t>(index);
	gOutGame.lastStage = stage;
	isDecided_ = SceneTransition::LoadScene(std::string(kStages[stage].scene));
}

void StageSelectMenu::OnCancel() {
	GameAudio::PlaySe(Se::UiBack);
	Close();
}

void StageSelectMenu::Close() {
	UIScreen::Close(screenName_); // 閉じると、タイトルの項目がまた動く
	isDecided_ = true;
}

//=============================================================================
// 項目（マスの後ろに Back）
//=============================================================================
std::vector<Handle<Entity>> StageSelectMenu::CollectItems() const {
	std::vector<Handle<Entity>> items = MenuBase::CollectItems(); // マス（Items の子。一覧の数だけ）
	// Back は Items の外に置く（Items の中だと、コードで作ったマスより前に並んで、番号がずれるので）
	if (HasFooterItem()) {
		items.push_back(gameObject.FindChild(backName_).handle);
	}
	return items;
}

bool StageSelectMenu::HasFooterItem() const {
	// Back は格子に入れない（入れると 09 の段の続きに数えられて、09〜の段で ↓ を押しても行けない。→ で行ってしまう）
	// 格子の外の下の項目にすると、一番下の段のどこからでも ↓ で行けて、↑ で元のマスへ戻る（MenuBase::MoveByInput）
	const GameObject back = gameObject.FindChild(backName_);
	return back && back.IsActiveInHierarchy();
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