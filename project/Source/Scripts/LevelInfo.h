#pragma once
#include <string>

#include <MyEngine/ECS/Script.h>


/// <summary>
/// レベル（1つのシーン）の情報と進み方。シーンに1つ、空の Entity（名前は Level）に付ける
/// <para>番号・場所の名前（G9 で左下に出す）・下の線の高さ（これより下に落ちたら一発アウト）・次のシーンの名前を持つ</para>
/// <para>R キー / パッドの Back で、いつでもレベルの最初からやり直せる</para>
/// </summary>
class LevelInfo : public Script {
public:
	void Update() override;
	void Fields(ComponentUI& ui) override;

	// ===== ほかの Script から使う =====
	int GetNumber() const { return number_; }
	const std::string& GetAreaName() const { return areaName_; }
	float GetKillHeight() const { return killHeight_; }
	// 次のレベルへ（Next Scene が空・見つからなければ、このレベルを最初から）
	void LoadNextLevel() const;
	// 今のシーンの LevelInfo（無ければ nullptr）。ポインタは使い捨て（フレームをまたいで持たない）
	static LevelInfo* Find() { return FindObjectOfType<LevelInfo>(); }

private:
	// ===== Inspector に出す・保存する値 =====
	int number_ = 1;              // レベルの番号
	std::string areaName_ = "森"; // 場所の名前
	float killHeight_ = -10.0f;   // これより下に落ちたら一発アウト（ワールドの Y）
	std::string nextScene_;       // ゴールの後に読むシーンの名前（resources/scenes/<名前>.scene.json。空なら、このレベルをもう一度）
};