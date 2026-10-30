#include "DayTimeLook.h"

#include <array>
#include <cmath>
#include <string>

#include <externals/magic_enum/magic_enum.hpp>

#include <MyEngine/Components/Rendering3D/FogComponent.h>
#include <MyEngine/Components/Lighting/LightComponent.h>
#include <MyEngine/Components/Effects/ParticleSystemComponent.h>
#include <MyEngine/Components/Rendering3D/PolygonRendererComponent.h>
#include <MyEngine/Components/Rendering2D/SpriteRendererComponent.h>
#include <MyEngine/ECS/ComponentUI.h>
#include <MyEngine/ECS/EntityManager.h>
#include <MyEngine/ECS/Script.h>
#include <MyEngine/Math/MathUtility.h>

#include "Source/Scripts/Effects/AmbientParticles.h"

namespace {
// ===== 時間帯ごとの値（DayTime と同じ並び）=====
// シーンに置いた値（今のステージの見た目）を「昼の少し前」として作っているので、昼はほぼそのまま
constexpr auto kPresets = std::to_array<DayTimePreset>({
    // --- 朝：淡い空・少し暖かい白の光・霧も少し暖かく ---
    {
     .skyTexture = "resources/Textures/Forest/sky_morning.png",
     .skyTint = {1.0f, 1.0f, 1.0f},
     .sceneryTint = {0.95f, 0.96f, 1.0f},
     .lightTint = {1.0f, 0.94f, 0.86f},
     .lightIntensity = 1.0f,
     .fogTint = {1.0f, 0.97f, 0.96f},
     .heightFogTint = {1.0f, 1.0f, 1.0f},
     .lampTint = {1.0f, 1.0f, 1.0f},
     .lampIntensity = 1.0f,
     .particleTint = {1.0f, 1.0f, 1.0f},
     .particleIntensity = 1.0f,
     .terrainIntensity = 1.0f,
     },
    // --- 昼：青い空・明るい白の光。漂う粒は明るさに紛れるので控えめ ---
    {
     .skyTexture = "resources/Textures/Forest/sky_day.png",
     .skyTint = {1.0f, 1.0f, 1.0f},
     .sceneryTint = {1.0f, 1.0f, 1.0f},
     .lightTint = {1.0f, 1.0f, 0.97f},
     .lightIntensity = 1.25f,
     .fogTint = {0.96f, 1.0f, 1.0f},
     .heightFogTint = {1.0f, 1.0f, 1.0f},
     .lampTint = {1.0f, 1.0f, 1.0f},
     .lampIntensity = 0.8f,
     .particleTint = {1.0f, 1.0f, 1.0f},
     .particleIntensity = 0.6f,
     .terrainIntensity = 1.0f,
     },
    // --- 夕方：元の夕焼けの空・橙の光・木は赤みのある影・霧も橙に ---
    {
     .skyTexture = "",
     .skyTint = {1.0f, 0.92f, 0.85f},
     .sceneryTint = {0.82f, 0.66f, 0.6f},
     .lightTint = {1.0f, 0.66f, 0.42f},
     .lightIntensity = 1.1f,
     .fogTint = {1.0f, 0.74f, 0.6f},
     .heightFogTint = {0.85f, 0.6f, 0.55f},
     .lampTint = {1.0f, 0.85f, 0.7f},
     .lampIntensity = 1.0f,
     .particleTint = {1.0f, 0.8f, 0.55f},
     .particleIntensity = 1.1f,
     .terrainIntensity = 1.0f,
     },
    // --- 夜：星の空・青く暗い光・木は濃い青の影・霧も暗い青。ライトは暖かく少し強く、漂う粒は蛍のように光る。足場・壁は真っ暗で見えないので明るく ---
    {
     .skyTexture = "resources/Textures/Forest/sky_night.png",
     .skyTint = {1.0f, 1.0f, 1.0f},
     .sceneryTint = {0.24f, 0.28f, 0.42f},
     .lightTint = {0.5f, 0.6f, 1.0f},
     .lightIntensity = 0.6f,
     .fogTint = {0.2f, 0.25f, 0.42f},
     .heightFogTint = {0.22f, 0.25f, 0.42f},
     .lampTint = {1.0f, 0.9f, 0.75f},
     .lampIntensity = 1.2f,
     .particleTint = {1.0f, 0.95f, 0.5f},
     .particleIntensity = 1.8f,
     .terrainIntensity = 6.0f,
     },
});
// magic_enum::enum_count は enum の項目の数（DayTime の数）。時間帯を足して表を足し忘れたら、ここでビルドが止まる
static_assert(kPresets.size() == magic_enum::enum_count<DayTime>(), "kPresets と DayTime の数が合っていません");

// ===== 板の絵の見分け方（絵のパスにこの言葉が入っているか）=====
constexpr std::string_view kSkyKeyword = "sky";       // 空の板（sky.png。上下反転した Sky_Top も同じ絵）
constexpr std::string_view kSceneryKeyword = "trees"; // 木の板（trees_far.png・trees_near.png）

// 色を成分ごとに掛ける
Vector3 Multiply(const Vector3& a, const Vector3& b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
Vector4 Multiply(const Vector4& a, const Vector3& b) { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w}; } // A（透け具合）はそのまま
} // namespace

