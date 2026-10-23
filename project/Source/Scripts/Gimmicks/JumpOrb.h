#pragma once
#include <MyEngine/Math/Vector3.h>
#include <MyEngine/Math/Vector4.h>

#include "Source/Scripts/Gimmicks/JumpRing.h"

/// <summary>
/// 水色の円（ジャンプの円）。Collider（Sphere・Is Trigger）を付けた Entity に付ける
/// <para>氷のキューブが触れている間、空中でもう一度跳べる。跳ぶのは IceCube で、「使えるか」は JumpRing が覚える</para>
/// <para>見た目：子の「Ring」（輪。ゆっくり回る。使うと広がって消える）と「Core」（真ん中の光る玉。使うと暗くなる）。どちらも無くてよい</para>
/// </summary>
class JumpOrb : public JumpRing {
public:
	void Start() override;
	void Update() override;
	void Fields(ComponentUI& ui) override;

protected:
	void OnUse() override { popTimer_ = 0.0f; } // 使った瞬間から、輪が広がり始める

private:
	// ===== Inspector に出す・保存する値 =====
	float spinSpeed_ = 30.0f;      // 輪が回る速さ（度/秒）
	float popScale_ = 1.6f;        // 使ったとき、輪がこの倍まで広がって消える
	float popTime_ = 0.25f;        // 輪が広がって消えるまでの秒数
	float usedBrightness_ = 0.25f; // 使った後の、真ん中の玉の明るさ（元の色に掛ける）

	// ===== 実行中だけの値（保存しない）=====
	float popTimer_ = 0.0f;                        // 使ってからの秒数
	GameObject ring_;                              // 子の「Ring」（Start で探す）
	GameObject core_;                              // 子の「Core」（Start で探す）
	Vector3 ringScale_ = {1.0f, 1.0f, 1.0f};       // 輪の元の大きさ
	Vector4 ringColor_ = {1.0f, 1.0f, 1.0f, 1.0f}; // 輪の元の色
	Vector4 coreColor_ = {1.0f, 1.0f, 1.0f, 1.0f}; // 玉の元の色
};