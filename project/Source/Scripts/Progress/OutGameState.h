#pragma once
#include <cstddef>

/// <summary>
/// 画面（シーン）をまたいで覚えておく値。シーンが替わると Entity は全部消えるので、シーンの外に置く
/// <para>保存はしない（ゲームを起動し直したら初めの値から）。保存する物は GameProgress</para>
/// </summary>
struct OutGameState {
	size_t lastStage = 0;     // 最後に遊んだ・次に遊ぶステージ（kStages の番号。ステージ選択で、最初にここを選ぶ）
	bool isRetrying = false;  // やられた・R で読み直す途中（LevelInfo::Retry が立て、読み直した LevelInfo::Start が下ろす。ステージの始まりの演出を出さない）
};

// ゲームに1つ（inline 変数：どの .cpp から読んでも同じ1つを指す）
inline OutGameState gOutGame;