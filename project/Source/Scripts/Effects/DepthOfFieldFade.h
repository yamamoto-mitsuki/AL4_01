#pragma once
#include <MyEngine/Components/Rendering3D/DepthOfFieldComponent.h>
#include <MyEngine/Core/Handle.h>
#include <MyEngine/ECS/Entity.h>


/// <summary>
/// シーンの被写界深度（Depth Of Field）の値を、今の値から決めた値へ、ゆっくり動かす（クリアの瞬間に、ゲームの絵をぼかしていく）
/// <para>Script のメンバーに持って、Begin で始め、毎フレーム Update を呼ぶ。Component ではない（保存しない）</para>
/// <para>ピントの合わせ方は Distance に切り替える。FollowTarget なら、今ピントが合っている奥行きから始める（切り替えた瞬間に、ぼけ方が跳ばないように）</para>
/// <para>Restore で、最初の Begin の前の値へ戻せる（ステージの始まりに、ぼけた所からはっきりさせる）。戻しきったら、ピントの合わせ方も元に戻す</para>
/// <para>シーンを読み直すと、Depth Of Field もファイルの値に戻る（戻す処理は要らない）</para>
/// </summary>
class DepthOfFieldFade {
public:
	/// <summary>
	/// 動かす値（Depth Of Field Component の同じ名前の値）
	/// </summary>
	struct Settings {
		float focusDistance = 2.0f;  // ピントの合う奥行き（m）
		float focusRange = 0.0f;     // ぼけない幅（m）
		float farTransition = 4.0f;  // 奥が一番ぼけるまでの奥行き（m）
		float nearTransition = 0.0f; // 手前が一番ぼけるまでの奥行き（m）
		float maxBlur = 20.0f;       // 一番ぼけた所の半径（ピクセル）
	};

	// 今の値から to へ、duration 秒で動かし始める。シーンに Depth Of Field が無ければ false（何もしない）
	bool Begin(const Settings& to, float duration);
	// 最初の Begin の前の値へ、duration 秒で戻し始める。戻しきったら、ピントの合わせ方も元に戻す。まだ Begin していなければ false
	bool Restore(float duration);
	// 毎フレーム（deltaTime は、ゲームと同じに止まる時間でも、止めない時間でもよい）
	void Update(float deltaTime);
	// 動かしている途中なら、すぐ終わりの値にする（Restore の途中なら、ピントの合わせ方も戻す）。持ち主が消える時に
	void Finish();
	bool IsPlaying() const { return isPlaying_; }

private:
	// 今の値から to へ、duration 秒で動かし始める（Begin・Restore の共通）
	void StartMove(const DepthOfFieldComponent& component, const Settings& to, float duration);

	Handle<Entity> entity_;  // 動かす Depth Of Field の Entity（エンジンが使うのと同じ、有効な物の最初の1つ）
	Settings from_;          // 始めの値
	Settings to_;            // 終わりの値
	float duration_ = 1.0f;  // 動かす秒数
	float elapsed_ = 0.0f;   // 始めてからの秒数
	bool isPlaying_ = false; // 動かしている途中
	// --- Restore で戻す先（最初の Begin の前の値）---
	Settings original_;                                                // 元の値
	DepthOfFieldFocus originalFocus_ = DepthOfFieldFocus::FollowTarget; // 元のピントの合わせ方
	bool hasOriginal_ = false;                                         // 元の値を覚えている（Begin した・まだ戻しきっていない）
	bool isRestoring_ = false;                                         // Restore で戻している途中
};
