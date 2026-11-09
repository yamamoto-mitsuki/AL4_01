#pragma once
#include <HTTEngine/Components/Gameplay/AttackTableComponent.h>
#include <HTTEngine/ECS/Script.h>

//=============================================================================
// ボスの猫の体。顔の表情と怯み、フェーズの移行、撃破のアニメーション
//=============================================================================

// ===== 顔の表情 =====
enum class BossFace {
	Normal,    // 0 普通（半目）
	Angry,     // 1 怒り
	Sleep,     // 2 寝る
	Surprised, // 3 驚き
	MouthOpen, // 4 口を開ける
	Hurt,      // 5 当たった
	Dizzy,     // 6 目が回る
	Happy,     // 7 うれしい
};



/// <summary>
/// ボスの猫の体
/// </summary>
class BossCat : public Script {
public:
	void Start() override;
	void Update() override;

	// 顔の表情を変える
	void SetFace(BossFace face);

private:
	void OnStateChanged(AttackTableState state, float duration); // 攻撃表の状態が変わった瞬間（怯み・移行・撃破のアニメ）
	static BossFace ChooseFace(AttackTableState state);          // 状態ごとの顔

	// ===== 実行中だけの値 =====
	AttackTableState lastState_ = AttackTableState::Selecting; // 前のフレームの攻撃表の状態
	BossFace face_ = BossFace::Normal;                         // 今の顔
};