#pragma once

/// <summary>
/// コントローラーの振動（一定の強さで、決めた秒数だけ震わせて止める）
/// <para>エンジンの InputManager::SetVibration は「止めるまで震え続ける」ので、残りの秒数をここで数えて止める</para>
/// <para>数えるのは IceCube::Update から（レベルにはいつもキューブがいる）。ポーズを開いた・キューブが消えた（シーンが替わった）ときは Stop で止める
/// （止まっている間は数えられないので、震えっぱなしにならないように）</para>
/// </summary>
class GameRumble {
public:
	// power（0〜1）の強さで、duration 秒だけ震わせる。震えている途中なら、頭からやり直す
	static void Play(float power, float duration);
	// 毎フレーム（IceCube から）。残りの秒数を減らし、0 になったら止める。deltaTime は止めない時間（ヒットストップ中も数える）
	static void Update(float deltaTime);
	// すぐ止める（震えていなければ何もしない）
	static void Stop();
};
