#pragma once
#include <cstdint>


/// <summary>
/// 効果音の種類（resources/Sounds の wav と1つずつ対応。ファイルの名前は GameAudio.cpp の kSeFiles）
/// </summary>
enum class Se : uint8_t {
	Jump,     // 地面のジャンプ・緑の壁・自分のジャンプ（高さを変えて使い分ける）
	RingJump,    // 水色の円でもう一度跳んだ（ド・ソ・高いド の和音）
	RingJumpLow, // 水色の円の1つ目（RingJump の1段下。ラ・ミ・高いラ）。音階を下へ広げるため
	Launch,   // 緑の円で吹っ飛んだ
	Stick,    // 緑の壁にくっついた
	Land,     // 着地
	Shatter,  // 砕けた
	Fall,     // 下に落ちた
	Goal,     // ゴールに入った
	Crystal,      // 隠し結晶を取った（ラ・ド#・ミ・ラ のやわらかい分散和音と、昇るきらめき）
	CrystalBurst, // 隠し結晶が昇りきって、はじけた（ふわっとした息の音と和音）
	UiMove,   // メニューの項目を動かした
	UiDecide, // 決めた
	UiBack,   // 戻った・閉じた・決められなかった
	UiOpen,   // ポーズの画面を開いた
};

/// <summary>
/// ゲームの音（効果音・BGM・風）をまとめて鳴らす
/// <para>音のファイルは初めて呼ばれた時に1回だけ読む（SoundManager::Load）。Entity も AudioSource も使わない</para>
/// <para>BGM はシーンをまたいで鳴り続ける（LoadScene ではエンジンが音を止めないので）。
/// やられ・やり直しで「低く・小さく」して、次にレベルが始まったら（StartBgm）元へゆっくり戻す</para>
/// <para>エディタで Play したシーンをやり直すと、エンジンが全部の音を止める（SceneManager::ReloadImmediate）。
/// そのときは StartBgm が頭からフェードインで鳴らし直す（製品版では止まらずに続きから戻る）</para>
/// <para>風の音も直接鳴らす（Wind Zone ごとではなく、キューブが今いる所の風で1つ。重なった Wind Zone でも二重に鳴らない）</para>
/// </summary>
class GameAudio {
public:
	// ===== 効果音 =====
	// se を1回鳴らす。pitch：1=元の高さ・2=1オクターブ上。volume：1=ファイルの大きさ
	static void PlaySe(Se se, float pitch = 1.0f, float volume = 1.0f);
	// 水色の円の音。chain（地面に降りてから何回目の円か。0 から）が増えるほど、音階で高くなる（ラ・ド・レ・ミ・ソ・ラ・ド の7段）
	static void PlayRingJump(int chain);

	// ===== BGM =====
	// レベル・タイトルが始まった。鳴っていなければ頭からフェードインで鳴らす。鳴っていれば（下げた音を）元の高さ・大きさへ戻す
	static void StartBgm();
	// やられ・やり直し：音を低くして、fadeTime 秒で小さくする（次の StartBgm で戻る）
	static void DownBgm(float fadeTime);
	// ゴール：fadeTime 秒で小さくする（ゴールの音を聞かせる。高さは変えない）
	static void DuckBgm(float fadeTime);

	// ===== 風 =====
	// 毎フレーム（IceCube から）。今いる所の風の速さ（m/s。ゆれ込み）と強さ（m/s²。0 なら風の外）で、風の音の大きさと音色を変える
	// 弱い風の音と強い風の音を2つずっと鳴らしておいて、混ぜる割合を変える（風が強いほど大きく、高い音・ひゅうっと鳴る音が増える）
	static void UpdateWind(float speed, float strength, float deltaTime);
	// 風の音をすぐ消す（タイトルへ戻ったとき。ポーズのまま風の中からタイトルへ戻ると、風の音が残るので）
	static void MuteWind();
};
