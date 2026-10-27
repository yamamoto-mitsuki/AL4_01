# 音：効果音・BGM・やり直しで BGM が沈む・風の強さで変わる風の音

前：[Polish2.md](Polish2.md)（仕上げ2）。設計：[OutGame.md](OutGame.md)

> **2026-10-27：このページの中身は、ユーザーに頼まれてソースへ直接入れた**（写経はしなくてよい。読む用の説明として残す）。
> 入れる前のファイルは scratchpad の `backup_before_sound` に写してある。風の音（パート W）は後から足した。

参考：
- エンジン：`Sound/SoundManager.h`（`Load`・`Play`・`SetPitch`・`FadeIn`・`FadeOut`・`IsPlaying`）・`Sound/AudioBus.h`（BGM / SE の音量のつまみ）・`Systems/AudioSystem.h`（Audio Source）・`Scene/SceneManager.cpp`（`ReloadImmediate` が `AudioSystem::StopAll` を呼ぶ）・`Scene/SceneTransition.h`（`kDefaultFadeTime`）
- XAudio2 のフィルター：[IXAudio2Voice::SetFilterParameters](https://learn.microsoft.com/ja-jp/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2voice-setfilterparameters)（声を `XAUDIO2_VOICE_USEFILTER` で作らないと効かない）
- magic_enum：[GitHub](https://github.com/Neargye/magic_enum)（`enum_count`）

## この段階でやること

ユーザーの希望（2026-10-27）：
1. 作った音（`resources/Sounds` の 15 個）を**ゲームで鳴らす**
2. **落ちてやり直し・リスタートのときは、BGM をフェードで分かりやすく変える**
3. **風は強さによって音を変える**（→ パート W）

| パート | ファイル | 新／変 | 中身 |
|---|---|---|---|
| A | `Source/Scripts/Audio/GameAudio.h / .cpp` | **新しいファイル** | 効果音と BGM をまとめて鳴らす（BGM を沈める・戻す） |
| B | `Source/Scripts/IceCube.h / .cpp` | h は2か所・cpp は7か所 | ジャンプ・円・壁・吹っ飛び・着地・やられの音。やられで BGM を沈める |
| C | `Source/Scripts/LevelInfo.cpp` | 3か所 | レベルが始まったら BGM を戻す。R で沈める。ポーズを開く音 |
| D | `Source/Scripts/GoalDoor.cpp` | 2か所 | ゴールの音。BGM を小さく |
| E | `Source/Scripts/UI/MenuBase.cpp` | 3か所 | 項目を動かす・決める・決められない音 |
| F | `Source/Scripts/UI/PauseMenu.h / .cpp`・`StageSelectMenu.h / .cpp` | 少し | 戻るの音（決めた音と重ならないように `Close` を分ける）。ポーズの Restart で BGM を沈める |
| G | `Source/Scripts/UI/TitleMenu.cpp` | 2か所 | タイトルで BGM を鳴らす・戻す（風の音を消す） |
| W | `GameAudio`・`IceCube.cpp`・`TitleMenu.cpp` | 足す | 風の音：キューブが今いる所の風の強さで、大きさと音色が変わる |

GameAudio.h / .cpp は vcxproj・filters に入っている（`Source\Scripts\Audio`）。エディタでする事は無い（風の音も Audio Source を付けない）。

---

## 0. 作りの考え方

### 鳴らす音とファイル

`resources/Sounds/` に置いた wav（プログラムで合成した物）。どこで鳴るか：

| Se | ファイル | 鳴らす所 |
|---|---|---|
| `Jump` | `se_jump.wav` | 地面のジャンプ（1.0）・緑の壁から跳ぶ（1.2）・戻った自分のジャンプ（1.35）。( ) は高さ |
| `RingJump` / `RingJumpLow` | `se_ring_jump.wav` / `se_ring_jump_low.wav` | 水色の円。**着地するまで続けて使うと、ラ・ド・レ・ミ・ソ・ラ・ド と上がる**（1つ目のラだけ Low） |
| `Launch` | `se_launch.wav` | 緑の円で吹っ飛ぶ |
| `Stick` | `se_stick.wav` | 緑の壁にくっつく |
| `Land` | `se_land.wav` | 着地（強く着地するほど大きい。そっと降りたら鳴らない） |
| `Shatter` / `Fall` | `se_shatter.wav` / `se_fall.wav` | 砕けた / 下に落ちた |
| `Goal` | `se_goal.wav` | ゴールに入った |
| `UiMove` / `UiDecide` / `UiBack` / `UiOpen` | `ui_*.wav` | 項目を動かす / 決める / 戻る・決められない / ポーズを開く |
| （BGM） | `bgm_forest_loop.wav` | ずっと（48 秒でつなぎ目なしに回る） |
| （風） | `amb_wind_loop.wav`（弱い）・`amb_wind_strong_loop.wav`（強い） | 風の中にいる間（パート W） |

熱湯（Melt）とトゲは使わないので、溶ける音・ダメージの音は無い。

UI の4つは 2026-10-27 に作り直した（ユーザー：「UI の音が気になる」）。前は高い鈴（1.3〜2kHz）がカチッと始まって耳に刺さったので、**約1オクターブ下・ほぼ澄んだ音（倍音を減らす）・ふわっと始まる（4〜8ms かけて立ち上がる）・5〜7dB 小さく**した。move＝D5 の小さな「ぽ」、decide＝E5→B5、back＝B4→E4（こもらせて、さらに小さく）、open＝息のような風と A4。前の音は scratchpad の `ui_old` にある。

### BGM はシーンをまたいで鳴らす（Audio Source にしない）

BGM を各シーンの Audio Source（Play On Start・Loop）にすると、やり直すたびに**頭から鳴り直す**。何度もやり直すゲームなので、同じ出だしを何回も聞くことになる。
そこで `SoundManager::Play` で直接鳴らして、再生ハンドルを `GameAudio` が覚えておく。エンジンは `LoadScene` では音を止めないので、シーンが替わっても続きが鳴る。

ただしエディタで **Play を押したシーン**をやり直すと、エンジンが全部の音を止める（`SceneManager::ReloadImmediate` の `AudioSystem::StopAll`）。そのときは `StartBgm` が「鳴っていない」と分かって、頭からフェードインで鳴らし直す。
→ **製品版（タイトルから入る）＝続きから戻る。エディタで Level を直接 Play ＝頭からフェードイン**。どちらも「やり直した」と耳で分かる。

### 「分かりやすく変える」＝音を低く・小さくして、始まったら戻す

| いつ | BGM | 秒数 |
|---|---|---|
| やられた瞬間（`Kill`） | **高さ 0.84 倍（約 3 半音下）にして、ふだんの 0.2 倍の大きさへ** | 幕が閉じきるまで（落ちた 0.2＋0.5 秒・砕けた 1＋0.5 秒） |
| R・ポーズの Restart | 同じ | 幕を閉じる 0.4 秒 |
| ゴール | 大きさだけ 0.35 倍へ（高さはそのまま） | ぼける秒数（Blur Time） |
| レベル・タイトルが始まった（`StartBgm`） | 高さを 1 に戻して、ふだんの大きさへ | 1.5 秒（鳴っていなければ無音から 2 秒） |

**こもらせる（ローパス）はできなかった**：`SoundManager::SetLowPassFilter` はあるが、エンジンが声を `XAUDIO2_VOICE_USEFILTER` なしで作っている（`CreateSourceVoice` のフラグが 0）ので、フィルターが効かない。エンジンを変えるとできる（エンジンのチャットで）。ここでは高さと大きさで変える。

**高さは一度に下げる**：`SetPitch` は呼ぶたびにログを1行出すので、毎フレーム少しずつ下げるとログがあふれる。一度に下げても、同時に大きさが下がり始めるので「がくっと沈む」感じになる。戻すときは小さい間に高さを戻すので、跳ねたのは目立たない。

**`FadeOut` で上げる**：エンジンの `FadeOut` は中で「今の大きさ → 目標の大きさ」へ動かすだけなので、目標を今より大きくすれば**上げる**のにも使える。`FadeIn` は必ず 0 から始め直すので、戻すのに使うと一度ぷつっと消える。
また、`FadeOut` の目標を 0 にすると、終わったときにエンジンが音を止める（続きから戻せない）ので、沈めるときも 0 にしない。

---

## パート A：`Source/Scripts/Audio/GameAudio.h / .cpp`（新しいファイル）

（下のコードは風の音を足す前の形。今のファイルには、パート W の風の分が足してある）

### `GameAudio.h`

```cpp
#pragma once
#include <cstdint>


/// <summary>
/// 効果音の種類（resources/Sounds の wav と1つずつ対応。ファイルの名前は GameAudio.cpp の kSeFiles）
/// </summary>
enum class Se : uint8_t {
	Jump,     // 地面のジャンプ・緑の壁・自分のジャンプ（高さを変えて使い分ける）
	RingJump, // 水色の円でもう一度跳んだ
	Launch,   // 緑の円で吹っ飛んだ
	Stick,    // 緑の壁にくっついた
	Land,     // 着地
	Shatter,  // 砕けた
	Fall,     // 下に落ちた
	Goal,     // ゴールに入った
	UiMove,   // メニューの項目を動かした
	UiDecide, // 決めた
	UiBack,   // 戻った・閉じた・決められなかった
	UiOpen,   // ポーズの画面を開いた
};

/// <summary>
/// ゲームの音（効果音と BGM）をまとめて鳴らす
/// <para>音のファイルは初めて呼ばれた時に1回だけ読む（SoundManager::Load）。Entity も AudioSource も使わない</para>
/// <para>BGM はシーンをまたいで鳴り続ける（LoadScene ではエンジンが音を止めないので）。
/// やられ・やり直しで「低く・小さく」して、次にレベルが始まったら（StartBgm）元へゆっくり戻す</para>
/// <para>エディタで Play したシーンをやり直すと、エンジンが全部の音を止める（SceneManager::ReloadImmediate）。
/// そのときは StartBgm が頭からフェードインで鳴らし直す（製品版では止まらずに続きから戻る）</para>
/// </summary>
class GameAudio {
public:
	// ===== 効果音 =====
	// se を1回鳴らす。pitch：1=元の高さ・2=1オクターブ上。volume：1=ファイルの大きさ
	static void PlaySe(Se se, float pitch = 1.0f, float volume = 1.0f);
	// 水色の円の音。chain（地面に降りてから何回目の円か。0 から）が増えるほど、音階で高くなる
	static void PlayRingJump(int chain);

	// ===== BGM =====
	// レベル・タイトルが始まった。鳴っていなければ頭からフェードインで鳴らす。鳴っていれば（下げた音を）元の高さ・大きさへ戻す
	static void StartBgm();
	// やられ・やり直し：音を低くして、fadeTime 秒で小さくする（次の StartBgm で戻る）
	static void DownBgm(float fadeTime);
	// ゴール：fadeTime 秒で小さくする（ゴールの音を聞かせる。高さは変えない）
	static void DuckBgm(float fadeTime);
};
```

### `GameAudio.cpp`

```cpp
#include "GameAudio.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <string_view>

#include <externals/magic_enum/magic_enum.hpp>

#include <MyEngine/Sound/SoundManager.h>

namespace {
//=============================================================================
// ファイルと決まった値
//=============================================================================
// 効果音のファイル（Se と同じ並び。Se を足したら、ここにも同じ所へ足す）
constexpr auto kSeFiles = std::to_array<std::string_view>({
    "resources/Sounds/se_jump.wav",
    "resources/Sounds/se_ring_jump.wav",
    "resources/Sounds/se_launch.wav",
    "resources/Sounds/se_stick.wav",
    "resources/Sounds/se_land.wav",
    "resources/Sounds/se_shatter.wav",
    "resources/Sounds/se_fall.wav",
    "resources/Sounds/se_goal.wav",
    "resources/Sounds/ui_move.wav",
    "resources/Sounds/ui_decide.wav",
    "resources/Sounds/ui_back.wav",
    "resources/Sounds/ui_open.wav",
});
// magic_enum::enum_count は enum の項目の数（Se の数）。足し忘れたら、ここでビルドが止まる
static_assert(kSeFiles.size() == magic_enum::enum_count<Se>(), "kSeFiles と Se の数が合っていません");
constexpr std::string_view kBgmFile = "resources/Sounds/bgm_forest_loop.wav";

// --- 水色の円：続けて使うと、ド・レ・ミ・ソ・ラ・ド と上がる（ペンタトニック。どこで止まっても濁らない）---
// 数は元の高さに掛ける倍率（1.5 なら5度上＝ドに対するソ）。最後まで行ったら、高いドのまま
constexpr auto kRingPitches = std::to_array<float>({1.0f, 1.125f, 1.25f, 1.5f, 1.6667f, 2.0f});

// --- BGM ---
constexpr float kBgmVolume = 0.6f;      // ふだんの大きさ（効果音より控えめに）
constexpr float kBgmFadeInTime = 2.0f;  // 無音から鳴らし始めるとき、ふだんの大きさになるまでの秒数
constexpr float kBgmRecoverTime = 1.5f; // 下げた音が、ふだんの大きさに戻るまでの秒数
constexpr float kDownPitch = 0.84f;     // やられ・やり直しの音の高さ（0.84 ≒ 3 半音下。がくっと沈んで聞こえる）
constexpr float kDownVolume = 0.2f;     // やられ・やり直しの大きさ（ふだんの何倍か）
constexpr float kDuckVolume = 0.35f;    // ゴールの大きさ（ふだんの何倍か）
constexpr float kMinFadeTime = 0.01f;   // フェードの秒数の下限（SoundManager が秒数で割るので 0 にしない）

/// <summary>
/// 読んだ音と、鳴っている BGM（この .cpp の中だけ）
/// </summary>
struct AudioState {
	std::array<uint32_t, kSeFiles.size()> seSounds{}; // 効果音の素材ハンドル（0 は読めなかった）
	uint32_t bgmSound = 0;  // BGM の素材ハンドル（0 は読めなかった）
	uint32_t bgmPlay = 0;   // 鳴っている BGM の再生ハンドル（止まったら SoundManager::IsPlaying が false）
	bool isBgmDown = false; // やられ・やり直しで、音を低くしている
	bool isLoaded = false;  // もう読んだ
};
AudioState state;

// 初めて呼ばれた時に1回だけ、全部の音を読む（読めなかった音は 0 のまま。鳴らすときに飛ばす）
// 1回だけにするのは、読めなかった音を鳴らすたびに読み直して、ログが増えないように
void LoadIfNeeded() {
	if (state.isLoaded) {
		return;
	}
	state.isLoaded = true;
	for (size_t i = 0; i < kSeFiles.size(); ++i) {
		state.seSounds[i] = SoundManager::Load(std::filesystem::path(kSeFiles[i]));
	}
	state.bgmSound = SoundManager::Load(std::filesystem::path(kBgmFile));
}
} // namespace


//=============================================================================
// 効果音
//=============================================================================
void GameAudio::PlaySe(Se se, float pitch, float volume) {
	LoadIfNeeded();
	const uint32_t sound = state.seSounds[static_cast<size_t>(se)];
	if (sound == 0) {
		return; // 読めなかった（Load がログを出した）。0 を Play に渡すと assert で止まる
	}
	SoundManager::Play(sound, false, volume, pitch, AudioBus::SE);
}

void GameAudio::PlayRingJump(int chain) {
	// 一覧より多く続けたら、最後（高いド）のまま。負の数は最初
	const size_t index = (chain <= 0) ? 0 : (std::min)(static_cast<size_t>(chain), kRingPitches.size() - 1);
	PlaySe(Se::RingJump, kRingPitches[index]);
}


//=============================================================================
// BGM
//=============================================================================
void GameAudio::StartBgm() {
	LoadIfNeeded();
	if (state.bgmSound == 0) {
		return;
	}
	// --- 鳴っていない（初めて・エディタのやり直しでエンジンが止めた）：頭から、無音からフェードイン ---
	if (!SoundManager::IsPlaying(state.bgmPlay)) {
		// 大きさ 0 で鳴らし始めてからフェードイン（ふだんの大きさで鳴らすと、FadeIn が 0 にするまでの一瞬だけ大きく鳴る）
		state.bgmPlay = SoundManager::Play(state.bgmSound, true, 0.0f, 1.0f, AudioBus::BGM);
		SoundManager::FadeIn(state.bgmPlay, kBgmFadeInTime, kBgmVolume, Ease::Type::InOutSine);
		state.isBgmDown = false;
		return;
	}
	// --- 鳴っている（製品版のやり直し・次のレベル・タイトルへ戻った）：元の高さに戻して、ふだんの大きさへゆっくり ---
	if (state.isBgmDown) {
		SoundManager::SetPitch(state.bgmPlay, 1.0f); // 小さい間に戻すので、高さが跳ねたのは目立たない
		state.isBgmDown = false;
	}
	// FadeOut は「今の大きさ → 目標の大きさ」へ動かすだけなので、上げるのにも使える
	// （FadeIn は 0 から始め直すので、ここで使うと一度ぷつっと消える）
	SoundManager::FadeOut(state.bgmPlay, kBgmRecoverTime, kBgmVolume, Ease::Type::InOutSine);
}

void GameAudio::DownBgm(float fadeTime) {
	if (!SoundManager::IsPlaying(state.bgmPlay)) {
		return;
	}
	// 高さは一度に下げる（SetPitch は呼ぶたびにログを出すので、毎フレーム少しずつは下げない）
	if (!state.isBgmDown) {
		SoundManager::SetPitch(state.bgmPlay, kDownPitch);
		state.isBgmDown = true;
	}
	// 目標は 0 にしない（0 で終わると SoundManager が音を止めてしまい、続きから戻せない）
	SoundManager::FadeOut(state.bgmPlay, (std::max)(fadeTime, kMinFadeTime), kBgmVolume * kDownVolume, Ease::Type::OutQuad);
}

void GameAudio::DuckBgm(float fadeTime) {
	if (!SoundManager::IsPlaying(state.bgmPlay)) {
		return;
	}
	SoundManager::FadeOut(state.bgmPlay, (std::max)(fadeTime, kMinFadeTime), kBgmVolume * kDuckVolume, Ease::Type::OutQuad);
}
```

### 解説

- **`magic_enum::enum_count<Se>()`**：あまり使わないライブラリなので説明。enum の中の項目の数を、コンパイルの時に数えてくれる（エンジンの `SoundManager.h` も `AudioBus` の数に使っている）。`static_assert` と組み合わせると、`Se` に項目を足して `kSeFiles` に足し忘れたとき、**ビルドの時点でエラー**になる（遊んでいて違う音が鳴る、にならない）。
- **`std::to_array`**：`StageCatalog.h` と同じ。数を書かずに `std::array` を作る。
- **`std::filesystem::path(kSeFiles[i])`**：`SoundManager::Load` は path を受け取る。ファイル名は英数字だけ（エンジンの決まり）なので、そのまま path にしてよい。
- **読むのは最初の1回**：`SoundManager::Load` は同じパスならキャッシュから返すが、読むたびにログを出す。なので最初にまとめて読んで、素材ハンドルを覚えておく。BGM（8MB）も一緒に読む（起動のタイトルで読まれる）。
- **`(std::min)` / `(std::max)` の括弧**：Windows.h の `min` / `max` マクロに置き換えられないようにするため（CLAUDE.md の決まり。IceCube と同じ書き方）。
- **BGM の大きさ**：Parameters ウィンドウの `Audio / Volume / BGM`（バスの音量）がさらに掛かる。全体を変えたいときはそこ、沈み方を変えたいときは `kDownPitch`・`kDownVolume`。

---

## パート B：`Source/Scripts/IceCube.h / .cpp`

### `IceCube.h`（2か所）

① 実行中だけの値の `isJustLanded_` の下に1行足す
```cpp
	bool isJustLanded_ = false;                 // このフレームに着地した
	int ringChain_ = 0;                         // 地面に降りてから、水色の円を何回使ったか（円の音の高さ。着地で 0 に戻す）
```

② 決まった値の一番下（`kMeltSpread` の下）に足す
```cpp
	static constexpr float kMeltSpread = 0.6f;            // 溶けきったとき、横がどれだけ広がるか
	// --- 音 ---
	static constexpr float kWallJumpPitch = 1.2f;      // 緑の壁から跳んだ音の高さ（地面のジャンプの音を少し高くして使う）
	static constexpr float kAirJumpPitch = 1.35f;      // 自分のジャンプ（緑の壁・緑の円で戻った分）を空中で使った音の高さ
	static constexpr float kLandSoundImpact = 0.15f;   // 着地の強さ（0〜1）がこれより小さければ、着地の音を鳴らさない（坂の小さな段差で鳴り続けないように）
	static constexpr float kLandSoundMinVolume = 0.4f; // 着地の音の一番小さい大きさ（強く着地するほど 1 へ近づく）
```

### `IceCube.cpp`（7か所）

① include に1行
```cpp
#include "LevelInfo.h"
#include "Source/Scripts/Audio/GameAudio.h"   // ← 足す
#include "Source/Scripts/Gimmicks/JumpOrb.h"
```

② `UpdateStick` の壁ジャンプ：`Jump(rigidbody);` の下に1行
```cpp
		ChangeState(State::Move);
		hasAirJump_ = false; // 壁で戻った自分のジャンプを使った
		rigidbody.useGravity = true;
		Jump(rigidbody);
		GameAudio::PlaySe(Se::Jump, kWallJumpPitch);   // ← 足す
```

③ `UpdateGround` の着地の瞬間
```cpp
// 前
		if (isJustLanded_) {
			const float impact = (maxFallSpeed_ > 0.0f) ? std::clamp(-previousVelocityY_ / maxFallSpeed_, 0.0f, 1.0f) : 1.0f;
			stretch_ = -landSquash_ * impact;
		}
// 後
		if (isJustLanded_) {
			const float impact = (maxFallSpeed_ > 0.0f) ? std::clamp(-previousVelocityY_ / maxFallSpeed_, 0.0f, 1.0f) : 1.0f;
			stretch_ = -landSquash_ * impact;
			// 音：強く着地するほど大きく（そっと降りた・小さな段差では鳴らさない）
			if (impact >= kLandSoundImpact) {
				GameAudio::PlaySe(Se::Land, 1.0f, MathUtility::Lerp(kLandSoundMinVolume, 1.0f, impact));
			}
			ringChain_ = 0; // 水色の円の音の高さを、最初に戻す
		}
```

④ `UpdateJump` の跳ぶ所
```cpp
// 前
		if (CanGroundJump() || UseJumpOrb() || UseAirJump()) {
			Jump(rigidbody);
		}
// 後
		// 音：地面のジャンプはここで鳴らす。円・自分のジャンプの音は、それぞれの Use の中で鳴らす（使えたときだけ）
		if (CanGroundJump()) {
			GameAudio::PlaySe(Se::Jump);
			Jump(rigidbody);
		} else if (UseJumpOrb() || UseAirJump()) {
			Jump(rigidbody);
		}
```
（上の2行のコメント「使う順番〜」「|| は左から順に〜」はそのまま残す。`||` の短絡評価は `else if` の中でも同じ）

⑤ `UseJumpOrb` と `UseAirJump`
```cpp
// 前（UseJumpOrb の中）
		if (orb != nullptr && orb->IsReady()) {
			orb->Use();
			return true;
		}
// 後
		if (orb != nullptr && orb->IsReady()) {
			orb->Use();
			// 音：着地するまで続けて使うほど、音階で高くなる（ringChain_ は着地で 0 に戻る）
			GameAudio::PlayRingJump(ringChain_);
			++ringChain_;
			return true;
		}
```
```cpp
// 前（UseAirJump）
	hasAirJump_ = false;
	return true;
}
// 後
	hasAirJump_ = false;
	GameAudio::PlaySe(Se::Jump, kAirJumpPitch);
	return true;
}
```

⑥ `TryStick` と `Launch`（どちらも1行足す）
```cpp
	// --- くっつく：止まって、自分のジャンプが戻る ---
	GameAudio::PlaySe(Se::Stick);   // ← 足す
	ChangeState(State::Stick);
```
```cpp
	GameAudio::PlaySe(Se::Launch);   // ← 足す
	ChangeState(State::Launch); // 吹っ飛びの最中に別の円の中でジャンプしたら、そっちの線で吹っ飛び直す
```

⑦ `Kill` の switch から最後まで
```cpp
// 前
	switch (how) {
	case DeathType::Shatter:
		SpawnShards(); // かけらを作って、元のキューブを隠す
		break;
	case DeathType::Melt:
		meltAmount_ = 0.0f; // UpdateDead で、だんだん溶ける
		break;
	case DeathType::Fall:
		break;
	}
}
// 後
	switch (how) {
	case DeathType::Shatter:
		SpawnShards(); // かけらを作って、元のキューブを隠す
		GameAudio::PlaySe(Se::Shatter);
		break;
	case DeathType::Melt:
		meltAmount_ = 0.0f; // UpdateDead で、だんだん溶ける（熱湯は使っていないので、音は無い）
		break;
	case DeathType::Fall:
		GameAudio::PlaySe(Se::Fall);
		break;
	}
	// --- BGM：低く・小さくして「やり直し」を知らせる。幕が閉じきるまでに小さくなりきる ---
	// （幕を閉じ始めるのは UpdateDead で、やられ方ごとの Delay の後。次にレベルが始まったら LevelInfo が戻す）
	const float delay = (how == DeathType::Fall) ? fallRestartDelay_ : restartDelay_;
	GameAudio::DownBgm(delay + restartFadeTime_);
}
```

### 解説

- **円の音の高さ**：`ringChain_` は「着地してから何個目の円か」。円 → 円 → 円と乗り継ぐと、ド → レ → ミ…と上がっていく（Level10 の水色の円 7 個の所で効く）。着地（`isJustLanded_`）で 0 に戻す。緑の壁にくっついても戻さない（壁を挟んだ乗り継ぎも上がり続ける）。
- **着地の音を鳴らさない時**：最初にシーンが始まったフレームも「着地した」になる（`isOnGround_` が false から始まる）が、そのときは落ちる速さが 0 なので `impact` が 0 → 鳴らない。坂のつなぎ目で一瞬浮いただけのときも同じ。
- **地面のジャンプの音を `Jump()` の中で鳴らさない理由**：`Jump()` は壁ジャンプ・吹っ飛びをやめて跳ぶ、でも呼ばれる。どのジャンプかで音（高さ）を変えたいので、呼ぶ側で鳴らす。

---

## パート C：`Source/Scripts/LevelInfo.cpp`（3か所）

① include に1行
```cpp
#include "Source/Scripts/Audio/GameAudio.h"   // ← 足す
#include "Source/Scripts/Progress/GameProgress.h"
```

② `Start` の頭
```cpp
void LevelInfo::Start() {
	// --- BGM：鳴っていなければ鳴らす。やられ・やり直しで下げていたら、元へゆっくり戻す（ここで「やり直した」と分かる）---
	GameAudio::StartBgm();
	// --- 番号が一覧に無ければ知らせる（記録も次のステージも無くなる）---
```

③ `Update` のやり直しとポーズ
```cpp
// 前
	if (InputManager::IsKeyTriggered(DIK_R) || InputManager::IsButtonTriggered(GamepadButton::Back)) {
		SceneTransition::ReloadScene();
	}
	// --- ポーズ：画面を出す（閉じるのは、画面の中の PauseMenu。ポーズ中はこの Update が呼ばれないので）---
	if (!pauseScreen_.empty() && (InputManager::IsKeyTriggered(DIK_ESCAPE) || InputManager::IsButtonTriggered(GamepadButton::Start))) {
		UIScreen::Open(pauseScreen_);
	}
// 後
	// 幕を閉じ始めたら（もう幕が動いていたら false）、BGM を低く・小さくする（幕が閉じきるまでに）
	if (InputManager::IsKeyTriggered(DIK_R) || InputManager::IsButtonTriggered(GamepadButton::Back)) {
		if (SceneTransition::ReloadScene()) {
			GameAudio::DownBgm(SceneTransition::kDefaultFadeTime);
		}
	}
	// --- ポーズ：画面を出す（閉じるのは、画面の中の PauseMenu。ポーズ中はこの Update が呼ばれないので）---
	if (!pauseScreen_.empty() && (InputManager::IsKeyTriggered(DIK_ESCAPE) || InputManager::IsButtonTriggered(GamepadButton::Start))) {
		if (UIScreen::Open(pauseScreen_)) {
			GameAudio::PlaySe(Se::UiOpen);
		}
	}
```

---

## パート D：`Source/Scripts/GoalDoor.cpp`（2か所）

① include に1行
```cpp
#include "IceCube.h"
#include "LevelInfo.h"
#include "Source/Scripts/Audio/GameAudio.h"   // ← 足す
```

② `OnTriggerEnter` の「レベルにクリアを知らせる」の下
```cpp
	if (LevelInfo* level = LevelInfo::Find()) {
		level->Clear();
	}
	// --- 音：ゴールの音を鳴らして、BGM はぼけるのと同じ秒数で小さくする（次のレベル・タイトルが始まったら戻る）---
	GameAudio::PlaySe(Se::Goal);
	GameAudio::DuckBgm(blurTime_);
	// --- 粒が昇り始める ---
```

---

## パート E：`Source/Scripts/UI/MenuBase.cpp`（3か所）

① include（エンジンの include の後に、1行空けて）
```cpp
#include <MyEngine/Systems/TextRenderSystem.h>

#include "Source/Scripts/Audio/GameAudio.h"   // ← 足す
```

② `Update` の「選ぶ」
```cpp
// 前
			if (!MoveByInput(count, stick) && hovered >= 0 && isMouseMoved) {
				selected_ = hovered;
			}
// 後
			const int before = selected_;
			if (!MoveByInput(count, stick) && hovered >= 0 && isMouseMoved) {
				selected_ = hovered;
			}
			// 選んだ項目が変わったときだけ鳴らす（端で止まった・同じ項目の上でマウスを動かしただけなら鳴らさない）
			if (selected_ != before) {
				GameAudio::PlaySe(Se::UiMove);
			}
```

③ `TryDecide`
```cpp
// 前
	if (!CanDecide(index)) {
		OnDecideRejected(index);
		return;
	}
	const Entity* item = EntityManager::Get(items[index]);
// 後
	if (!CanDecide(index)) {
		GameAudio::PlaySe(Se::UiBack); // 決められない（まだ遊べないマスなど）：戻るの音で「入れない」と知らせる
		OnDecideRejected(index);
		return;
	}
	GameAudio::PlaySe(Se::UiDecide);
	const Entity* item = EntityManager::Get(items[index]);
```

**戻るの音は MenuBase に入れない**：戻るキーは全部のメニューで `OnCancel` を呼ぶが、タイトルの `OnCancel` は何もしない（戻る先が無い）。そこで鳴ると「何か起きた」ように聞こえるので、閉じるメニュー（F）だけで鳴らす。

---

## パート F：`PauseMenu`・`StageSelectMenu`

Resume / Back の項目を**決めた**ときは、今まで `OnCancel()` を呼んでいた。そこで戻るの音を鳴らすと、決めた音（E）と重なる。なので「閉じるだけ」の `Close()` を分けて、`OnCancel` は「戻るの音＋Close」にする。

### `PauseMenu.h`
```cpp
// 前
	void OnCancel() override; // 閉じて、ゲームに戻る

private:
// 後
	void OnCancel() override; // 戻るの音を鳴らして、閉じる

private:
	// 閉じて、ゲームに戻る（Resume を決めたときは、決めた音だけにしたいので、戻るの音は鳴らさない）
	void Close();

```

### `PauseMenu.cpp`（全部差し替え）
```cpp
#include "PauseMenu.h"

#include "Source/Scripts/Audio/GameAudio.h"

SCRIPT(PauseMenu, "UI");

//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void PauseMenu::Fields(ComponentUI& ui) {
	MenuBase::Fields(ui);
	ui.Label("項目の名前で何をするか決める：Resume（つづける）/ Restart（やりなおす）/ Title（タイトルへ）。戻るキーでも閉じる");
	ui.Field("Screen Name", screenName_, Tip("この画面の名前（resources/ui/<名前>.uiscreen）。閉じるときに使う"));
	ui.Field("Title Scene", titleScene_, Tip("Title を選んだときに読むシーンの名前（resources/scenes の .scene.json の名前）"));
}

//=============================================================================
// 決める・戻る
//=============================================================================
void PauseMenu::Decide(int /*index*/, const std::string& itemName) {
	if (itemName == "Resume") {
		Close();
	} else if (itemName == "Restart") {
		// ポーズのまま幕を閉じて、最初から（作り直すとポーズも画面も消える）
		isDecided_ = SceneTransition::ReloadScene();
		// BGM を低く・小さくする（幕が閉じきるまでに。次にレベルが始まったら LevelInfo が戻す）
		if (isDecided_) {
			GameAudio::DownBgm(SceneTransition::kDefaultFadeTime);
		}
	} else if (itemName == "Title") {
		// ポーズのまま幕を閉じて、タイトルへ（シーンが替わるとポーズも画面も消える）
		isDecided_ = SceneTransition::LoadScene(titleScene_);
	}
}

void PauseMenu::OnCancel() {
	GameAudio::PlaySe(Se::UiBack);
	Close();
}

void PauseMenu::Close() {
	UIScreen::Close(screenName_); // 消し始めたら、すぐゲームが動く
	isDecided_ = true;
}
```

### `StageSelectMenu.h`（private の `SetText` の下に足す）
```cpp
	void SetText(const std::string& name, const std::string& text) const;
	// 閉じてタイトルへ戻る（Back を決めたときは、決めた音だけにしたいので、戻るの音は鳴らさない）
	void Close();
```

### `StageSelectMenu.cpp`（2か所）

① include に1行
```cpp
#include "Source/Scripts/Audio/GameAudio.h"   // ← 足す
#include "Source/Scripts/Progress/GameProgress.h"
```

② `Decide` の Back と `OnCancel`
```cpp
// 前
	if (IsBackIndex(index)) {
		OnCancel();
		return;
	}
	...
void StageSelectMenu::OnCancel() {
	UIScreen::Close(screenName_); // 閉じると、タイトルの項目がまた動く
	isDecided_ = true;
}
// 後
	if (IsBackIndex(index)) {
		Close();
		return;
	}
	...
void StageSelectMenu::OnCancel() {
	GameAudio::PlaySe(Se::UiBack);
	Close();
}

void StageSelectMenu::Close() {
	UIScreen::Close(screenName_); // 閉じると、タイトルの項目がまた動く
	isDecided_ = true;
}
```
（`...` の間の `const size_t stage = ...` から `LoadScene` までは変えない）

---

## パート G：`Source/Scripts/UI/TitleMenu.cpp`（2か所）

① include に1行
```cpp
#include "Source/Scripts/Audio/GameAudio.h"   // ← 足す
#include "Source/Scripts/GameTitle.h"
```

② `OnStart` の頭
```cpp
void TitleMenu::OnStart() {
	// --- BGM：鳴っていなければ鳴らす（起動したとき）。ゴールで小さくしていたら戻す（クリア画面から戻ったとき）---
	GameAudio::StartBgm();
	// --- ロゴにゲームの名前を書く（名前は GameTitle.h の1か所だけで変える）---
```

---

## パート W：風の強さで変わる風の音（直接入れた）

### 作り

- **聞くのはキューブが今いる所の風**（`IceCube::Update` で読む `wind_`）。Wind Zone ごとに Audio Source を付けると、重なった所で二重に鳴り、強さで変えるのに毎フレーム Inspector の値を書き換えることになる（下のログの話）ので、`GameAudio` が1か所で鳴らす。
- **風の強さ（0〜1）＝ 0.7 × 速さ ÷ 7 ＋ 0.3 × Strength ÷ 80**。速さは Wind Zone の Gust（ゆれ）込みなので、ゆれる風は音も大きくなったり小さくなったりする。

| 今のステージの風 | 強さ | 聞こえ方 |
|---|---|---|
| Speed 3・Strength 40・Gust 0.3（Level09・10 の横風） | 0.36〜0.55 でゆれる | 弱い風の音が中心で、強く吹いた時だけ強い音が少し混ざる |
| Speed 3.5・Strength 40・Gust 0.7（Level11） | 大きくゆれる | 強い音が出たり消えたりする（突風） |
| Speed 6・Strength 60（Level10 の上昇気流） | 約 0.82 | 強い風の音（高めの、ひゅうっと鳴る音）だけ |

- **2つの音を混ぜる**：`amb_wind_loop.wav`（弱い・低めのさあっ）と `amb_wind_strong_loop.wav`（強い・高めで、笛のようにひゅうっと鳴る音が混ざる。今回合成した）を、初めて風に入った時に2つとも鳴らし始めて、あとはずっと鳴らしたまま大きさだけ変える。強さ 0.35 から強い音が混ざり始め、0.85 で強い音だけ。混ぜるのは cos / sin（等パワーのクロスフェード）なので、途中で音が痩せない。
- **ゆっくり近づける**：聞こえる強さを、毎フレーム `1 − e^(−4 × 秒)` だけ今の強さへ近づける（約 0.5 秒でほぼ追いつく。フレームの長さが違っても同じ速さ）。風に入る・出るとき、ぷつっと切り替わらない。
- やられ・クリアの間は「風の外」と同じにして消していく。タイトルの `OnStart` で `MuteWind`（ポーズのまま風の中からタイトルへ戻ったとき用）。

### なぜ音の高さ（Pitch）ではなく、2つの音を混ぜるのか

エンジンの `SoundManager::SetVolume` と `SetPitch` は、呼ぶたびにログを1行出す。毎フレーム呼ぶと、1秒に60行ログが増える。ログを出さないのは `SetSpatial`（3D の音の大きさと左右）だけなので、**毎フレーム変えるのは SetSpatial の大きさだけ**にして、音色の違いは「2つの音の混ぜ具合」で作った（音の高さを変えるのと同じくらい、強さの違いが分かる）。

### コード（入れた所）

`IceCube::Update` の風を読んだ直後：
```cpp
	wind_ = WindZoneSystem::Sample(GetWorldPosition(transform));
	// --- 風の音：今いる所の風の速さ・強さで、大きさと音色を変える（やられ・クリアの間は風の外と同じに、消えていく）---
	GameAudio::UpdateWind(IsPlayable() ? Length(wind_.velocity) : 0.0f, IsPlayable() ? wind_.strength : 0.0f, deltaTime);
```
`GameAudio` に `UpdateWind`・`MuteWind`、決まった値（`kWindVolume`・`kFullWindSpeed`・`kFullWindStrength`・`kWindSpeedWeight`・`kWindStrongStart`・`kWindStrongRange`・`kWindFollowRate`）、`EnsureWindVoice`・`ApplyWindGains`（無名 namespace）。中身は `Source/Scripts/Audio/GameAudio.cpp` の「風」を見る。

### 変えたいとき

| こうしたい | 変える値（GameAudio.cpp） |
|---|---|
| 風の音全体を小さく | `kWindVolume` |
| 弱い風でも強い音を出したい | `kWindStrongStart` を下げる |
| 入った・出たときに、もっとすぐ変わる | `kWindFollowRate` を上げる |
| Strength の違いをもっと音に出す | `kWindSpeedWeight` を下げる（Strength の割合が増える） |

---

## 追記（2026-10-27）：円の音階を1段広げた・ステージ選択の Back

ユーザーの希望：「青い円に入ったときの音階の幅を、もう1段階広げたい。もっと低い音がほしい」「ステージ選択で、09〜の段で ↓ を押したら Back を選べるように」。どちらもソースへ直接入れた。

### 円の音階：ラ・ド・レ・ミ・ソ・ラ・ド（7段）

- **上には足せない**：エンジンは声を作るときに高さの上限を `XAUDIO2_DEFAULT_FREQ_RATIO`（2 倍）にしている（`SoundManager::PlayInternal` の `CreateSourceVoice`）。今の音階はもう 2.0（高いド）まで使っているので、それより上は鳴らない。
- **下に1段**：`se_ring_jump_low.wav` を新しく合成した。`se_ring_jump.wav` の和音（ド・ソ・高いド）を、ペンタトニックで1つ下（ラ・ミ・高いラ）にした音。風の音・低い「ぽん」はそのまま。高さの倍率 0.83 で鳴らすのではなく別に作ったのは、倍率を下げると風の音もゆっくりになって、1つ目だけ重く聞こえるから。
- `GameAudio.cpp`：`kRingPitches`（倍率の並び）を `kRingSteps`（`{鳴らす Se, 倍率}` の並び）にした。1つ目だけ `Se::RingJumpLow`、2つ目からは今までの `Se::RingJump` と倍率。`Se` に `RingJumpLow` を足したので `kSeFiles` にも足した（足し忘れは static_assert が止める）。

### ステージ選択の Back

- **原因**：ステージ選択は Columns 8・ステージ 11 個。Back は 12 番目の項目なので、格子では「09 の段の 4 列目」に数えられていた（見た目はマスの下なのに）。09〜11 で ↓ を押しても下の段が無いので動かず、11 で → を押すと Back に行っていた。
- **直し**：`MenuBase` に `virtual bool HasFooterItem() const`（最後の項目を、格子の外の下の項目として扱う）を足した。`StageSelectMenu` が Back がある時に true を返す。
  - 格子の一番下の段（09〜11）で ↓ → Back（どの列からでも）
  - 上の段で真下にマスが無い所（04〜08）で ↓ → 下の段の一番後ろ（11）。もう一度 ↓ で Back
  - Back で ↑ → 最後にいたマスへ戻る（`footerReturn_`。マウスで Back へ移ったときも）
  - → では Back へ行かない（格子に入れていないので）
- ほかのメニュー（タイトル・ポーズ・クリア）は `HasFooterItem` が false のままなので変わらない。

## 確かめること（写した後）

1. 製品版（Release）で起動 → タイトルで BGM が 2 秒かけて聞こえてくる。項目を動かす・決めると音がする。ステージ選択で、まだ遊べないマスを決めると「戻る」の音
2. Level で：ジャンプ・着地（高い所から落ちると大きく）・水色の円を続けて使う（**だんだん高くなる**）・緑の壁・緑の円
3. **落ちる** → 落ちた音と同時に BGM ががくっと低く・小さくなる → 幕 → 始まると BGM が元の高さで 1.5 秒かけて戻る（**頭からではなく続きから**）
4. **R** と **ポーズの Restart** も 3 と同じ
5. ゴール → ゴールの音、BGM が小さくなる → Next → 次のレベルで戻る
6. エディタで Level を直接 Play してやり直すと、BGM は**頭から**フェードインで鳴り直す（エンジンが止めるので。0章）
7. Level09〜11 で風に入る → 風の音がふわっと聞こえてくる。横風はゆれに合わせて大きさが変わる。Level10 の上昇気流は高い、ひゅうっという音になる。出ると消えていく

## 作業用コピーで確かめたこと（2026-10-27）

- Debug・Release のビルドが通った（警告は前からある物だけ）
- Release で起動して、ログで確かめた（作業用コピーにだけ「2 秒後に Level01 へ・3 秒後に R と同じやり直し」を入れた）：
  - 14 個の音が全部読めた（`登録完了 ... resources/Sounds/...`）
  - タイトル：BGM を大きさ 0 で鳴らして `FadeIn 2.000 → 0.600`
  - Level01 へ（LoadScene）：止まらずに `FadeOut 1.500 → 0.600`（そのまま）
  - やり直し：`SetPitch 0.840` → `FadeOut 0.400 → 0.120` → 読み直した後 `SetPitch 1.000` → `FadeOut 1.500 → 0.600`。**その間に「鳴っている音を全部止める」が無い**＝続きから戻っている
- 風（ソースへ入れた後、自分用の作業用コピーで Debug・Release をビルドし、IceCube に「3〜6 秒は Speed 3・Strength 40（ゆれ付き）、6〜9 秒は Speed 6・Strength 60」の仮の風を入れて Release のログで見た）：風の外 0 → 横風 0.36〜0.55 でゆれる（弱い音が中心）→ 上昇気流 0.825（強い音だけ。弱い音 0.009）→ 風を出ると約 0.5 秒で 0。風の音は初めて風に入った時に2つ鳴り始め、毎フレームのログは出ない
- **耳では確かめていない**（こちらでは音が聞こえないので）。沈み方が強すぎる・弱すぎるときは `kDownPitch`（1 に近いほど弱い）と `kDownVolume` を変える

## 困ったとき

| こうなった | 見る所 |
|---|---|
| リンクエラー（`GameAudio::PlaySe` が見つからない） | GameAudio.cpp が vcxproj に入っているか（VS で `Source/Scripts/Audio` に見えるか） |
| 風の音が鳴らない | `amb_wind_strong_loop.wav` があるか。Wind Zone の Enabled・Strength が 0 でないか |
| 音が全部鳴らない・ログに「ファイル読み込み失敗」 | `resources/Sounds/` に wav があるか（名前の綴り）。起動したフォルダが `project` か |
| BGM だけ鳴らない | Parameters の `Audio / Volume / BGM` が 0 になっていないか |
| BGM が二重に鳴る | どこかのシーンに BGM の Audio Source を置いていないか（BGM はコードで鳴らすので要らない） |
| やり直すと BGM が頭から鳴る | エディタで Level を直接 Play している（0章。タイトルから入れば続きから） |
| `static_assert` で止まる | `Se` に項目を足して、`kSeFiles` に足していない |
