#pragma once
#include <MyEngine/ECS/Entity.h>
#include <MyEngine/Math/Vector3.h>

// 状態
enum class PlayerState {
	Idle, // 何もしていない
	Move, // WASDで歩く（地面のインクに触れると回収）
	Draw, // 左クリックを押している間は、時間がゆっくりになり、線を引く
	Dash, // 離したあと、書いた線に沿って高速で進み、振れた敵を切る
};


// プレイヤー
struct Player {
	// --- 調整項目 ---
	float moveSpeed = 3.0f;     // WASDで1秒に進む距離
	float dashSpeed = 50.0f;    // 線に沿ってダッシュする1秒に進む距離
	float slowScale = 0.1f;     // 書いている間の時間の遅さ（0.1 で10分の1）
	float maxInk = 50.0f;       // インクの上限（掛ける線の長さ）
	float pointSpacing = 0.25f; // マウスがこの距離動くたびに点を足す
	float slashRadius = 0.6f;   // 斬る太さ（動いた跡のカプセルの半径）
	float pickupRadius = 0.5;   // この距離まで近づいたインクを回収
	float hitStopTime = 0.05f;  // 敵を倒したときに止める秒数
	float inkHeight = 0.01f;    // インクの高さ（地面のインクの厚み）
	float wallMargin = 0.05f;   // 壁の手前で止める距離（壁にめり込まないように）
	float drawLineWidth = 0.5f; // 書いた線の太さ（TrailComponent の width に掛ける）
	Vector3 drawLineColor = {1.0f, 0.2f, 0.8f}; // 書いた線の色

	// --- 実行中だけの値 ---
	PlayerState state = PlayerState::Idle;
	float ink = maxInk;     // 現在のインク量
	bool isBlocked = false; // 壁化インク切れで、線がもう伸ばせない
	EntityRef line; // 今の線（描いている・進んでいる・最後に引いた線）
	int dashIndex = 0; // 進んでいるとき、次に向かう点の番号
};