//=============================================================================
// 時間帯の値
//=============================================================================
const DayTimePreset& GetDayTimePreset(DayTime time) { return kPresets[magic_enum::enum_index(time).value_or(0)]; }

DayTimePreset LerpDayTimePreset(const DayTimePreset& a, const DayTimePreset& b, float t) {
	return {
	    .skyTexture = a.skyTexture,
	    .skyTint = MathUtility::Lerp(a.skyTint, b.skyTint, t),
	    .sceneryTint = MathUtility::Lerp(a.sceneryTint, b.sceneryTint, t),
	    .lightTint = MathUtility::Lerp(a.lightTint, b.lightTint, t),
	    .lightIntensity = std::lerp(a.lightIntensity, b.lightIntensity, t),
	    .fogTint = MathUtility::Lerp(a.fogTint, b.fogTint, t),
	    .heightFogTint = MathUtility::Lerp(a.heightFogTint, b.heightFogTint, t),
	    .lampTint = MathUtility::Lerp(a.lampTint, b.lampTint, t),
	    .lampIntensity = std::lerp(a.lampIntensity, b.lampIntensity, t),
	    .particleTint = MathUtility::Lerp(a.particleTint, b.particleTint, t),
	    .particleIntensity = std::lerp(a.particleIntensity, b.particleIntensity, t),
	    .terrainIntensity = std::lerp(a.terrainIntensity, b.terrainIntensity, t),
	};
}

//=============================================================================
// 置いた値を覚える
//=============================================================================
void DayTimeLook::Capture() {
	*this = DayTimeLook{};
	// ===== 平行光源 =====
	EntityManager::ForEach<DirectionalLightComponent>([this](Handle<Entity> entity, const DirectionalLightComponent& light) {
		directionalLights_.push_back({.entity = entity, .color = light.color, .intensity = light.intensity});
	});
	// ===== 空・木の板（絵のパスで見分ける）=====
	EntityManager::ForEach<SpriteRendererComponent>([this](Handle<Entity> entity, const SpriteRendererComponent& sprite) {
		const std::string& path = GetAssetPath(AssetType::Texture, sprite.textureHandle);
		if (path.find(kSkyKeyword) != std::string::npos) {
			skySprites_.push_back({.entity = entity, .color = sprite.color, .texture = sprite.textureHandle});
		} else if (path.find(kSceneryKeyword) != std::string::npos) {
			scenerySprites_.push_back({.entity = entity, .color = sprite.color});
		}
	});
	// ===== 霧 =====
	EntityManager::ForEach<FogComponent>([this](Handle<Entity> entity, const FogComponent& fog) {
		fogs_.push_back({.entity = entity, .color = fog.color, .heightColor = fog.heightColor});
	});
	// ===== ポイントライト・スポットライト =====
	EntityManager::ForEach<PointLightComponent>([this](Handle<Entity> entity, const PointLightComponent& light) {
		pointLights_.push_back({.entity = entity, .color = light.color, .intensity = light.intensity});
	});
	EntityManager::ForEach<SpotLightComponent>([this](Handle<Entity> entity, const SpotLightComponent& light) {
		spotLights_.push_back({.entity = entity, .color = light.color, .intensity = light.intensity});
	});
	// ===== 足場・壁（Polygon Renderer。ギミックは Primitive Renderer なので変えない）=====
	EntityManager::ForEach<PolygonRendererComponent>([this](Handle<Entity> entity, const PolygonRendererComponent& renderer) {
		terrains_.push_back({.entity = entity, .intensity = renderer.material.intensity});
	});
	// ===== 漂う粒（AmbientParticles の付いた Particle System だけ。ギミックの粒の色は変えない）=====
	for (const AmbientParticles* ambient : Script::FindObjectsOfType<AmbientParticles>()) {
		if (const ParticleSystemComponent* particle = ambient->GetComponent<ParticleSystemComponent>()) {
			particles_.push_back({.entity = ambient->GetGameObject().handle, .colorA = particle->colorA, .colorB = particle->colorB, .intensity = particle->intensity});
		}
	}
}

