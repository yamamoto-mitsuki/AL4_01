#include "GameSettings.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

#include <externals/magic_enum/magic_enum.hpp>
#include <externals/nlohmann/json.hpp>

#include <MyEngine/Diagnostics/LogManager.h>
#include <MyEngine/Engine.h>
#include <MyEngine/Systems/AudioSystem.h>

namespace {
constexpr const char* kFilePath = "saves/Settings.json"; // 保存の場所（Progress.json と同じフォルダ）
constexpr int kFileVersion = 1;                          // ファイルの形の版（形を変えたら上げる）

// つまみの名前（ファイルの項目名。"Master" / "BGM" / "SE"。つまみを足しても、ここは直さなくてよい）
std::string KindName(VolumeKind kind) { return std::string(magic_enum::enum_name(kind)); }

// つまみの今の音量（0〜1。Master はエンジンの全体の音量、ほかは音の種類ごとの音量）
float GetVolume(VolumeKind kind) {
	switch (kind) {
	case VolumeKind::BGM:
		return AudioSystem::GetBusVolume(AudioBus::BGM);
	case VolumeKind::SE:
		return AudioSystem::GetBusVolume(AudioBus::SE);
	default:
		return AudioSystem::GetMasterVolume();
	}
}

// つまみの音量を当てる（鳴っている音へは、次の AudioSystem::Update で伝わる）
void SetVolume(VolumeKind kind, float volume) {
	switch (kind) {
	case VolumeKind::BGM:
		AudioSystem::SetBusVolume(AudioBus::BGM, volume);
		break;
	case VolumeKind::SE:
		AudioSystem::SetBusVolume(AudioBus::SE, volume);
		break;
	default:
		AudioSystem::SetMasterVolume(volume);
		break;
	}
}

// 段 → 音量（0〜1）
float StepToVolume(int step) { return static_cast<float>(step) / static_cast<float>(GameSettings::kVolumeSteps); }

// メインのウィンドウ（無ければ nullptr）
Win32Window* GetWindow() { return Engine::GetWindowManager()->GetMainWindow(); }

// 今の設定を全部ファイルに書く
void Save() {
	nlohmann::json json = nlohmann::json::object();
	json["version"] = kFileVersion;
	nlohmann::json& volumes = json["volumeSteps"];
	for (VolumeKind kind : magic_enum::enum_values<VolumeKind>()) {
		volumes[KindName(kind)] = GameSettings::GetVolumeStep(kind);
	}
	json["fullscreen"] = GameSettings::IsFullscreen();

	// --- フォルダが無ければ作って、書く ---
	std::error_code error;
	std::filesystem::create_directories(std::filesystem::path(kFilePath).parent_path(), error);
	std::ofstream stream(kFilePath);
	if (!stream) {
		LogManager::Warning(std::string(kFilePath) + " に書けませんでした");
		return;
	}
	stream << json.dump(4) << '\n';
}
} // namespace

//=============================================================================
// 読む
//=============================================================================
void GameSettings::Load() {
	std::ifstream stream(kFilePath);
	if (!stream) {
		return; // まだ無い（初めての起動）：エンジンの音量のまま・ウィンドウのまま
	}
	const nlohmann::json json = nlohmann::json::parse(stream, nullptr, false); // false：壊れていても例外を投げない
	if (json.is_discarded() || !json.is_object()) {
		LogManager::Warning(std::string(kFilePath) + " が読めないので、設定は初めのままにします");
		return;
	}
	// --- 音の大きさ（書いてあるつまみだけ当てる。Master の無い古いファイルなら、全体はエンジンの音量のまま）---
	if (const auto volumes = json.find("volumeSteps"); volumes != json.end() && volumes->is_object()) {
		for (VolumeKind kind : magic_enum::enum_values<VolumeKind>()) {
			const auto step = volumes->find(KindName(kind));
			if (step != volumes->end() && step->is_number_integer()) {
				SetVolume(kind, StepToVolume(std::clamp(step->get<int>(), 0, kVolumeSteps)));
			}
		}
	}
	// --- 全画面（製品版だけ）---
#ifndef USE_IMGUI
	if (const auto fullscreen = json.find("fullscreen"); fullscreen != json.end() && fullscreen->is_boolean()) {
		if (Win32Window* window = GetWindow()) {
			window->SetFullscreen(fullscreen->get<bool>());
		}
	}
#endif
}

//=============================================================================
// 音の大きさ
//=============================================================================
int GameSettings::GetVolumeStep(VolumeKind kind) {
	// エンジンの音量（Parameters で 0.75 などにしていても）を、一番近い段にする
	const int step = static_cast<int>(std::lround(GetVolume(kind) * kVolumeSteps));
	return std::clamp(step, 0, kVolumeSteps);
}

void GameSettings::SetVolumeStep(VolumeKind kind, int step) {
	SetVolume(kind, StepToVolume(std::clamp(step, 0, kVolumeSteps)));
	Save();
}

//=============================================================================
// 全画面
//=============================================================================
bool GameSettings::IsFullscreen() {
	const Win32Window* window = GetWindow();
	return window != nullptr && window->GetIsFullscreen();
}

void GameSettings::SetFullscreen(bool fullscreen) {
	if (Win32Window* window = GetWindow()) {
		window->SetFullscreen(fullscreen);
	}
	Save();
}
