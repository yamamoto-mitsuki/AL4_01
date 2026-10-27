#include "GameAudio.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <numbers>
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
    "resources/Sounds/se_ring_jump_low.wav",
    "resources/Sounds/se_launch.wav",
    "resources/Sounds/se_stick.wav",
    "resources/Sounds/se_land.wav",
    "resources/Sounds/se_shatter.wav",
    "resources/Sounds/se_fall.wav",
    "resources/Sounds/se_goal.wav",
    "resources/Sounds/se_crystal.wav",
    "resources/Sounds/se_crystal_burst.wav",
    "resources/Sounds/ui_move.wav",
    "resources/Sounds/ui_decide.wav",
    "resources/Sounds/ui_back.wav",
    "resources/Sounds/ui_open.wav",
});
// magic_enum::enum_count は enum の項目の数（Se の数）。足し忘れたら、ここでビルドが止まる
static_assert(kSeFiles.size() == magic_enum::enum_count<Se>(), "kSeFiles と Se の数が合っていません");
constexpr std::string_view kBgmFile = "resources/Sounds/bgm_forest_loop.wav";
constexpr std::string_view kWindWeakFile = "resources/Sounds/amb_wind_loop.wav";          // 弱い風（低めの、さあっという音）
constexpr std::string_view kWindStrongFile = "resources/Sounds/amb_wind_strong_loop.wav"; // 強い風（高めの、ひゅうっと鳴る音が混ざる）

// --- 水色の円：続けて使うと、ラ・ド・レ・ミ・ソ・ラ・ド と上がる（ペンタトニック。どこで止まっても濁らない）---
// pitch は元の高さに掛ける倍率（1.5 なら5度上＝ドに対するソ）。最後まで行ったら、高いドのまま
// 1つ目のラだけ、低く作った別の音（RingJumpLow）を使う。エンジンは元の高さの 2 倍までしか上げられない
// （声を作るときの上限が XAUDIO2_DEFAULT_FREQ_RATIO＝2）ので、ド〜高いドの1オクターブより上には足せない。下へ広げた
/// <summary>
/// 円の音階の1段
/// </summary>
struct RingStep {
	Se se;       // 鳴らす音
	float pitch; // 高さの倍率
};
constexpr auto kRingSteps = std::to_array<RingStep>({
    {Se::RingJumpLow, 1.0f},    // ラ（低く作った音をそのまま）
    {Se::RingJump, 1.0f},       // ド
    {Se::RingJump, 1.125f},     // レ
    {Se::RingJump, 1.25f},      // ミ
    {Se::RingJump, 1.5f},       // ソ
    {Se::RingJump, 1.6667f},    // ラ
    {Se::RingJump, 2.0f},       // 高いド（上限）
});

// --- BGM ---
constexpr float kBgmVolume = 0.6f;      // ふだんの大きさ（効果音より控えめに）
constexpr float kBgmFadeInTime = 2.0f;  // 無音から鳴らし始めるとき、ふだんの大きさになるまでの秒数
constexpr float kBgmRecoverTime = 1.5f; // 下げた音が、ふだんの大きさに戻るまでの秒数
constexpr float kDownPitch = 0.84f;     // やられ・やり直しの音の高さ（0.84 ≒ 3 半音下。がくっと沈んで聞こえる）
constexpr float kDownVolume = 0.2f;     // やられ・やり直しの大きさ（ふだんの何倍か）
constexpr float kDuckVolume = 0.35f;    // ゴールの大きさ（ふだんの何倍か）
constexpr float kMinFadeTime = 0.01f;   // フェードの秒数の下限（SoundManager が秒数で割るので 0 にしない）

// --- 風 ---
// 風の強さ（0〜1）＝ 速さの割合 × kWindSpeedWeight ＋ Strength の割合 × 残り
// 速さはゆれ（Gust）込みなので、ゆれる風は音も大きくなったり小さくなったりする
constexpr float kWindVolume = 0.8f;        // 風の音の大きさ（強さ 1 のとき。ここに強さが掛かる）
constexpr float kFullWindSpeed = 7.0f;     // この速さ（m/s）で、速さの割合が 1（今のステージは 3〜6。ゆれで ±30% など）
constexpr float kFullWindStrength = 80.0f; // この Strength（m/s²）で、Strength の割合が 1（今のステージは 40〜60）
constexpr float kWindSpeedWeight = 0.7f;   // 強さを決めるときの、速さの割合（残りが Strength）
constexpr float kWindStrongStart = 0.35f;  // 強さがここを超えると、強い風の音が混ざり始める
constexpr float kWindStrongRange = 0.5f;   // そこからこれだけ強くなると、強い風の音だけになる
constexpr float kWindFollowRate = 4.0f;    // 聞こえる強さが、今の風の強さへ近づく速さ（大きいほどすぐ変わる。4 なら約 0.5 秒でほぼ追いつく）
constexpr float kWindSilent = 0.001f;      // 聞こえる強さがこれより小さく、風の外なら 0 にする（いつまでも小さく残らないように）