//=============================================================================
// 時間帯の色を掛ける
//=============================================================================
void DayTimeLook::Apply(const DayTimePreset& preset) const {
	for (const Light& base : directionalLights_) {
		if (DirectionalLightComponent* light = EntityManager::Get<DirectionalLightComponent>(base.entity)) {
			light->color = Multiply(base.color, preset.lightTint);
			light->intensity = base.intensity * preset.lightIntensity;
		}
	}
	for (const SkySprite& base : skySprites_) {
		if (SpriteRendererComponent* sprite = EntityManager::Get<SpriteRendererComponent>(base.entity)) {
			sprite->color = Multiply(base.color, preset.skyTint);
		}
	}
	for (const ColoredSprite& base : scenerySprites_) {
		if (SpriteRendererComponent* sprite = EntityManager::Get<SpriteRendererComponent>(base.entity)) {
			sprite->color = Multiply(base.color, preset.sceneryTint);
		}
	}
	for (const Fog& base : fogs_) {
		if (FogComponent* fog = EntityManager::Get<FogComponent>(base.entity)) {
			fog->color = Multiply(base.color, preset.fogTint);
			fog->heightColor = Multiply(base.heightColor, preset.heightFogTint);
		}
	}
	for (const Light& base : pointLights_) {
		if (PointLightComponent* light = EntityManager::Get<PointLightComponent>(base.entity)) {
			light->color = Multiply(base.color, preset.lampTint);
			light->intensity = base.intensity * preset.lampIntensity;
		}
	}
	for (const Light& base : spotLights_) {
		if (SpotLightComponent* light = EntityManager::Get<SpotLightComponent>(base.entity)) {
			light->color = Multiply(base.color, preset.lampTint);
			light->intensity = base.intensity * preset.lampIntensity;
		}
	}
	for (const Terrain& base : terrains_) {
		if (PolygonRendererComponent* renderer = EntityManager::Get<PolygonRendererComponent>(base.entity)) {
			renderer->material.intensity = base.intensity * preset.terrainIntensity;
		}
	}
	for (const Particle& base : particles_) {
		if (ParticleSystemComponent* particle = EntityManager::Get<ParticleSystemComponent>(base.entity)) {
			particle->colorA = Multiply(base.colorA, preset.particleTint);
			particle->colorB = Multiply(base.colorB, preset.particleTint);
			particle->intensity = base.intensity * preset.particleIntensity;
		}
	}
}

uint32_t DayTimeLook::LoadSkyTexture(DayTime time, uint32_t original) {
	const std::string_view path = GetDayTimePreset(time).skyTexture;
	return path.empty() ? original : LoadAsset(AssetType::Texture, std::string(path));
}

Vector3 DayTimeLook::GetHazeColor(const DayTimePreset& preset) const {
	return fogs_.empty() ? Vector3{1.0f, 1.0f, 1.0f} : Multiply(fogs_.front().color, preset.fogTint);
}
