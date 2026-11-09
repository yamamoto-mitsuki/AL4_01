#pragma once
#include <HTTEngine/ECS/Script.h>

//=============================================================================
// 被弾の後の無敵の間、モデルを点滅させる（Health の Invincible After Hit の残りを見る）
//=============================================================================

/// <summary>
/// 被弾の後の無敵の点滅
/// <para>同じ Entity の Health と Model Renderer を使う。回避の無敵（Health の Invincible）では点滅しない</para>
/// </summary>
class HitBlink : public Script {
public:
	void LateUpdate() override;
	void OnDisable() override;
	void Fields(ComponentUI& ui) override;

private:
	void StopBlink();              // 点滅を終えて、表示に戻す
	void SetVisible(bool visible); // Model Renderer の Enabled を書く

	// ===== Inspector で変える値 =====
	float blinkInterval_ = 0.1f; // 消える・出るを切り替える秒（小さいほど速く点滅する）

	// ===== 実行中だけの値 =====
	float blinkElapsed_ = 0.0f; // 点滅を始めてからの秒
	bool isBlinking_ = false;   // 点滅中か（終わったときに表示へ戻すため）
};