/// <summary>
/// 読んだ音と、鳴っている BGM・風（この .cpp の中だけ）
/// </summary>
struct AudioState {
	std::array<uint32_t, kSeFiles.size()> seSounds{}; // 効果音の素材ハンドル（0 は読めなかった）
	uint32_t bgmSound = 0;  // BGM の素材ハンドル（0 は読めなかった）
	uint32_t bgmPlay = 0;   // 鳴っている BGM の再生ハンドル（止まったら SoundManager::IsPlaying が false）
	bool isBgmDown = false; // やられ・やり直しで、音を低くしている
	// --- 風（弱い音・強い音の2つ）---
	uint32_t windWeakSound = 0;   // 素材ハンドル（0 は読めなかった）
	uint32_t windStrongSound = 0; // 素材ハンドル（0 は読めなかった）
	uint32_t windWeakPlay = 0;    // 再生ハンドル（初めて風に入った時に鳴らし始めて、あとは大きさ 0 でも鳴らしたまま）
	uint32_t windStrongPlay = 0;  // 再生ハンドル
	float windPower = 0.0f;       // 今聞こえている風の強さ（0〜1。風の強さへゆっくり近づける）
	bool isLoaded = false;        // もう読んだ
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
	state.windWeakSound = SoundManager::Load(std::filesystem::path(kWindWeakFile));
	state.windStrongSound = SoundManager::Load(std::filesystem::path(kWindStrongFile));
}

// 風の音が鳴っていなければ、聞こえない大きさでループを鳴らし始める（エディタのやり直しでエンジンが止めたときも、ここで鳴らし直す）
void EnsureWindVoice(uint32_t sound, uint32_t& play) {
	if (sound == 0 || SoundManager::IsPlaying(play)) {
		return;
	}
	// 大きさ 0 で鳴らす → 3D の大きさ（gain）を 0 にする → 大きさを戻す、の順（ふだんの大きさで鳴らすと、0 にするまでの一瞬だけ聞こえる）
	// 毎フレーム変えるのは gain の方（SetSpatial はログを出さない。SetVolume は呼ぶたびにログを出す）
	play = SoundManager::Play(sound, true, 0.0f, 1.0f, AudioBus::SE);
	SoundManager::SetSpatial(play, 0.0f, 0.0f);
	SoundManager::SetVolume(play, kWindVolume);
}

// 聞こえる強さ（state.windPower）から、2つの音の大きさを決めて伝える
void ApplyWindGains() {
	// 強い風の音を混ぜる割合（0〜1）。なめらかに切り替わるように smoothstep（3t² − 2t³）を通す
	const float t = std::clamp((state.windPower - kWindStrongStart) / kWindStrongRange, 0.0f, 1.0f);
	const float mix = t * t * (3.0f - 2.0f * t);
	// cos と sin で分けると、混ぜている途中でも2つを合わせた音の大きさが変わらない（等パワーのクロスフェード）
	const float angle = mix * std::numbers::pi_v<float> * 0.5f;
	SoundManager::SetSpatial(state.windWeakPlay, state.windPower * std::cos(angle), 0.0f);
	SoundManager::SetSpatial(state.windStrongPlay, state.windPower * std::sin(angle), 0.0f);
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
	const size_t index = (chain <= 0) ? 0 : (std::min)(static_cast<size_t>(chain), kRingSteps.size() - 1);
	PlaySe(kRingSteps[index].se, kRingSteps[index].pitch);
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


//=============================================================================
// 風
//=============================================================================
void GameAudio::UpdateWind(float speed, float strength, float deltaTime) {
	LoadIfNeeded();
	// --- 今の風の強さ（0〜1。風の外なら 0）---
	float target = 0.0f;
	if (strength > 0.0f) {
		target = std::clamp(kWindSpeedWeight * speed / kFullWindSpeed + (1.0f - kWindSpeedWeight) * strength / kFullWindStrength, 0.0f, 1.0f);
	}
	// --- 聞こえる強さを、ゆっくり近づける（風に入る・出るとき、ぷつっと切り替わらないように）---
	// 1 − e^(−速さ×秒) だけ近づけると、フレームの長さが違っても同じ速さで近づく（60fps でも 144fps でも同じ）
	state.windPower += (target - state.windPower) * (1.0f - std::exp(-kWindFollowRate * deltaTime));
	if (target <= 0.0f && state.windPower < kWindSilent) {
		state.windPower = 0.0f;
	}
	// --- 鳴らす（まだ一度も風に入っていなければ、鳴らし始めない）---
	if (state.windPower > 0.0f) {
		EnsureWindVoice(state.windWeakSound, state.windWeakPlay);
		EnsureWindVoice(state.windStrongSound, state.windStrongPlay);
	}
	ApplyWindGains(); // 鳴っていない音には、SetSpatial が何もしない
}

void GameAudio::MuteWind() {
	state.windPower = 0.0f;
	ApplyWindGains();
}
