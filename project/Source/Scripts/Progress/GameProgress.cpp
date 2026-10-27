#include "GameProgress.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

#include <externals/nlohmann/json.hpp>

#include <MyEngine/Diagnostics/LogManager.h>

#include "Source/Scripts/Progress/StageCatalog.h"

namespace {
constexpr const char* kFilePath = "saves/Progress.json"; // 保存の場所（起動したフォルダから。resources と同じ所）
constexpr int kFileVersion = 1;                          // ファイルの形の版（形を変えたら上げる）
constexpr bool kAllStagesOpen = true;                    // 最初から全部のステージを遊べる（false：前のステージをクリアしたら開く）

/// <summary>
/// ステージ1つ分の記録
/// </summary>
struct StageRecord {
	bool isCleared = false;        // クリアした
	std::optional<float> bestTime; // 1位のタイム（秒。まだ無ければ空）
	int clearCount = 0;            // クリアした回数
	std::vector<int> crystals;     // 取った隠し結晶の Id（小さい順・同じ番号は1つ）
	int crystalTotal = 0;          // ステージに置いてある隠し結晶の数（そのステージを遊ぶと分かる。まだなら 0）
};

/// <summary>
/// 進み具合の中身（この .cpp の中だけ。ヘッダに nlohmann を出さないため）
/// </summary>
struct ProgressState {
	std::array<StageRecord, kStageCount> records;   // kStages と同じ並び
	nlohmann::json file = nlohmann::json::object(); // 読んだファイルの中身（一覧から消したステージの記録も、消さずに書き戻すため）
	bool isLoaded = false;                          // もうファイルを読んだ
};
ProgressState state;

// 初めて呼ばれた時に1回だけ、ファイルを読む（無い・壊れている → 何もクリアしていない扱い）
void LoadIfNeeded() {
	if (state.isLoaded) {
		return;
	}
	state.isLoaded = true;
	std::ifstream stream(kFilePath);
	if (!stream) {
		return; // まだ無い（初めての起動）
	}
	nlohmann::json json = nlohmann::json::parse(stream, nullptr, false); // false：壊れていても例外を投げない（is_discarded になる）
	if (json.is_discarded() || !json.is_object()) {
		LogManager::Warning(std::string(kFilePath) + " が読めないので、何もクリアしていない扱いにします");
		return;
	}
	state.file = std::move(json);
	// --- 一覧のステージごとに、シーンの名前で記録を探す ---
	const auto stages = state.file.find("stages");
	if (stages == state.file.end() || !stages->is_object()) {
		return;
	}
	for (size_t i = 0; i < kStageCount; ++i) {
		const auto found = stages->find(std::string(kStages[i].scene));
		if (found == stages->end() || !found->is_object()) {
			continue;
		}
		StageRecord& record = state.records[i];
		record.isCleared = found->value("cleared", false);
		if (const auto time = found->find("bestTime"); time != found->end() && time->is_number()) {
			record.bestTime = time->get<float>();
		}
		if (const auto clears = found->find("clears"); clears != found->end() && clears->is_number_integer()) {
			record.clearCount = clears->get<int>();
		}
		// 回数を数える前のファイル（clears が無い）でクリアしていたら、1回は数える（0回なのにクリア済み、にならないように）
		record.clearCount = (std::max)(record.clearCount, record.isCleared ? 1 : 0);
		// --- 隠し結晶：取った Id の並び（数でない物は飛ばす）と、置いてある数 ---
		if (const auto crystals = found->find("crystals"); crystals != found->end() && crystals->is_array()) {
			for (const nlohmann::json& id : *crystals) {
				if (id.is_number_integer()) {
					record.crystals.push_back(id.get<int>());
				}
			}
			std::ranges::sort(record.crystals);
			const auto [first, last] = std::ranges::unique(record.crystals); // 同じ番号が重なっていたら1つに
			record.crystals.erase(first, last);
		}
		if (const auto total = found->find("crystalTotal"); total != found->end() && total->is_number_integer()) {
			record.crystalTotal = (std::max)(total->get<int>(), 0);
		}
	}
}

// 今の記録をファイルに書く（一覧に無いステージの記録は、読んだまま残す）
void Save() {
	nlohmann::json& stages = state.file["stages"]; // 無ければ空のオブジェクトができる
	if (!stages.is_object()) {
		stages = nlohmann::json::object();
	}
	for (size_t i = 0; i < kStageCount; ++i) {
		const StageRecord& record = state.records[i];
		nlohmann::json entry = {
		    {"cleared", record.isCleared },
		    {"clears",  record.clearCount}
        };
		if (record.bestTime) {
			entry["bestTime"] = *record.bestTime;
		}
		entry["crystals"] = record.crystals; // std::vector<int> は、そのまま JSON の配列になる
		entry["crystalTotal"] = record.crystalTotal;
		stages[std::string(kStages[i].scene)] = std::move(entry);
	}
	state.file["version"] = kFileVersion;

	// --- フォルダが無ければ作って、書く ---
	std::error_code error;
	std::filesystem::create_directories(std::filesystem::path(kFilePath).parent_path(), error);
	std::ofstream stream(kFilePath);
	if (!stream) {
		LogManager::Warning(std::string(kFilePath) + " に書けませんでした");
		return;
	}
	stream << state.file.dump(4) << '\n';
}

// 番号が一覧の中か
bool IsValidIndex(size_t index) { return index < kStageCount; }
} // namespace

