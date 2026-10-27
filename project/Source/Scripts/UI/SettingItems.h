#pragma once
#include <string>
#include <vector>

#include <MyEngine/ECS/GameObject.h>

/// <summary>
/// 設定の項目（音の大きさ・全画面）の動き（SettingsMenu から使う。タイトルの設定・ポーズの設定の画面は、どちらも SettingsMenu）
/// <para>項目は名前で見分ける：Master・SE・BGM（音の大きさ。VolumeKind の名前と同じ）/ Fullscreen（全画面）</para>
/// <para>値は、項目の直下の子の Value（Text）に書く。Value の色・光は、項目の Text（MenuBase が選んだ・外した飾りを書く）に合わせる</para>
/// <para>変えた設定は、すぐ当てて saves/Settings.json に書く（GameSettings）</para>
/// </summary>
namespace SettingItems {
/// <summary>
/// 値の書き方（On・Off の言葉と、値を書く Text の名前）
/// </summary>
struct ValueTexts {
	std::string valueName = "Value"; // 値を書く Text の名前（項目の直下の子）
	std::string onText = "On";       // 全画面のとき
	std::string offText = "Off";     // 全画面でないとき
};

// 設定の項目か（Master・SE・BGM・Fullscreen）
bool IsSettingItem(const std::string& itemName);
// ←→ で値を変える。音の大きさは端で止まる・全画面はどちらでも切り替える。変えたら true（設定の項目でなければ false）
bool Adjust(const std::string& itemName, int direction);
// 決めたときに値を変える。音の大きさは1段上げて、一番上の次は 0（マウスのクリックだけでも全部の段を選べるように）・全画面は切り替える
// 変えたら true（設定の項目でなければ false）
bool Decide(const std::string& itemName);
// items（子の Items）の下の設定の項目の Value に、今の値を書く（無い項目は飛ばす）
void WriteValues(const GameObject& items, const ValueTexts& texts);
// 項目の Value の色・光を、項目の Text に合わせる（選んだ項目は値も同じ色で光る。毎フレーム呼ぶ）
void MatchValueLooks(const std::vector<Handle<Entity>>& items, const std::string& valueName);
} // namespace SettingItems
