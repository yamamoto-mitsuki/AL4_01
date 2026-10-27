#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

#include <MyEngine/Core/Handle.h>
#include <MyEngine/ECS/Entity.h>
#include <MyEngine/Math/Vector3.h>
#include <MyEngine/Math/Vector4.h>

/// <summary>
/// 時間帯（朝・昼・夕方・夜）
/// </summary>
enum class DayTime : uint8_t {
	Morning, // 朝：淡い空・少し暖かい白の光
	Noon,    // 昼：青い空・明るい白の光
	Evening, // 夕方：夕焼けの空（元の sky.png）・橙の光・暖かい霧
	Night,   // 夜：星の空・青く暗い光・蛍のような粒
};

/// <summary>
/// 時間帯1つ分の見た目。シーンに置いた値に掛ける（色は成分ごとに掛ける。1 ならそのまま）
/// </summary>
struct DayTimePreset {
	std::string_view skyTexture; // 空の板に使う絵（空の文字なら替えない＝元の sky.png の夕焼け）
	Vector3 skyTint;             // 空の板の色に掛ける
	Vector3 sceneryTint;         // 木の板（trees）の色に掛ける（夜は暗い影の色）
	Vector3 lightTint;           // 平行光源の色に掛ける
	float lightIntensity;        // 平行光源の強さに掛ける
	Vector3 fogTint;             // 距離の霧の色に掛ける
	Vector3 heightFogTint;       // 高さの霧の色に掛ける
	Vector3 lampTint;            // ポイントライト・スポットライトの色に掛ける
	float lampIntensity;         // ポイントライト・スポットライトの強さに掛ける
	Vector3 particleTint;        // 漂う粒（AmbientParticles）の色に掛ける
	float particleIntensity;     // 漂う粒の明るさに掛ける（夜は蛍のように強く光らせる）
	float terrainIntensity;      // 足場・壁（Polygon Renderer）の明るさ（Intensity）に掛ける（夜は暗くて見えないので強く）
};

// time の見た目（DayTimeLook.cpp の kPresets の値）
const DayTimePreset& GetDayTimePreset(DayTime time);
// a と b を t（0：a 〜 1：b）で混ぜる。空の絵は混ぜられないので a の物（TimeOfDayCycle が霧の色を通って替える）
DayTimePreset LerpDayTimePreset(const DayTimePreset& a, const DayTimePreset& b, float t);

/// <summary>
/// シーンの見た目（平行光源・空の板・木の板・霧・ライト・漂う粒・足場と壁）の、置いた値を覚えて、時間帯の色を掛ける
/// <para>Capture で置いた値を覚え、Apply で「覚えた値 × 時間帯の色」を書く。何回 Apply しても、置いた値から掛け直す（掛けた上に掛けない）</para>
/// <para>どの板かは絵のパスで見分ける（sky が入っていれば空・trees が入っていれば木。名前は Sky・Sky2・Sky_Top などばらばらなので）</para>
/// <para>空の板の絵は Apply では替えない（使う側が GetSkySprites で替える。TimeOfDay は1回だけ・TimeOfDayCycle は霧の色を通って替える）</para>
/// <para>Script のメンバーに持つ（Component ではない。保存しない）</para>
/// </summary>
class DayTimeLook {
public:
	/// <summary>
	/// 覚えた空の板1枚分
	/// </summary>
	struct SkySprite {
		Handle<Entity> entity; // 板の Entity
		Vector4 color;         // 置いた色
		uint32_t texture = 0;  // 置いた絵（夕方は、この絵のまま）
	};

	// 今のシーンの置いた値を覚える（ゲームが始まった時に1回。前に覚えた物は捨てる）
	void Capture();
	// 覚えた値に preset の色を掛けて書く（空の絵は替えない。消えた Entity は飛ばす）
	void Apply(const DayTimePreset& preset) const;
	// time の空の絵（Preset の Sky Texture を読む。空の文字なら original）
	static uint32_t LoadSkyTexture(DayTime time, uint32_t original);
	const std::vector<SkySprite>& GetSkySprites() const { return skySprites_; }
	// 霧の色に preset を掛けた色（距離の霧。霧が無ければ白）。空の絵を替えるときに、いったんこの色へ寄せる（TimeOfDayCycle）
	Vector3 GetHazeColor(const DayTimePreset& preset) const;

private:
	/// <summary>
	/// 色だけ掛ける板（木）
	/// </summary>
	struct ColoredSprite {
		Handle<Entity> entity;
		Vector4 color;
	};
	/// <summary>
	/// 光（平行光源・ポイントライト・スポットライト）
	/// </summary>
	struct Light {
		Handle<Entity> entity;
		Vector3 color;
		float intensity = 1.0f;
	};
	/// <summary>
	/// 霧（距離・高さ）
	/// </summary>
	struct Fog {
		Handle<Entity> entity;
		Vector3 color;
		Vector3 heightColor;
	};
	/// <summary>
	/// 漂う粒（AmbientParticles の付いた Particle System）
	/// </summary>
	struct Particle {
		Handle<Entity> entity;
		Vector4 colorA;
		Vector4 colorB;
		float intensity = 1.0f;
	};
	/// <summary>
	/// 足場・壁（Polygon Renderer）
	/// </summary>
	struct Terrain {
		Handle<Entity> entity;
		float intensity = 1.0f;
	};

	std::vector<Light> directionalLights_;      // 平行光源
	std::vector<SkySprite> skySprites_;         // 空の板
	std::vector<ColoredSprite> scenerySprites_; // 木の板
	std::vector<Fog> fogs_;                     // 霧
	std::vector<Light> pointLights_;            // ポイントライト
	std::vector<Light> spotLights_;             // スポットライト
	std::vector<Terrain> terrains_;             // 足場・壁
	std::vector<Particle> particles_;           // 漂う粒
};
