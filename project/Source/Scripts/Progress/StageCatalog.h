#pragma once
#include <array>
#include <cstddef>
#include <string_view>

/// <summary>
/// ステージ1つ分（一覧の1行）
/// </summary>
struct StageEntry {
	std::string_view scene; // シーンの名前（resources/scenes/<scene>.scene.json）
	std::string_view title; // ステージ選択の下の行に出す名前
};

// ステージの一覧（遊ぶ順）。ステージを増やす・減らす・並べ替えるときは、ここの行を足す・消す・入れ替える
// 数は書かない（std::to_array が、並べた行の数から std::array の大きさを決める）。ステージ選択のマスも、この数だけ作られる
// 進み具合はシーンの名前で保存するので、並べ替えても記録はずれない（GameProgress）
inline constexpr auto kStages = std::to_array<StageEntry>({
    {"Level01", "Stage 01"},
    {"Level02", "Stage 02"},
    {"Level03", "Stage 03"},
    {"Level04", "Stage 04"},
    {"Level05", "Stage 05"},
    {"Level06", "Stage 06"},
    {"Level07", "Stage 07"},
    {"Level08", "Stage 08"},
    {"Level09", "Stage 09"},
    {"Level10", "Stage 10"},
});

// ステージの数
inline constexpr size_t kStageCount = kStages.size();