//=============================================================================
// 読む
//=============================================================================
bool GameProgress::IsCleared(size_t index) {
	LoadIfNeeded();
	return IsValidIndex(index) && state.records[index].isCleared;
}

bool GameProgress::IsUnlocked(size_t index) {
	if (!IsValidIndex(index)) {
		return false;
	}
	// 最初から全部のステージを遊べる（前のステージをクリアしたら開く形に戻すなら、kAllStagesOpen を false に）
	return kAllStagesOpen || index == 0 || IsCleared(index - 1);
}

std::optional<float> GameProgress::GetBestTime(size_t index) {
	LoadIfNeeded();
	return IsValidIndex(index) ? state.records[index].bestTime : std::nullopt;
}

int GameProgress::GetClearCount(size_t index) {
	LoadIfNeeded();
	return IsValidIndex(index) ? state.records[index].clearCount : 0;
}

//=============================================================================
// 書く
//=============================================================================
std::optional<float> GameProgress::RecordClear(size_t index, float time) {
	LoadIfNeeded();
	if (!IsValidIndex(index)) {
		return std::nullopt;
	}
	StageRecord& record = state.records[index];
	const std::optional<float> previousBest = record.bestTime;
	record.isCleared = true;
	++record.clearCount;
	if (!record.bestTime || time < *record.bestTime) {
		record.bestTime = time;
	}
	Save();
	return previousBest;
}

//=============================================================================
// 隠し結晶
//=============================================================================
bool GameProgress::IsCrystalCollected(size_t index, int id) {
	LoadIfNeeded();
	return IsValidIndex(index) && std::ranges::binary_search(state.records[index].crystals, id);
}

void GameProgress::RecordCrystals(size_t index, const std::vector<int>& ids) {
	LoadIfNeeded();
	if (!IsValidIndex(index)) {
		return;
	}
	bool isChanged = false;
	std::vector<int>& crystals = state.records[index].crystals;
	for (const int id : ids) {
		// --- 小さい順の並びを崩さないように、入る所を探して入れる（もうあれば何もしない）---
		const auto place = std::ranges::lower_bound(crystals, id);
		if (place != crystals.end() && *place == id) {
			continue;
		}
		crystals.insert(place, id);
		isChanged = true;
	}
	if (isChanged) {
		Save();
	}
}

int GameProgress::GetCrystalCount(size_t index) {
	LoadIfNeeded();
	if (!IsValidIndex(index)) {
		return 0;
	}
	const StageRecord& record = state.records[index];
	const int count = static_cast<int>(record.crystals.size());
	// 置いてある数が分かっていれば、それを越えない（結晶を減らした・Id を付け直したとき）
	return (record.crystalTotal > 0) ? (std::min)(count, record.crystalTotal) : count;
}

int GameProgress::GetCrystalTotal(size_t index) {
	LoadIfNeeded();
	return IsValidIndex(index) ? state.records[index].crystalTotal : 0;
}

void GameProgress::SetCrystalTotal(size_t index, int total) {
	LoadIfNeeded();
	if (!IsValidIndex(index) || state.records[index].crystalTotal == total) {
		return; // 変わっていなければ書かない（ステージを始めるたびに書かないように）
	}
	state.records[index].crystalTotal = (std::max)(total, 0);
	Save();
}

//=============================================================================
// 全部を変える
//=============================================================================
void GameProgress::UnlockAll() {
	LoadIfNeeded();
	for (StageRecord& record : state.records) {
		record.isCleared = true;
	}
	Save();
	LogManager::Log("GameProgress：全部のステージを開きました");
}

void GameProgress::ResetAll() {
	LoadIfNeeded();
	// --- 置いてある結晶の数は、記録ではなくステージの形なので残す（消すと、もう一度遊ぶまでステージ選択に数が出なくなる）---
	std::array<int, kStageCount> crystalTotals{};
	for (size_t i = 0; i < kStageCount; ++i) {
		crystalTotals[i] = state.records[i].crystalTotal;
	}
	state.records = {};
	for (size_t i = 0; i < kStageCount; ++i) {
		state.records[i].crystalTotal = crystalTotals[i];
	}
	state.file = nlohmann::json::object(); // 一覧に無いステージの記録も消す
	Save();
	LogManager::Log("GameProgress：記録を全部消しました");
}

//=============================================================================
// タイムの文字
//=============================================================================
std::string FormatTime(float seconds) {
	// 1/100 秒の整数にしてから分ける（float のまま割ると、59.999 が 1:00.00 になるなど、切り上がることがある）
	const long long centiseconds = static_cast<long long>(std::floor((std::max)(seconds, 0.0f) * 100.0f));
	const long long minutes = centiseconds / 6000;
	const long long secondsPart = (centiseconds / 100) % 60;
	const long long hundredths = centiseconds % 100;
	return std::format("{}:{:02}.{:02}", minutes, secondsPart, hundredths);
}