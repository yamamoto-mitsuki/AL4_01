#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

#include <MyEngine/Math/Vector3.h>

/// <summary>
/// 取った中間地点（Checkpoint）。やられてシーンを読み直すと Entity は全部作り直されるので、シーンの外に置く
/// <para>流れ：中間地点に触れる（Checkpoint が書く）→ やられた・R（LevelInfo::Retry が isRespawning を立てて読み直す）
/// → 読み直したシーンの最初のフレームで、IceCube・Checkpoint・LevelInfo の Start が IsRespawning を見て、中間地点から始める</para>
/// <para>隠し結晶：中間地点に触れた時に、それまでに取った結晶を crystals に覚える（LevelInfo::KeepCrystals）。復活したら、その結晶は取った扱いのまま
/// （LevelInfo が持ち直し、SecretCrystal は消えた姿で始まる）。保存（GameProgress）はクリアした時だけ</para>
/// <para>isRespawning は「読み直した最初のフレーム」だけ true（LevelInfo の最初の Update で下ろす）。
/// なので、ポーズの Restart・ステージ選択・次のステージなど、ほかの読み方で入ったときは、LevelInfo::Start が全部消す（最初から）</para>
/// <para>保存はしない（ゲームを起動し直したら無い）</para>
/// </summary>
struct CheckpointState {
	bool hasCheckpoint = false;                  // 中間地点を取った
	size_t stage = 0;                            // 取ったステージ（kStages の番号。LevelInfo::GetStageIndex）
	int order = 0;                               // 取った中間地点の順番（Checkpoint の Order。小さい順番の物を後から取っても戻らない）
	Vector3 localPosition = {0.0f, 0.0f, 0.0f};  // 取った中間地点の Transform の位置（読み直した後、どの中間地点か見分ける）
	Vector3 spawnPosition = {0.0f, 0.0f, 0.0f};  // 復活する位置（ワールド）
	float playTime = 0.0f;                       // やられた時の遊んだ時間（復活したら、続きから数える）
	bool isRespawning = false;                   // 中間地点から始める読み直しの途中（読み直した最初のフレームだけ true）
	std::vector<int> crystals;                   // 中間地点を取った時までに取っていた隠し結晶の Id（復活したら取った扱いのまま。保存はクリアした時）

	// stage のシーンを、中間地点から始めるところか（IceCube・Checkpoint・LevelInfo の Start から）
	bool IsRespawning(size_t stageIndex) const { return isRespawning && hasCheckpoint && stage == stageIndex; }
	// stage のシーンを中間地点から始めるところで、id の隠し結晶を中間地点までに取っていたか（SecretCrystal の Start から）
	bool HasCrystal(size_t stageIndex, int id) const { return IsRespawning(stageIndex) && std::ranges::find(crystals, id) != crystals.end(); }
	// 消す（最初から）
	void Reset() { *this = CheckpointState{}; }
};

// ゲームに1つ（inline 変数：どの .cpp から読んでも同じ1つを指す）
inline CheckpointState gCheckpoint;
