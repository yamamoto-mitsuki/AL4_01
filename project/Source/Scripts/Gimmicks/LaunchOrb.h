#pragma once
#include <MyEngine/Math/Geometry/PathCurve.h>
#include <MyEngine/Math/Vector3.h>

#include "Source/Scripts/Gimmicks/JumpRing.h"

/// <summary>
/// 緑の円（吹っ飛ばす円）。Collider（Sphere・Is Trigger）を付けた Entity に付ける
/// <para>氷のキューブが輪の中でジャンプすると、線の始まり（ふつうは輪の真ん中）に寄ってから、子の「Arrows」（Arrow Strip）の線に沿って吹っ飛ぶ。
/// 線の終わりの後は、終わりの向きへまっすぐ（End At Path End を付けると、線の終わりで吹っ飛びをやめる）。触れただけでは何も起きない（通り抜ける）</para>
/// <para>Arrows が無ければ、この Entity の右（ローカルの +X。Rotation の Z で回る）へまっすぐ</para>
/// <para>吹っ飛ぶ動き（重力なし・始まって少しはジャンプできない・ぶつかるか時間切れで終わる）は IceCube。ここは線・速さ・長さを渡すだけ。
/// 「使えるか」（使ったら輪から出るまで使えない）は JumpRing</para>
/// <para>見た目：子の「Ring」（使うと一瞬ふくらむ）。矢印は子の Arrows（エンジンの Arrow Strip。コードでは動かさない。親と一緒に回る）</para>
/// </summary>
class LaunchOrb : public JumpRing {
public:
	void Start() override;
	void Update() override;
	void Fields(ComponentUI& ui) override;

	// ===== IceCube から使う =====
	// 飛ぶ線を作る（ワールド座標）。子の Arrows の線。無ければ、この Entity の真ん中から右へまっすぐ
	void BuildPath(PathCurve& path) const;
	float GetSpeed() const { return speed_; }
	float GetDuration() const { return duration_; }
	// 線の終わりで吹っ飛びをやめるか（やめると、その時の勢いのまま普通の空中へ）
	bool IsEndAtPathEnd() const { return endAtPathEnd_; }

protected:
	void OnUse() override { pulseTimer_ = 0.0f; } // 使った瞬間に、輪がふくらむ

private:
	// ===== Inspector に出す・保存する値 =====
	float speed_ = 16.0f;       // 飛ぶ速さ（m/s。線に沿って）
	float duration_ = 1.5f;     // 吹っ飛びの長さ（秒。何にもぶつからなければ、この秒数で普通の空中に戻る）
	bool endAtPathEnd_ = false; // 線の終わりで吹っ飛びをやめる（false なら、終わりの向きへまっすぐ飛び続ける）
	float pulseScale_ = 1.4f;   // 使ったとき、輪がこの倍にふくらむ
	float pulseTime_ = 0.2f;    // ふくらんだ輪が戻るまでの秒数

	// ===== 実行中だけの値（保存しない）=====
	GameObject ring_;                        // 子の「Ring」（Start で探す）
	GameObject arrows_;                      // 子の「Arrows」（Arrow Strip。Start で探す）
	Vector3 ringScale_ = {1.0f, 1.0f, 1.0f}; // 輪の元の大きさ
	float pulseTimer_ = 0.0f;                // 使ってからの秒数（Start で Pulse Time にして、ふくらんでいない所から始める）
};