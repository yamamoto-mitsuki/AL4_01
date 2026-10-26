#pragma once
#include <string>

#include <MyEngine/ECS/Script.h>

/// <summary>
/// ジャンプで使う輪（水色の円 JumpOrb・緑の円 LaunchOrb の共通部分）。これだけでは付けられない（SCRIPT で登録しない）
/// <para>氷のキューブが輪に触れている間、ジャンプで使える。使ったら、キューブが輪から一度出るまで使えない（入り直すと、また使える）</para>
/// <para>何が起きるか（もう一度跳ぶ・吹っ飛ぶ）は IceCube が決める。ここは「使えるか」を輪ごとに覚えるだけ</para>
/// <para>IceCube は触れている輪を GetComponent&lt;JumpRing&gt;() で集める（JumpOrb・LaunchOrb のどちらも見つかる）</para>
/// <para>粒（どちらも無くてよい）：子の Gather Particle（ふだん真ん中へ吸い寄せられる粒。Converge・Play On Start）は、使ったら消えて、
/// キューブが輪から出たら、また流れ始める。子の Burst Particle（Play On Start を外す）は、使った瞬間に外へはじけ飛ぶ</para>
/// </summary>
class JumpRing : public Script {
public:
	void OnTriggerExit(GameObject other) override; // キューブが出た → また使える
	void Fields(ComponentUI& ui) override;         // 派生クラスの Fields から、最初に呼ぶ

	// ===== IceCube から使う =====
	// 使えるか（まだ使っていない）
	bool IsReady() const { return !isUsed_; }
	// 使う（キューブが輪から出るまで、使えない）。粒はここで、ほかの見た目は OnUse で
	void Use();

protected:
	// 使った瞬間に呼ぶ（輪が広がる・ふくらむなどの見た目を始める。派生クラスで書く）
	virtual void OnUse() {}
	// 使ったか（見た目を変えるのに使う）
	bool IsUsed() const { return isUsed_; }

private:
	// ===== Inspector に出す・保存する値 =====
	std::string gatherParticleName_ = "GatherParticle"; // ふだん吸い寄せられる粒の Particle System（この Entity の直下の子）
	std::string burstParticleName_ = "BurstParticle";   // 使ったときにはじけ飛ぶ粒の Particle System（この Entity の直下の子）

	// ===== 実行中だけの値 =====
	bool isUsed_ = false; // 使った（キューブが出るまで使えない）
};