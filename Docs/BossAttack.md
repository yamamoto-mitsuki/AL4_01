# BossAttack：ボスの攻撃・プレイヤーの被弾・ジャスト回避の入口

参照・参考元
- 設計書：`C:\Users\K025G\Downloads\ShooterGameDesign.md`（4章 回避ジャンプとジャスト回避・5章 ボス・9章 UI。2026-11-06 に確定）
- エンジンの攻撃の当たりとダメージ：`HTTEngine/Docs/Tasks/HitDamage.md`（0.3 使い方）
- エンジンの床の予告：`HTTEngine/Docs/Tasks/MaterialTelegraph.md`
- エンジンの攻撃表：`HTTEngine/Docs/Tasks/AttackTable.md`
- エンジンの待ちの部品・アニメを流す窓口・角度の計算：エンジンの 24e4d2b・86df43b（アニメを名前で選ぶ）の md
- エンジンの見本：`HTTEngine/project/Sandbox/Source/AttackParts/ChaseTarget.cpp`・`SpinAttack.cpp`、`Sandbox/Source/Scripts/HitTestPlayer`
- 前の作業：`Docs/PlayerMove.md`（PlayerController）
- Unreal の Behavior Tree（考える所＝AI と、動く所＝体を分ける。待つ・近づくも AI の仕事）：https://dev.epicgames.com/documentation/en-us/unreal-engine/behavior-tree-in-unreal-engine---overview
- フレームの速さに左右されない近づけ方（`1 - e^(-dt / smoothTime)`）：Freya Holmér「Lerp smoothing is broken」 https://www.youtube.com/watch?v=LSNQuFEDOyQ
- 放物線の高さの式（`4H × s(1 − s)`。頂点が s ＝ 0.5 で H になる2次式）：https://en.wikipedia.org/wiki/Parabola
- ベヨネッタのウィッチタイム（ジャスト回避のたびに反撃に入る形）：https://bayonetta.fandom.com/wiki/Witch_Time

ブランチは `feature/BossAttack`（develop から）。エンジンは 86df43b（待ちの部品・アニメを名前で流す・角度の計算が入った版）。

---

## 0. 進め方

| パート | やること | 書き方 |
|---|---|---|
| 0 | ボスのアニメの名前から、頭の番号を外す | Blender（プレイヤーと同じ） |
| A | `PlayerController`：被弾・倒れる・ジャスト回避の入口・回避の待ち時間の割合。エンジンの角度・なめらかさの道具に置き換え | **全部差し替え** |
| B | `JustCounter`：ジャスト反撃の流れ（今は記録とログだけ） | 新しいファイル |
| C | `GroundAttackPart`：地面を動く敵の部品の道具（向く・止まる・アニメを段階の秒で流す） | 新しいファイル |
| D | 攻撃部品 `Chase`（待ち）・`StationaryAttack`（その場の攻撃）・`Pounce`（飛びかかり） | 新しいファイル |
| E | `BossCat`：ボスの体（顔の表情・怯み・移行・撃破のアニメ） | 新しいファイル |
| E2 | `HitBlink`：被弾の後の無敵の間、モデルを点滅させる | 新しいファイル |
| F | VS に足して、ビルドする | VS |
| G | レイヤー・プレイヤー・ボス・予告の Prefab・攻撃表・仮の台地 | エディタ |
| H | 動かして確かめる | Play |

A〜E2 のコードは、作業用コピーで Debug・Development・Release のビルドが通る（このコードからの警告は無い）ことを確かめた。動きの確かめは H。

### 0.1 2026-11-07 の追加（A〜E を写した後に足す分）

A〜E を写した後に、点滅と Inspector の説明を足した。写した後のコード（自分で付けた名前・コメント）に合わせてある。

| どこ | やること |
|---|---|
| `PlayerController.h` | `hitChangeAlphaTime_` の行を消す（点滅は E2 の `HitBlink` が持つ） → **済み**（2026-11-07 に直接消した） |
| `PlayerController.cpp` | `Update` の「無敵時間の点滅のコード」のコメントを消す。最後の `Fields` を **A-2 の最後** のとおりに書く → **済み**（2026-11-07 に直接入れた） |
| `JustCounter.h` | 今の短い形（`Begin` がログを出すだけ）のままなら、`void Fields(ComponentUI& ui) override;` の行を消す（出す値が無い。宣言だけ残すとリンクエラー）。B の形に戻すなら B のとおり → **済み**（2026-11-07 に直接消した） |
| `Chase.cpp`・`StationaryAttack.cpp`・`Pounce.cpp` | 最後の `Fields` を **D-1・D-2・D-3 の最後** のとおりに書く → **済み**（2026-11-07 に直接入れた。3構成でビルドが通る） |
| E2 | `HitBlink.h`・`HitBlink.cpp` を作る → **済み**（2026-11-07 に直接入れた。中身は E2 のとおり） |
| F | フィルター `Source/Scripts/Combat` を作り、`HitBlink` を入れる → **済み**（vcxproj・filters に足した） |
| `Pounce.cpp` | `leapSeconds_` のコメントの「空中**の**いる秒数」は「空中**に**いる秒数」に直す（Tip は直した文にしてある） → **済み** |
| G-2 | プレイヤーに `HitBlink` を足す。Health の `Invincible After Hit` は `Hit Stun Time` より長くする → **済み**（エディタで） |
| G-5・G-6 | **済み**（2026-11-07 に GameScene・攻撃表・Prefab の JSON へ直接書いた。下の「G-5・G-6 で直接書いたこと」） |

**G-5・G-6 で直接書いたこと**（G-1〜G-4 と、攻撃表の Phase 1・Pounce の数値はエディタで作った物をそのまま使った）

| ファイル | 書いたこと |
|---|---|
| `resources/attackTable/Boss.attacktable` | Pounce の Impact Prefab（`resources/prefabs/PounceImpact.prefab`）と4つの Clip を入れた。Tail Sweep の行を足した（G-5 の表のとおり） |
| `resources/scenes/GameScene.scene.json` | Boss に Attack Table（Table＝Boss.attacktable・Target＝Player）を足した。Boss を (6, 3, 0)・回転 (0, −90, 0)、Player を (−30, 0, 0)・回転 (0, 90, 0) にした。G-6 の `Boss Plateau`・`Boss Slope`・`Boss Wall N/S/E` と、坂の口を空けた `Boss Wall W (+Z)`・`Boss Wall W (-Z)` を足した。シーンに残っていた `PounceImpact`（Prefab を作った元）を消した（G-4 の 3） |
| `Tail Sweep Hitbox`（シーン）・`PounceImpact.prefab` | **Just Margin が 0 だったので 0.5 にした**（0 だとジャスト判定が無く、ジャスト回避を確かめられない） |

Targets は `-6`（Default と Enemy 以外の全部）で、Player に当たるのでそのままにした。G-3 の表の通りにするなら、Player だけを選ぶ。

作業用コピーの Release で GameScene を動かして、読み込みの警告なし → ボスが坂を降りて近づく → しっぽ4回・飛びかかり1回が当たり、残り 0 で倒れる、まで確かめた（入力はしていない）。

Fields の `Tip`（欄にマウスを乗せたときの説明）は、ヘッダーで変数の右に書いたコメントと同じ文にした。

**被弾の後の無敵の長さ**：PlayerController の `IsInvincible()` は回避の無敵だけを見ている。被弾の後の無敵は Health の **Invincible After Hit** が持ち、エンジンの DamageSystem が**当たった瞬間から**数える。のけぞり（Hit Stun Time）より長くすれば、その差の分だけ「動けるのに無敵」の時間ができる。

```
当たる ─ のけぞり（Hit Stun Time 0.4 秒）─ 動ける ─────── 無敵が切れる
       |←────────── Invincible After Hit 1.0 秒 ──────────→|
                                        |← 動けて無敵 0.6 秒 →|
                                  ↑ この間ずっと点滅（HitBlink）
```

---

## 1. 決めたこと

| 決めたこと | 理由 |
|---|---|
| ボスの攻撃していない間の動き（向く・遠ければ近づく）は、攻撃表のフェーズの **Idle Part** に `Chase` を選ぶ。攻撃の間の「間」は **Min Idle Seconds** | 「いつ攻撃するか」も「いつ近づくか」もボスの頭（AI）の仕事。Script で状態を見て近づける形だと、頭が2か所に分かれ、攻撃が始まったフレームに歩きの速さが残る |
| ジャスト反撃の入口は、別の Script **`JustCounter`**。PlayerController は回避ジャンプを始めた瞬間に `HitSystem::TryConsumeJust` → 成功したら `JustCounter::Begin(just)` を呼ぶだけ | 反撃の中身（スロー・カメラ・輪・判定・UI・ゲージ）は流れの仕切り役、撃つのは体。1つのクラスに入れると、時間・カメラ・UI が体のクラスに入る |
| 被弾：のけぞり（`PlayerState::Hit`）は **Hit Stun Time** 秒、操作なし。Hit のアニメをその秒で流しきる。続けて当たったら最初から | 当たったことがはっきり分かる。回避で取り消せると被弾の損が無くなる |
| 被弾：当たった向き（地面に平ら）へ **Hit Knockback Distance**（m）だけ押す。のけぞりの秒の間に、出だしが速くだんだん止まる形（OutQuad）でちょうどその距離を進む | 円の攻撃では向きが「円の真ん中から外へ」なので、円の外へ押されて続けて当たりにくい。**相談では「速さ 5 m/秒を Acceleration のなめらかさで 0 へ」だったが、動かすと 0.25 m しか押されなかった**ので、回避ジャンプと同じ「距離で決める」形に変えた |
| 被弾の後の無敵は Health の **Invincible After Hit**。PlayerController には持たない | 値が2か所に分かれない |
| 被弾の後の無敵の間は、別の Script **`HitBlink`** が Model Renderer の **Enabled** を切り替えて点滅させる（消える・出る）。回避の無敵では点滅しない。倒れたら点滅をやめて表示に戻す | 半透明（Color の A）は Depth Mode を TestNoWrite にしないと透けず、その間は体の奥の部品が手前に透ける。点滅は見た目の仕事なので操作（PlayerController）と分け、Health を持つほかの物にもそのまま付けられる |
| 空中（回避ジャンプ中）で当たったら、回避をやめて Hit。縦の速さはそのまま。回避の待ち時間は入れない | 着地は狙われる、の仕様のまま。のけぞりが罰なので、待ち時間まで重ねない |
| HP 0 で `PlayerState::Down`。Down のアニメを1回、その後ずっと操作なし、ログに「倒れた」 | ゲームオーバー・やり直しはまだ決まっていない |
| HP はハート＝ Health の Max Health（5）。減る量は攻撃ごとの Damage Source の Damage | 数を Inspector で変えられる。減る量は攻撃の Entity が持つ |
| 飛びかかり：**Leap Seconds（空中の秒）と Leap Height（高さ）を別々の値**にする。空中はボスの Character Controller の Use Gravity を切り、放物線の上の位置の差を速さにして縦も横も書く。着地・OnEnd で戻す | 空中の秒はジャスト回避の読みやすさ、高さは見た目、と目的が違う。重力から出すと、長く浮かせると高く跳んでしまう。放物線の上を正確に進むので、着地が攻撃の段階の頭とぴったり合う |
| かがむ秒 ＝ 攻撃表の Windup Seconds − Leap Seconds | 攻撃表の段階は Windup・Active・Recovery の3つのまま |
| 着地点は跳ぶ瞬間のプレイヤーの位置（地面に平ら・高さはボスの床）。先読みしない。遠ければ **Max Leap Distance** の所まで（調整する） | 円を見てから避ける遊び |
| かがむ間はプレイヤーへ向き続け、跳ぶ瞬間に着地点の向きへぴったり合わせる | 向きで跳ぶ先が読める |
| 飛びかかりの当たりは、着地点に置く **予告の Prefab**（Hitbox の Circle・Damage Source・Hitbox Telegraph）。部品が攻撃の始めに作り、跳ぶ瞬間に着地点へ動かして予告し、終わったら消す | 子の Hitbox だと、空中のネコと一緒に予告が動く。Prefab から作った Entity の Component は次のフレームの頭に付くので、跳ぶ瞬間ではなく攻撃の始めに作っておく |
| しっぽは汎用の部品 **`StationaryAttack`**（段階ごとのアニメの名前・子の Hitbox の名前を攻撃表に書く） | その場の技は、行を足して値を書くだけで作れる |
| アニメの名前は、どの部品も攻撃表の値（Clip の欄）で持つ。プレイヤーとボスの体の Script はコードの定数 | 部品は別の敵・別のモデルでも使う。体の Script はそのモデル専用 |
| 部品の「向く・止まる・アニメを段階の秒で流す」は **`GroundAttackPart`**（部品の基底）にまとめる | 3つの部品が同じことを書く（コピペを減らす）。中身はエンジンの道具（DeltaAngle・CalcSmoothRate・Play・CalcSpeedToFit）を組み合わせるだけ |
| ボスの体の Script **`BossCat`** は、顔の表情と、怯み・フェーズの移行・撃破のアニメだけ | 攻撃と待ちの動きは部品。体は状態を読んで見せるだけ |
| ボスは Character Controller＋ Capsule で動かす | 歩く・跳ぶ・落ちる・壁で止まるはエンジンの仕事（プレイヤーと同じ）。自分の子の Collider・Hurtbox とは当たらない |
| ボス部屋の大きさ・作り方、飛びかかりの届く距離は**作りながら調整**する | 地形の作り方（Polygon Shape の向き・地形の機能・モジュール）を考えている途中。G-6 は仮置き |

---

## 2. 全体の形

### 2.1 ファイル

```
project/Source/Scripts/
├ Combat/
│ └ HitBlink.h・.cpp           … E2（新しい）
├ Player/
│ ├ PlayerController.h・.cpp   … A（全部差し替え）
│ └ JustCounter.h・.cpp        … B（新しい）
└ Enemy/
  ├ BossCat.h・.cpp            … E（新しい）
  └ AttackParts/
    ├ GroundAttackPart.h・.cpp … C（新しい）
    ├ Chase.cpp                … D（新しい）
    ├ StationaryAttack.cpp     … D（新しい）
    └ Pounce.cpp               … D（新しい）
```

攻撃部品は、ほかのファイルから使わない（攻撃表が名前から作る）ので、`.h` を作らずに `.cpp` の中にクラスを書く（エンジンの Sandbox の ChaseTarget と同じ）。

### 2.2 Entity の組み立て

```
Player（Layer：Player）
├ Character Controller・Collider（Sphere）・Model Renderer・Animator     … 今まで通り
├ Health（Max 5・Invincible After Hit 1.0）・Hurtbox（Sphere）           … G-2
├ PlayerController                                                     … A
├ JustCounter                                                          … B
└ HitBlink                                                             … E2

Boss（Layer：Enemy）
├ Model Renderer・Animator（Boss.gltf）・Character Controller・Collider（Capsule）・Health … G-3
├ Attack Table（Boss.attacktable・Target は Player）                      … G-5
├ BossCat                                                               … E
├ Hurtbox Head / Hurtbox Front / Hurtbox Rear（子。Layer：Enemy）         … G-3
└ Tail Sweep Hitbox（子。Hitbox の Circle・Damage Source・Hitbox Telegraph） … G-3

Pounce Impact.prefab（Pounce が攻撃の始めに作り、跳ぶ瞬間に着地点へ動かす）    … G-4
```

### 2.3 1回の攻撃の流れ

```
待ち（Chase。攻撃表の Selecting の間）
 毎フレーム        プレイヤーの方へ向く。Stop Distance より遠ければ歩いて近づく
 攻撃を選んだ      OnEnd で止まる（攻撃の最初のフレームに歩きの速さが残らない）

飛びかかり（Pounce）
 OnBegin           Pounce Impact を Prefab から作る（Component が付くのは次のフレーム。まだ Off）
 Windup 0〜0.8     Pounce_Windup（かがむ）。プレイヤーへ向き続ける
 Windup 0.8        跳ぶ：着地点を決める → Pounce Impact を着地点へ動かして Warn（空中の秒）→ 着地点へ向く → 重力を切る → Pounce_Leap
 Windup 0.8〜1.6   放物線を進む（縦も横もコード）。Just Lead Time 秒前から、ジャスト判定の範囲が有効
 Active            着地：重力を戻す・Activate（当たる）・Pounce_Active
 Recovery          閉じる・Pounce_Recovery
 OnEnd             閉じる・Pounce Impact を消す・重力を戻す（怯みで止められたときも）

しっぽ（StationaryAttack）
 Windup            TailSweep_Windup。SyncHitbox → Warning（床に円の予告）
 Active            TailSweep_Active（1回転）。SyncHitbox → Active
 Recovery          TailSweep_Recovery。SyncHitbox → Off
 OnEnd             閉じる

プレイヤー
 回避ジャンプを始めた瞬間  TryConsumeJust → ジャストなら JustCounter::Begin（よけた攻撃は、その回ではもう当たらない）
 毎フレームの最後          Health の Invincible ＝ IsInvincible()（跳んでから Dodge Invincible Time 秒）
 OnHitTaken                HP 0 なら Down、でなければ Hit（のけぞり・押し戻し）
```

---

## パート 0：ボスのモデルを直して書き出し直す（Blender）

> **2026-11-07 に済み**（頼まれて直接やった）。下のスクリプトを開いている Boss.blend に流し、保存して `Boss.gltf`・`Boss.bin` を書き出し直した。
> 書き出しの設定：glTF Separate（.gltf＋.bin）・Selected Only（BossRig と Boss だけ。カメラ・床・ライトは出さない）・Animation の Mode は Actions・Keep Original（BossFace.png は書き直さず、元のファイルを指す）。ほかは既定。
> 確かめたこと：アニメの名前に番号が無い（30本）・`texCoord` が無い・UV は1つ（TEXCOORD_0 だけ）・BossFace.png は変わっていない・エンジンで読めて、飛びかかり・しっぽ・倒れるまで動く。
> 前の Boss.blend・Boss.gltf・Boss.bin の写しは、そのセッションの scratchpad（`boss_backup`）にだけある。保存のときにできた Boss.blend1 は消した。

直すことは2つ。

| 直すこと | 理由 |
|---|---|
| アニメの名前の頭の番号（`11_` など）を外す | エンジンがアニメを名前で選ぶようになった（プレイヤーと同じ）。外した後は `Idle`・`Walk`・`Pounce_Windup`・`Pounce_Leap`・`Pounce_Active`・`Pounce_Recovery`・`TailSweep_Windup`・`TailSweep_Active`・`TailSweep_Recovery`・`Stagger`・`PhaseChange`・`Down` など。この md の攻撃表・BossCat はこの名前を使う |
| **顔の UV を直す** | 今の `Boss.gltf` は**エンジンで読めない**（読み込みで止まる）。顔のマテリアルに `"texCoord": -1` が書き出されているため。メッシュの UV が2つあり、顔の UV が入っているのは `Float2` という名前の方で、`UVマップ` は全部 0 の空。しかも描画に使う UV（カメラの印）がどちらにも付いていないので、書き出しが番号を決められなかった |

1. `project/Resources/Model/Boss/Boss.blend` を Blender で開く（**開いている Blender に保存していない変更があれば、先に保存するか捨てるか決める**）。
2. **Scripting** のタブ → Text の **New** に下を貼って **Run Script**。

```python
import re
import bpy

# --- アニメの名前の頭の「数字＋_」を消す（このファイルは NLA のトラックを使っていないので、アクションだけ）---
prefix = re.compile(r"^\d+_")
for action in bpy.data.actions:
    action.name = prefix.sub("", action.name)

# --- 顔の UV：空の「UVマップ」を消し、顔の UV が入っている「Float2」を「UVMap」にして、描画に使う UV にする ---
mesh = bpy.data.objects["Boss"].data
empty = mesh.uv_layers.get("UVマップ")
if empty is not None:
    mesh.uv_layers.remove(empty)
uv = mesh.uv_layers["Float2"]
uv.name = "UVMap"
mesh.uv_layers.active = uv
uv.active_render = True

print([action.name for action in bpy.data.actions])
print([(layer.name, layer.active_render) for layer in mesh.uv_layers])
```

3. 保存して、前と同じ設定で `Boss.gltf` に書き出し直す。
4. 書き出した `Boss.gltf` をテキストで開き、次を確かめる。
   - `"animations"` の名前に番号が無い
   - `"texCoord":-1` が無い（顔の `baseColorTexture` に `texCoord` が無いか、`0`）

---

## パート A：PlayerController（全部差し替え）

変わる所：
- `PlayerState` に `Hit`・`Down`。`OnHitTaken` で入る（`StartHit`・`StartDown`・`UpdateHit`）。当たるたびにログに「被弾：攻撃の名前（減った量・残り）」
- 押し戻しは `CalcKnockbackSpeed`（回避ジャンプの `CalcDodgeHorizontalSpeed` と同じ「進んだ割合の差 ÷ 時間」。割合の形はエンジンの `Ease::Apply` の OutQuad）
- 回避ジャンプを始めた瞬間に `HitSystem::TryConsumeJust` → `JustCounter::Begin`
- 毎フレームの最後に `SyncInvincible`（`IsInvincible()` を Health の Invincible へ）
- `CanDodge()`・`GetDodgeCooldownRatio()`（右下の回避の輪で使う）。回避の待ち時間は、どの状態でも時間で減る
- **Acceleration・Turn Sharpness を Move Smooth Time・Turn Smooth Time（秒）に変えた**。エンジンの `MathUtility::CalcSmoothRate`（秒で持つ）に合わせて、自前の `CalcDampFactor` を消すため。前の値 20・18 は、0.05・0.055 秒と同じ動き（1 ÷ 20、1 ÷ 18）
- 角度の計算は `MathUtility::DirectionToYaw`・`YawToDirection`・`DeltaAngle` に置き換え
- Dodge Cooldown の初めの値を 1.0 に

### A-1 `project/Source/Scripts/Player/PlayerController.h`

```cpp
#pragma once
#include <HTTEngine/Components/Physics/CharacterControllerComponent.h>
#include <HTTEngine/ECS/Script.h>

// プレイヤーの操作（歩く・振り向く・回避ジャンプ・被弾・倒れる・アニメの切り替え）。プレイヤーの Entity に付ける
// 同じ Entity に付ける物：Collider（Sphere か Capsule）・Character Controller・Model Renderer・Animator（Player.gltf）・Health・Hurtbox・JustCounter
// 動かすのは CharacterControllerSystem。この Script は「どの向きへどれだけの速さで動きたいか」を velocity に書くだけ
// （坂で止まる・段を上る・壁に沿う・地面に吸い付く・重力は Character Controller の仕事）
// 攻撃を断るかは DamageSystem が Health の Invincible を見て決める。この Script は毎フレーム IsInvincible() を Health に写すだけ

// 前方宣言
class Curve;

/// <summary>
/// プレイヤーの状態（反撃・必殺などは、ここに足していく）
/// </summary>
enum class PlayerState {
	Move,  // 地面を歩く（落ちている間も、入力で横に動ける）
	Dodge, // 回避ジャンプ（入力の向きへ斜め上に跳んで1回転。着地まで向きは変えない。横の進み方は Dodge Move Curve）
	Hit,   // のけぞり（Hit Stun Time 秒。操作なし。当たった向きへ Hit Knockback Distance だけ押される）
	Down,  // 倒れた（HP 0。この後はずっと操作なし）
};


/// <summary>
/// プレイヤーの操作
/// </summary>
class PlayerController : public Script {
public:
	void Start() override;
	void Update() override;
	void OnHitTaken(const HitInfo& hit) override;
	void Fields(ComponentUI& ui) override;

	// ===== ほかの Script から読む（ボスの攻撃・ジャスト回避の判定・カメラ・UI など）=====
	PlayerState GetState() const { return state_; }                 // 今の状態
	bool IsDodging() const { return state_ == PlayerState::Dodge; } // 回避ジャンプ中
	bool IsInvincible() const;                                      // 攻撃が当たらない（回避ジャンプの始めの Dodge Invincible Time 秒）
	Vector3 GetFacingDirection() const;                             // 向いている向き（地面に平ら・長さ1）
	bool CanDodge() const;                                          // 今、回避ジャンプを始められる（歩いている・地面にいる・待ち時間が終わっている）
	float GetDodgeCooldownRatio() const;                            // 回避の待ち時間の残りの割合（着地した瞬間が 1、跳べるようになると 0。右下の回避の輪に使う）

private:
	// ===== 状態ごとの処理 =====
	void UpdateMove(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime);  // 歩く・回避を始める
	void UpdateDodge(CharacterControllerComponent& controller, float deltaTime);                              // 回避ジャンプ中（横の速さ）・着地
	void UpdateHit(CharacterControllerComponent& controller, float deltaTime);                                // のけぞり（押し戻し・時間が来たら歩きへ）
	void StartDodge(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime); // 回避ジャンプを始める（ジャスト回避の判定も）
	void StartHit(const HitInfo& hit);                                                                        // のけぞりを始める
	void StartDown();                                                                                         // 倒れる

	// ===== 毎フレーム（状態によらない）=====
	Vector3 CalcMoveDirection() const;                                // 入力をカメラから見た向きに直す
	void UpdateFacing(const Vector3& moveDirection, float deltaTime); // 向き
	void UpdateAnimation(const Vector3& moveDirection);               // 歩きと待機のアニメ（ほかの状態のアニメは、入ったときに1回だけ流す）
	void SyncInvincible();                                            // IsInvincible() を Health の Invincible に写す

	// ===== 道具 =====
	static float CalcGravity(const CharacterControllerComponent& controller); // このキャラにかかる重力の強さ（m/秒²。Character Controller と同じ値）
	float CalcDodgeHorizontalSpeed(float elapsed, float deltaTime) const;    // 回避ジャンプで、elapsed 秒から deltaTime 秒の間に横へ進む速さ（カーブから）
	const Curve& GetDodgeMoveCurve() const;                                  // 回避ジャンプの横の進み方のカーブ（未選択なら InOutSine）
	float CalcKnockbackSpeed(float elapsed, float deltaTime) const;          // のけぞりで、elapsed 秒から deltaTime 秒の間に押される速さ
	void DampHorizontalVelocity(CharacterControllerComponent& controller, const Vector3& target, float deltaTime) const; // 横の速さを target へなめらかに近づける

	// ===== Inspector で変える値 =====
	float moveSpeed_ = 6.0f;            // 歩く速さ（m/秒）
	float moveSmoothTime_ = 0.05f;      // 速さが入力に追いつくまでの秒（小さいほどキビキビ、大きいほど滑る。着地の後の勢いの残り方も決まる）
	float turnSmoothTime_ = 0.055f;     // 向きが入力に追いつくまでの秒（小さいほど素早く振り向く）
	float dodgeDistance_ = 4.0f;        // 回避ジャンプで横に進む距離（m。平らな地面で、跳んでから着地するまで）
	float dodgeHeight_ = 1.2f;          // 回避ジャンプの一番高い所（m。足元から）。空中で反撃射撃を撃つ高さになる
	uint32_t dodgeMoveCurve_ = 0;       // 回避ジャンプの横の進み方（.curve の番号。横軸＝跳んでからの時間の割合 0〜1、縦軸＝進んだ距離の割合 0〜1。0 なら InOutSine）
	float dodgeInvincibleTime_ = 0.35f; // 回避ジャンプを始めてから無敵の秒（跳んでいる間ずっとではない。着地を狙われると当たる）
	float dodgeCooldown_ = 1.0f;        // 着地してから、次の回避ジャンプができるまでの秒（右下の回避の輪で見せる）
	float hitStunTime_ = 0.4f;          // 被弾してから動けるようになるまでの秒（のけぞり。Hit のアニメもこの秒で流しきる）
	float hitKnockbackDistance_ = 1.0f; // 被弾したときに、当たった向きへ押される距離（m。のけぞりの間に、出だしが速く、だんだん止まる形で進む）

	// ===== 実行中だけの値（保存しない）=====
	PlayerState state_ = PlayerState::Move;       // 今の状態
	float facingYaw_ = 0.0f;                      // 向いている角度（度。Y のまわり。0 で +Z、90 で +X）
	Vector3 dodgeDirection_ = {0.0f, 0.0f, 1.0f}; // 回避ジャンプの向き（始めたときに決めて、着地まで変えない）
	float dodgeElapsed_ = 0.0f;                   // 回避ジャンプを始めてからの秒
	float dodgeAirTime_ = 0.0f;                   // 平らな地面で着地するまでの秒（アニメの速さを合わせるのに使う）
	float dodgeCooldownTimer_ = 0.0f;             // 次の回避ジャンプができるまでの残りの秒
	float hitElapsed_ = 0.0f;                     // のけぞりを始めてからの秒
	Vector3 knockbackDirection_ = {0.0f, 0.0f, -1.0f}; // 押される向き（当たったときに決めて、のけぞりの間は変えない）
};
```

### A-2 `project/Source/Scripts/Player/PlayerController.cpp`

```cpp
#include "PlayerController.h"

#include <cmath>
#include <format>
#include <string_view>

#include <HTTEngine/Components/Combat/HealthComponent.h>
#include <HTTEngine/Curve/Curve.h>
#include <HTTEngine/Curve/CurveManager.h>
#include <HTTEngine/Diagnostics/LogManager.h>
#include <HTTEngine/Input/InputManager.h>
#include <HTTEngine/Math/Easing.h>
#include <HTTEngine/Systems/Animation/AnimationSystem.h>
#include <HTTEngine/Systems/Combat/HitSystem.h>
#include <HTTEngine/Systems/Physics/PhysicsSystem.h>

#include "JustCounter.h"

SCRIPT(PlayerController, "Player");

namespace {
// --- アニメの名前（Player.gltf の中の名前＝Blender のアクションの名前。ファイルの中の並びは関係ない）---
constexpr std::string_view kClipIdle = "Idle";           // 待機（ループ）
constexpr std::string_view kClipRun = "Run";             // 走り（ループ。杖を前に構えたまま）
constexpr std::string_view kClipHit = "Hit";             // のけぞり（1回だけ。Hit Stun Time 秒で流しきる）
constexpr std::string_view kClipDown = "Down";           // 倒れる（1回だけ。最後の形のまま止まる）
constexpr std::string_view kClipDodgeJump = "DodgeJump"; // 回避ジャンプ（1回だけ。跳ぶ → 前に1回転 → 着地）

// --- 入力 ---
constexpr float kMoveInputDeadzone = 0.1f; // これより小さい入力は「止まっている」扱い（スティックを離したときの小さな揺れ）
constexpr float kRunAnimMinSpeed = 0.5f;   // スティックを少しだけ倒したときの走りのアニメの速さ（倒しきると 1）
} // namespace


//=============================================================================
// エンジンが呼ぶ関数
//=============================================================================
// ===== 始め =====
void PlayerController::Start() {
	// 置いたときの向きから始める（エディタで回して置いた向きを引き継ぐ）
	facingYaw_ = GetEulerAngles(transform).y;
	AnimationSystem::Play(gameObject.handle, kClipIdle, true, 1.0f);
}

// ===== 毎フレーム =====
void PlayerController::Update() {
	const float deltaTime = Time::GetDeltaTime();
	if (deltaTime <= 0.0f) {
		return; // 時間が止まっている（ヒットストップ・時間の倍率 0）
	}
	CharacterControllerComponent* controller = GetComponent<CharacterControllerComponent>();
	if (controller == nullptr) {
		return; // Character Controller を付け忘れている（動かせない）
	}

	// --- 回避の待ち時間は、どの状態でも時間で減る（のけぞっている間も）---
	dodgeCooldownTimer_ = (std::max)(dodgeCooldownTimer_ - deltaTime, 0.0f);

	// --- 入力 → 状態ごとの処理 → 向き → アニメ → 無敵 の順（後ろの3つは、このフレームに状態が変わったかを見るため後ろ）---
	const Vector3 moveDirection = CalcMoveDirection();
	switch (state_) {
	case PlayerState::Move:
		UpdateMove(*controller, moveDirection, deltaTime);
		break;
	case PlayerState::Dodge:
		UpdateDodge(*controller, deltaTime);
		break;
	case PlayerState::Hit:
		UpdateHit(*controller, deltaTime);
		break;
	case PlayerState::Down:
		DampHorizontalVelocity(*controller, {0.0f, 0.0f, 0.0f}, deltaTime); // 倒れた所で止まる（押し戻しの勢いだけ消していく）
		break;
	}
	UpdateFacing(moveDirection, deltaTime);
	UpdateAnimation(moveDirection);
	// 当たりを調べる HitSystem はこの後（フレームの終わり）に回るので、このフレームの状態の無敵がそのまま効く
	SyncInvincible();
}

// ===== 攻撃を受けた =====
// 受けた本体（Health の付いたこの Entity）に、DamageSystem が HP を減らした後で届く（届くのは次のフレームの Update の前）
// 無敵で断った攻撃は届かない（Health の Invincible・被弾の後の無敵の間）
void PlayerController::OnHitTaken(const HitInfo& hit) {
	if (state_ == PlayerState::Down) {
		return; // もう倒れている（HP 0 の後は当たらないが、同じフレームに2つ当たったときのため）
	}
	LogManager::Log(std::format("被弾：{}（{} 減って、残り {}）", GameObject{hit.hitbox}.GetName(), hit.damage, hit.healthAfter));
	if (hit.isDefeated) {
		StartDown();
	} else {
		StartHit(hit);
	}
}

// ===== Inspector に出す・保存する値 =====
// Tip はヘッダーで変数の右に書いたコメントと同じ文（2026-11-07）。並びもヘッダーと同じ
void PlayerController::Fields(ComponentUI& ui) {
	// --- State: Move ---
	ui.Field("Move Speed", moveSpeed_, AtLeast(0.0f).Tip("移動速度"));
	ui.Field("Move Smooth Time", moveSmoothTime_, AtLeast(0.0f).Tip("速さが入力に追いつくまでの時間"));
	ui.Field("Turn Smooth Time", turnSmoothTime_, AtLeast(0.0f).Tip("向きが入力に追いつくまでの時間"));
	// --- State: Dodge ---
	ui.Field("Dodge Distance", dodgeDistance_, AtLeast(0.0f).Tip("回避で横に進む距離"));
	ui.Field("Dodge Height", dodgeHeight_, AtLeast(0.0f).Tip("回避で上に上がる高さ"));
	ui.Field("Dodge Cooldown", dodgeCooldown_, AtLeast(0.0f).Tip("回避のクールタイム"));
	ui.AssetField("Dodge Move Curve", dodgeMoveCurve_, AssetType::Curve, Tip("回避の進み方の.curveの番号"));
	ui.Field("Dodge Invincible Time", dodgeInvincibleTime_, AtLeast(0.0f).Tip("回避の無敵時間"));
	// --- State: Hit ---
	ui.Field("Hit Stun Time", hitStunTime_, AtLeast(0.0f).Tip("スタンの時間"));
	ui.Field("Hit Knockback Distance", hitKnockbackDistance_, AtLeast(0.0f).Tip("ノックバックの距離"));
}


//=============================================================================
// ほかの Script から読む
//=============================================================================
// ===== 攻撃が当たらない =====
bool PlayerController::IsInvincible() const {
	return state_ == PlayerState::Dodge && dodgeElapsed_ < dodgeInvincibleTime_;
}

// ===== 向いている向き =====
Vector3 PlayerController::GetFacingDirection() const { return MathUtility::YawToDirection(facingYaw_); }

// ===== 今、回避ジャンプを始められる =====
bool PlayerController::CanDodge() const {
	const CharacterControllerComponent* controller = GetComponent<CharacterControllerComponent>();
	return state_ == PlayerState::Move && controller != nullptr && controller->IsGrounded() && dodgeCooldownTimer_ <= 0.0f;
}

// ===== 回避の待ち時間の残りの割合 =====
float PlayerController::GetDodgeCooldownRatio() const { return (dodgeCooldown_ > 0.0f) ? dodgeCooldownTimer_ / dodgeCooldown_ : 0.0f; }


//=============================================================================
// 状態ごとの処理
//=============================================================================
// ===== 歩く =====
// velocity の X・Z を毎フレーム書く（Y は Character Controller が重力で決める。地面にいる間は 0）
void PlayerController::UpdateMove(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime) {
	// --- 横の速さ：目標の速さへなめらかに近づける（着地の後は、回避の速さから歩きの速さへ少しずつ落ちる＝勢いが残る）---
	DampHorizontalVelocity(controller, moveDirection * moveSpeed_, deltaTime);

	// --- 回避ジャンプ：キーボードは Space、パッドは A。地面にいて、待ち時間が終わっているときだけ（空中では出ない＝1回だけ）---
	const bool isPressed = InputManager::IsKeyTriggered(DIK_SPACE) || InputManager::IsButtonTriggered(GamepadButton::A);
	if (isPressed && CanDodge()) {
		StartDodge(controller, moveDirection, deltaTime);
	}
}

// ===== 回避ジャンプを始める =====
// 「どれだけ横に進むか（Dodge Distance）」と「どこまで高く上がるか（Dodge Height）」から、跳ぶ速さを逆算する
//   上向きの速さ vy ＝ √(2 × 重力 × 高さ)       … 投げ上げで、その高さで速さが 0 になる速さ
//   着地までの時間 T ＝ 2 × vy ÷ 重力            … 上がる時間と下りる時間は同じ
//   横は T の間に Dodge Distance だけ進む。どう進むか（出だし・終わりの速さ）は Dodge Move Curve（CalcDodgeHorizontalSpeed）
// 重力は Character Controller と同じ値を使うので、Gravity Scale を変えても距離と高さはそのまま（速く跳んで、速く落ちるだけ）
void PlayerController::StartDodge(CharacterControllerComponent& controller, const Vector3& moveDirection, float deltaTime) {
	const float gravity = CalcGravity(controller);
	if (gravity <= 0.0f || dodgeHeight_ <= 0.0f) {
		return; // 重力が無い（落ちてこない）・高さ 0 では跳べない
	}
	const float upSpeed = std::sqrt(2.0f * gravity * dodgeHeight_);
	dodgeAirTime_ = 2.0f * upSpeed / gravity;

	// 入力があればその向き、無ければ今向いている向きへ（止まったままでも避けられる）
	dodgeDirection_ = (LengthSq(moveDirection) > 0.0f) ? Normalize(moveDirection) : GetFacingDirection();
	state_ = PlayerState::Dodge;
	dodgeElapsed_ = 0.0f;

	// --- アニメ：1回転を、平らな地面で着地するまでの時間でちょうど流しきる（距離・高さ・重力を変えてもアニメが合う）---
	// 前の回避のアニメがまだ流れていても最初から（Play は同じ名前が流れていると速さしか変えないので、Restart も呼ぶ）
	AnimationSystem::Play(gameObject.handle, kClipDodgeJump, false, AnimationSystem::CalcSpeedToFit(gameObject.handle, kClipDodgeJump, dodgeAirTime_));
	AnimationSystem::Restart(gameObject.handle);

	// 上の速さは1回だけ書く（あとは Character Controller が重力で落とす）。横はこのフレームの分。次のフレームからは UpdateDodge が書く
	controller.velocity = dodgeDirection_ * CalcDodgeHorizontalSpeed(0.0f, deltaTime) + Vector3{0.0f, upSpeed, 0.0f};

	// --- ジャスト回避：跳んだ瞬間に、ボスの攻撃のジャスト判定の範囲に入っていたか ---
	// 入っていれば HitSystem が「よけた」と記録する（その攻撃の回では、無敵が切れた後も、もう当たらない）
	// 位置は前のフレームの終わりの物（HitSystem が最後に調べた位置）。跳んだ後の位置ではない
	JustCandidate just;
	if (HitSystem::TryConsumeJust(gameObject.handle, just)) {
		if (JustCounter* counter = GetComponent<JustCounter>()) {
			counter->Begin(just);
		} else {
			LogManager::Warning("ジャスト回避したが、JustCounter が付いていない（プレイヤーの Entity に足してください）");
		}
	}
}

// ===== 回避ジャンプ中・着地 =====
void PlayerController::UpdateDodge(CharacterControllerComponent& controller, float deltaTime) {
	// --- 着地：地面に戻ったら歩きへ。始めたフレームは Character Controller がまだ動かしていないので、ここに来るのは次のフレームから ---
	// （跳んだ次のフレームには空中になっている。天井ですぐ止められたときも、落ちて地面に着けばここで終わる）
	if (controller.IsGrounded()) {
		state_ = PlayerState::Move;
		dodgeCooldownTimer_ = dodgeCooldown_;
		return;
	}
	// --- 横の速さ：カーブの進み方になるように、このフレームに進む分の速さを毎フレーム書く（縦は Character Controller の重力のまま）---
	// 壁に当たっても毎フレーム同じ向きへ書くが、壁へ向かう分は Character Controller が消すので、壁に沿うか止まる
	// 平らな地面で着地する時間（T）を過ぎてもまだ落ちている（台地の端から跳んだ）ときは書かない＝最後の速さのまま落ちる
	if (dodgeElapsed_ < dodgeAirTime_) {
		const Vector3 horizontal = dodgeDirection_ * CalcDodgeHorizontalSpeed(dodgeElapsed_, deltaTime);
		controller.velocity.x = horizontal.x;
		controller.velocity.z = horizontal.z;
	}
	dodgeElapsed_ += deltaTime;
}

// ===== のけぞり =====
// 押し戻しの横の速さを毎フレーム書く（縦は Character Controller の重力のまま。空中で当たったら、そのまま落ちる）
// 時間が来たら歩きへ戻る（空中でも戻る。歩きは落ちている間も横に動ける）。回避の待ち時間は足さない（のけぞりが罰）
void PlayerController::UpdateHit(CharacterControllerComponent& controller, float deltaTime) {
	const Vector3 horizontal = knockbackDirection_ * CalcKnockbackSpeed(hitElapsed_, deltaTime);
	controller.velocity.x = horizontal.x;
	controller.velocity.z = horizontal.z;
	hitElapsed_ += deltaTime;
	if (hitElapsed_ >= hitStunTime_) {
		state_ = PlayerState::Move;
	}
}

// ===== のけぞりを始める =====
void PlayerController::StartHit(const HitInfo& hit) {
	state_ = PlayerState::Hit; // 回避ジャンプの途中で当たったら、回避はここで終わる
	hitElapsed_ = 0.0f;

	// --- 押し戻しの向き：攻撃の向き（円の攻撃なら、円の真ん中から外へ）を地面に平らにした向き。円の外へ押されて、続けて当たりにくい ---
	// 速さは、このフレームから UpdateHit が書く
	const Vector3 direction = {hit.direction.x, 0.0f, hit.direction.z};
	knockbackDirection_ = (LengthSq(direction) > 0.0001f) ? Normalize(direction) : GetFacingDirection() * -1.0f; // 真上・真下からの攻撃は向きが決まらないので、後ろへ

	// --- アニメ：のけぞりの秒でちょうど流しきる。続けて当たったときも最初から（Restart）---
	AnimationSystem::Play(gameObject.handle, kClipHit, false, AnimationSystem::CalcSpeedToFit(gameObject.handle, kClipHit, hitStunTime_));
	AnimationSystem::Restart(gameObject.handle);
}

// ===== 倒れる =====
void PlayerController::StartDown() {
	state_ = PlayerState::Down;
	AnimationSystem::Play(gameObject.handle, kClipDown, false, 1.0f);
	AnimationSystem::Restart(gameObject.handle);
	LogManager::Log("プレイヤーが倒れた（ゲームオーバーの流れは、まだ作っていない）");
}


//=============================================================================
// 毎フレーム（状態によらない）
//=============================================================================
// ===== 入力をカメラから見た向きに直す =====
// 上（W・スティックを奥）を押したら画面の奥へ、右を押したら画面の右へ動くようにする。長さは 0〜1（スティックの倒し具合）
Vector3 PlayerController::CalcMoveDirection() const {
	const Vector2 input = InputManager::GetMoveAxis();
	if (Length(input) < kMoveInputDeadzone) {
		return {0.0f, 0.0f, 0.0f};
	}

	// --- カメラの前を地面に平らにした向き（カメラが無いときはワールドの +Z を奥にする）---
	Vector3 forward = {0.0f, 0.0f, 1.0f};
	if (const Camera* camera = CameraSystem::GetMainCamera()) {
		// ビュー行列はカメラのワールド行列の逆。回転の部分は「逆＝転置」なので、ビュー行列の3列目がカメラの前（ワールドの向き）になる
		const Matrix4x4& view = camera->GetViewMatrix();
		const Vector3 flatForward = {view.m[0][2], 0.0f, view.m[2][2]}; // 見下ろしのカメラは下を向いているので、y を捨てて地面に平らにする
		if (LengthSq(flatForward) > 0.0001f) {                          // 真下を向いているときだけは前が決まらないので、+Z のまま
			forward = Normalize(flatForward);
		}
	}
	// 右は、前を Y のまわりに 90 度回した向き（(x, z) → (z, -x)。左手系で上から見て時計回り）
	const Vector3 right = {forward.z, 0.0f, -forward.x};
	return right * input.x + forward * input.y;
}

// ===== 向き =====
// 歩いている向きへ振り向く。回避ジャンプ中は跳んだ向きにすぐ向く（1回転はアニメがする）
// 止まっているとき・のけぞり・倒れている間は、最後に向いた向きのまま
void PlayerController::UpdateFacing(const Vector3& moveDirection, float deltaTime) {
	Vector3 target = {0.0f, 0.0f, 0.0f};
	if (state_ == PlayerState::Move) {
		target = moveDirection;
	} else if (state_ == PlayerState::Dodge) {
		target = dodgeDirection_;
	}
	if (LengthSq(target) > 0.0f) {
		// 今の角度から一番近い回り方で回す（350 度の差は -10 度になる＝大回りしない）
		const float targetYaw = MathUtility::DirectionToYaw(target);
		const float rate = IsDodging() ? 1.0f : MathUtility::CalcSmoothRate(deltaTime, turnSmoothTime_);
		facingYaw_ = std::remainder(facingYaw_ + MathUtility::DeltaAngle(facingYaw_, targetYaw) * rate, 360.0f); // 何周しても -180〜180 に収める
	}
	SetEulerAngles(transform, {0.0f, facingYaw_, 0.0f});
}

// ===== 歩きと待機のアニメ =====
void PlayerController::UpdateAnimation(const Vector3& moveDirection) {
	if (state_ != PlayerState::Move) {
		// 回避ジャンプ・のけぞり・倒れるのアニメは、その状態に入ったときに1回だけ流す
		// ここで毎フレーム Play すると、アニメが終わった後に Play が「止まっている」と見て最初から流し直してしまう
		return;
	}
	const float moveAmount = Length(moveDirection); // スティックの倒し具合（0〜1）
	if (moveAmount > 0.0f) {
		// ゆっくり歩くときは足もゆっくり。std::lerp（C++20 の <cmath>）は a + (b - a) × t。エンジンの float の Lerp は MathUtility の中にあるので、STL の方を使う
		// Play は毎フレーム呼んでよい（同じ名前が流れていれば、速さだけ変わる）
		AnimationSystem::Play(gameObject.handle, kClipRun, true, std::lerp(kRunAnimMinSpeed, 1.0f, moveAmount));
	} else {
		AnimationSystem::Play(gameObject.handle, kClipIdle, true, 1.0f);
	}
}

// ===== 無敵を Health に写す =====
// Health の Invincible が true の間、DamageSystem は攻撃を断る（記録しないので、無敵が切れた後に同じ攻撃の範囲にいれば当たる）
// 被弾の後の無敵（Invincible After Hit）は Health が自分で数えるので、ここでは書かない
void PlayerController::SyncInvincible() {
	if (HealthComponent* health = GetComponent<HealthComponent>()) {
		health->invincible = IsInvincible();
	}
}


//=============================================================================
// 道具
//=============================================================================
// ===== このキャラにかかる重力の強さ =====
// Character Controller は「Settings の Physics → World → Gravity × Gravity Scale」で落とす。それと同じ値（向きは真下とみなして、長さだけ）
float PlayerController::CalcGravity(const CharacterControllerComponent& controller) {
	if (!controller.useGravity) {
		return 0.0f;
	}
	return Length(PhysicsSystem::GetGravity()) * controller.gravityScale;
}

// ===== 回避ジャンプで、elapsed 秒から deltaTime 秒の間に横へ進む速さ =====
// カーブは「時間の割合 → 進んだ距離の割合」。2つの時刻の割合の差 × 距離 が、この間に進む距離。それを時間で割ると速さになる
// 速さではなく「進んだ割合」をカーブにするのは、カーブの形をどう変えても、最後に進む距離がちょうど Dodge Distance になるため
// （速さのカーブだと、形を変えるたびに面積＝距離が変わってしまう）
float PlayerController::CalcDodgeHorizontalSpeed(float elapsed, float deltaTime) const {
	if (dodgeAirTime_ <= 0.0f || deltaTime <= 0.0f) {
		return 0.0f;
	}
	const Curve& curve = GetDodgeMoveCurve();
	const float from = curve.Evaluate(elapsed / dodgeAirTime_);
	const float to = curve.Evaluate((elapsed + deltaTime) / dodgeAirTime_); // 1 を過ぎたら最後の値（Evaluate がそう返す）
	return dodgeDistance_ * (to - from) / deltaTime;
}

// ===== 回避ジャンプの横の進み方のカーブ =====
const Curve& PlayerController::GetDodgeMoveCurve() const {
	if (const Curve* curve = CurveManager::Get(dodgeMoveCurve_)) {
		return *curve;
	}
	// 未選択：ゆっくり出て、ゆっくり止まる形（関数の中の static は、最初に通ったときに1回だけ作られる）
	static const Curve kDefaultCurve = Curve::FromEase(Ease::Type::InOutSine);
	return kDefaultCurve;
}

// ===== のけぞりで、elapsed 秒から deltaTime 秒の間に押される速さ =====
// 回避ジャンプの横の進み方と同じ考え方：「時間の割合 → 進んだ距離の割合」の差 × 距離 ÷ 時間
// 割合の形はエンジンの OutQuad（1 −（1 − t）²。出だしが速く、だんだん止まる）。のけぞりの秒の終わりに、ちょうど Hit Knockback Distance 進む
float PlayerController::CalcKnockbackSpeed(float elapsed, float deltaTime) const {
	if (hitStunTime_ <= 0.0f || deltaTime <= 0.0f) {
		return 0.0f;
	}
	const auto progress = [this](float time) { return Ease::Apply((std::min)(time / hitStunTime_, 1.0f), Ease::Type::OutQuad); };
	return hitKnockbackDistance_ * (progress(elapsed + deltaTime) - progress(elapsed)) / deltaTime;
}

// ===== 横の速さを target へなめらかに近づける =====
// 毎フレーム「残りの差 × 割合」だけ近づける。割合はエンジンの CalcSmoothRate（1 - e^(-時間 ÷ Move Smooth Time)）なので、フレームの速さに左右されない
// 縦（Y）は触らない（Character Controller の重力のまま）
void PlayerController::DampHorizontalVelocity(CharacterControllerComponent& controller, const Vector3& target, float deltaTime) const {
	const Vector3 current = {controller.velocity.x, 0.0f, controller.velocity.z};
	const Vector3 next = Lerp(current, {target.x, 0.0f, target.z}, MathUtility::CalcSmoothRate(deltaTime, moveSmoothTime_));
	controller.velocity.x = next.x;
	controller.velocity.z = next.z;
}
```

---

## パート B：JustCounter（新しいファイル）

ジャスト反撃の流れの仕切り役。今は「入った」記録とログだけ。次の作業で、スロー・背中側のカメラ・縮む輪・A を押した時間の判定・UI・必殺ゲージをここに足し、撃つ瞬間に PlayerController へ伝える。

Inspector の表示は `ui.Label`（灰色の文字。保存されない）で、Play 中の回数とよけた攻撃を見せる。

### B-1 `project/Source/Scripts/Player/JustCounter.h`

```cpp
#pragma once
#include <string>

#include <HTTEngine/ECS/Script.h>
#include <HTTEngine/Systems/Combat/HitSystem.h>

// ジャスト反撃の流れを受け持つ（仕切り役）。プレイヤーの Entity に付ける
// 入口：PlayerController が回避ジャンプを始めた瞬間に HitSystem::TryConsumeJust し、ジャストなら Begin を呼ぶ
// 今は記録とログだけ。次の作業で、スロー・背中側のカメラ・縮む輪・A を押した時間の判定・UI・必殺ゲージをここに足し、
// 撃つ瞬間に PlayerController へ「反撃を撃て」（判定の結果つき）と伝える（撃つ体の動きは PlayerController の PlayerState::Counter）

/// <summary>
/// ジャスト反撃の流れ
/// </summary>
class JustCounter : public Script {
public:
	void Fields(ComponentUI& ui) override;

	// ===== PlayerController から呼ぶ =====
	// ジャスト回避した（just はよけた攻撃）。反撃の流れを始める
	void Begin(const JustCandidate& just);

	// ===== ほかの Script から読む =====
	bool IsActive() const { return false; }               // 反撃の流れの途中か（今は入ってすぐ終わるので、いつも false。PlayerController の無敵に足す）
	int GetJustCount() const { return justCount_; }       // Play を押してからジャスト回避した回数（確かめ用）

private:
	// ===== 実行中だけの値（保存しない。Inspector に出すだけ）=====
	int justCount_ = 0;          // Play を押してからジャスト回避した回数
	std::string lastAttackName_; // 最後によけた攻撃（Hitbox の Entity の名前）
	float lastTimeToActive_ = 0.0f; // 最後によけたとき、攻撃が当たるまで何秒前だったか（0 なら当たっている最中）
};
```

### B-2 `project/Source/Scripts/Player/JustCounter.cpp`

```cpp
#include "JustCounter.h"

#include <format>

#include <HTTEngine/Diagnostics/LogManager.h>

SCRIPT(JustCounter, "Player");


//=============================================================================
// PlayerController から呼ぶ
//=============================================================================
// ===== ジャスト回避した =====
void JustCounter::Begin(const JustCandidate& just) {
	++justCount_;
	lastAttackName_ = GameObject{just.hitbox}.GetName();
	lastTimeToActive_ = just.timeToActive;
	LogManager::Log(std::format("ジャスト回避 {} 回目：{}（{} の攻撃・当たる {:.2f} 秒前）→ ジャスト反撃に入る（中身は次の作業）", justCount_, lastAttackName_,
	                            GameObject{just.instigator}.GetName(), lastTimeToActive_));
}


//=============================================================================
// Inspector
//=============================================================================
// 保存する値は無い。Play 中にジャスト回避を確かめるため、記録を灰色の文字で出すだけ（Label は保存されない）
void JustCounter::Fields(ComponentUI& ui) {
	ui.Label(std::format("ジャスト回避：{} 回", justCount_).c_str());
	ui.Label(std::format("最後によけた攻撃：{}（当たる {:.2f} 秒前）", lastAttackName_.empty() ? "なし" : lastAttackName_, lastTimeToActive_).c_str());
}
```

---

## パート C：GroundAttackPart（新しいファイル）

3つの部品が書く「相手の方を見る・向く・横の速さを書く・アニメを段階の秒で流す」をまとめた基底。`AttackPart` を継承し、部品はこれを継承する。中身はエンジンの道具を組み合わせるだけ。

`PlayFitted` が `Play` の後に `Restart` も呼ぶのは、前の段階・前の攻撃と同じ名前のアニメが流れていると、`Play` が速さしか変えない（最初に戻らない）ため。

### C-1 `project/Source/Scripts/Enemy/AttackParts/GroundAttackPart.h`

```cpp
#pragma once
#include <string_view>

#include <HTTEngine/AI/AttackPart.h>
#include <HTTEngine/Components/Animation/AnimatorComponent.h> // AnimationClipName（部品の値にアニメの名前を持つ）

// Character Controller で地面を動く敵の、攻撃部品・待ちの部品の基底（ゲーム側の道具）
// 部品が何度も書く「相手の方を見る・向く・横の速さを書く・アニメを段階の秒に合わせて流す」をまとめる（コピペを減らす）
// 敵の Entity に付ける物：Character Controller・Collider（Sphere か Capsule）・Animator・Attack Table
// 向きは Transform の Y のまわりだけを回す（見下ろしの敵は傾かない）

/// <summary>
/// 地面を動く敵の部品の基底
/// </summary>
class GroundAttackPart : public AttackPart {
protected:
	/// <summary>
	/// 相手への向き（地面に平ら・長さ1）と距離（地面に平らに測る）
	/// </summary>
	/// <returns>相手が居るか（居なければ direction・distance は変えない）</returns>
	static bool CalcToTarget(const AttackPartContext& context, Vector3& direction, float& distance);

	// direction（地面に平ら）の方へ、なめらかに向く。smoothTime 秒で残りの差が約 37% になる（0 ならすぐ向く）
	static void TurnToward(const AttackPartContext& context, const Vector3& direction, float smoothTime);

	// 横の速さ（X・Z）を書く。縦（Y）は Character Controller の重力のまま
	static void SetHorizontalVelocity(const AttackPartContext& context, const Vector3& velocity);

	// アニメを最初から、ちょうど seconds 秒で流しきる（ループしない）。名前が空なら何もしない
	static void PlayFitted(const AttackPartContext& context, std::string_view clip, float seconds);
};
```

### C-2 `project/Source/Scripts/Enemy/AttackParts/GroundAttackPart.cpp`

```cpp
#include "GroundAttackPart.h"

#include <HTTEngine/Components/Physics/CharacterControllerComponent.h>
#include <HTTEngine/Math/MathUtility.h>
#include <HTTEngine/Systems/Animation/AnimationSystem.h>

namespace {
constexpr float kMinDirectionLengthSq = 0.0001f; // これより短い向き（ほぼ真上・真下に居る）は、向きが決まらないとみなす
} // namespace


//=============================================================================
// 相手・向き
//=============================================================================
// ===== 相手への向きと距離 =====
bool GroundAttackPart::CalcToTarget(const AttackPartContext& context, Vector3& direction, float& distance) {
	if (!context.target) {
		return false;
	}
	Vector3 toTarget = GetWorldPosition(context.target.transform) - GetWorldPosition(context.self.transform);
	toTarget.y = 0.0f; // 地面に平らに（相手が跳んでいても、上を向かない）
	distance = Length(toTarget);
	if (distance * distance > kMinDirectionLengthSq) {
		direction = toTarget / distance;
	}
	return true;
}

// ===== なめらかに向く =====
// 今の角度から一番近い回り方で回す（DeltaAngle）。割合は CalcSmoothRate なので、フレームの速さに左右されない
void GroundAttackPart::TurnToward(const AttackPartContext& context, const Vector3& direction, float smoothTime) {
	if (LengthSq(Vector3{direction.x, 0.0f, direction.z}) <= kMinDirectionLengthSq) {
		return; // 向きが決まらない
	}
	TransformComponent& transform = context.self.transform;
	const float yaw = GetEulerAngles(transform).y;
	const float targetYaw = MathUtility::DirectionToYaw(direction);
	const float rate = MathUtility::CalcSmoothRate(context.deltaTime, smoothTime);
	SetEulerAngles(transform, {0.0f, yaw + MathUtility::DeltaAngle(yaw, targetYaw) * rate, 0.0f});
}


//=============================================================================
// 動き・アニメ
//=============================================================================
// ===== 横の速さを書く =====
void GroundAttackPart::SetHorizontalVelocity(const AttackPartContext& context, const Vector3& velocity) {
	if (CharacterControllerComponent* controller = context.self.GetComponent<CharacterControllerComponent>()) {
		controller->velocity.x = velocity.x;
		controller->velocity.z = velocity.z;
	}
}

// ===== アニメを seconds 秒で流しきる =====
// 前の段階・前の攻撃と同じ名前のアニメが流れていると、Play は速さしか変えない（最初に戻らない）。段階の頭から流したいので Restart も呼ぶ
void GroundAttackPart::PlayFitted(const AttackPartContext& context, std::string_view clip, float seconds) {
	if (clip.empty()) {
		return; // アニメを付けない段階
	}
	const Handle<Entity> self = context.self.handle;
	AnimationSystem::Play(self, clip, false, AnimationSystem::CalcSpeedToFit(self, clip, seconds));
	AnimationSystem::Restart(self);
}
```

---

## パート D：攻撃部品（新しいファイル）

### D-1 `project/Source/Scripts/Enemy/AttackParts/Chase.cpp`（待ちの部品）

止まる距離と歩き出す距離を **Resume Margin** だけずらしている。同じ距離にすると、境目で「歩く → 止まる → 歩く」をフレームごとに繰り返して、アニメががたつくため。

```cpp
#include <HTTEngine/Systems/Animation/AnimationSystem.h>

#include "GroundAttackPart.h"

// 攻撃部品はほかのファイルから使わない（攻撃表が名前から作る）ので、.h を作らずに .cpp の中にクラスを書く


/// <summary>
/// 待ちの部品：相手の方を向き、遠ければ歩いて近づき、近ければ止まる（攻撃表のフェーズの Idle Part に選ぶ）
/// <para>攻撃を選んだら、その攻撃より先に OnEnd が呼ばれて止まる（攻撃の最初のフレームに歩きの速さが残らない）</para>
/// </summary>
class Chase : public GroundAttackPart {
public:
	void OnBegin(const AttackPartContext& context) override;
	void OnUpdate(const AttackPartContext& context) override;
	void OnEnd(const AttackPartContext& context, AttackPartEnd reason) override;
	void Fields(ComponentUI& ui) override;

private:
	// ===== 攻撃表に出す値 =====
	float moveSpeed_ = 4.0f;           // 近づく速さ（m/秒）
	float stopDistance_ = 5.0f;        // 相手とこの距離まで近づいたら止まる（m。地面に平らに測る）
	float resumeMargin_ = 0.5f;        // 止まった後、Stop Distance よりこれだけ離れたら、また歩き出す（m。境目で歩く・止まるを繰り返さないように）
	float turnSmoothTime_ = 0.2f;      // 相手の方へ向く速さ（秒。小さいほど速く向く。0 ならすぐ向く）
	AnimationClipName idleClip_;       // 止まっている間のアニメ（ループ）
	AnimationClipName walkClip_;       // 歩いている間のアニメ（ループ）
	float walkAnimSpeed_ = 1.0f;       // 歩きのアニメの速さ（足が滑って見えたら、Move Speed に合わせて変える）

	// ===== 実行中だけの値 =====
	bool isWalking_ = false; // 今、歩いているか（境目の行ったり来たりを防ぐために覚える）
};
ATTACK_PART(Chase, "待ちの部品。相手の方を向き、遠ければ Character Controller で歩いて近づき、近ければ止まる");


//=============================================================================
// 始め・毎フレーム・終わり
//=============================================================================
// ===== 始め（Selecting に入った最初のフレーム）=====
void Chase::OnBegin(const AttackPartContext& /*context*/) { isWalking_ = false; }

// ===== 毎フレーム =====
void Chase::OnUpdate(const AttackPartContext& context) {
	Vector3 direction = {0.0f, 0.0f, 0.0f};
	float distance = 0.0f;
	if (!CalcToTarget(context, direction, distance)) {
		// 相手が居ない：その場で待つ
		isWalking_ = false;
		SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f});
		AnimationSystem::Play(context.self.handle, idleClip_.View(), true);
		return;
	}

	// --- 向く ---
	TurnToward(context, direction, turnSmoothTime_);

	// --- 歩く・止まる：止まるのは Stop Distance、歩き出すのはそれより Resume Margin だけ遠く（境目で行ったり来たりしない）---
	isWalking_ = isWalking_ ? (distance > stopDistance_) : (distance > stopDistance_ + resumeMargin_);
	SetHorizontalVelocity(context, isWalking_ ? direction * moveSpeed_ : Vector3{0.0f, 0.0f, 0.0f});

	// --- アニメ：同じ名前を毎フレーム渡しても、最初には戻らない（速さだけ変わる）---
	if (isWalking_) {
		AnimationSystem::Play(context.self.handle, walkClip_.View(), true, walkAnimSpeed_);
	} else {
		AnimationSystem::Play(context.self.handle, idleClip_.View(), true);
	}
}

// ===== 終わり（攻撃を選んだ・怯んだなど）=====
void Chase::OnEnd(const AttackPartContext& context, AttackPartEnd /*reason*/) {
	isWalking_ = false;
	SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f});
}


//=============================================================================
// 攻撃表に出す値
//=============================================================================
// Tip は変数の右に書いたコメントと同じ文（2026-11-07）
void Chase::Fields(ComponentUI& ui) {
	ui.Field("Move Speed###moveSpeed", moveSpeed_, AtLeast(0.0f).Tip("近づく速さ"));
	ui.Field("Stop Distance###stopDistance", stopDistance_, AtLeast(0.0f).Tip("相手とこの距離まで近づいたら止まる"));
	ui.Field("Resume Margin###resumeMargin", resumeMargin_, AtLeast(0.0f).Tip("止まった後、Stop Distanceよりこれだけ離れたらまた歩き出す"));
	ui.Field("Turn Smooth Time###turnSmoothTime", turnSmoothTime_, AtLeast(0.0f).Tip("相手の方を向く速さ"));
	ui.ClipField("Idle Clip###idleClip", idleClip_, Tip("止まっている間のアニメーション"));
	ui.ClipField("Walk Clip###walkClip", walkClip_, Tip("歩いている間のアニメーション"));
	ui.Field("Walk Anim Speed###walkAnimSpeed", walkAnimSpeed_, AtLeast(0.0f).Tip("歩きのアニメーションの速さ"));
}
```

### D-2 `project/Source/Scripts/Enemy/AttackParts/StationaryAttack.cpp`（その場の攻撃）

攻撃の始め（OnBegin）に、子の Hitbox の **Instigator（撃った本人）をこの敵にする**。空のままだと Hitbox の Entity が本人になり、ジャスト回避の記録・当たりの知らせで「誰の攻撃か」がボスにならない（次の作業の反撃で、ボスへ向くのに要る）。シーンで Instigator に Boss を入れなくてよい。

```cpp
#include <format>
#include <string>

#include <HTTEngine/Components/Combat/HitboxComponent.h>
#include <HTTEngine/Diagnostics/LogManager.h>
#include <HTTEngine/Systems/Combat/HitSystem.h>

#include "GroundAttackPart.h"

// 攻撃部品はほかのファイルから使わない（攻撃表が名前から作る）ので、.h を作らずに .cpp の中にクラスを書く


/// <summary>
/// その場の攻撃：動かずに、段階ごとのアニメを段階の秒で流し、子の Hitbox の窓を段階に合わせる
/// <para>予備動作 → 予告（床に形）、攻撃 → 当たる、硬直 → 閉じる。しっぽの薙ぎ払い・ボスの周りの攻撃などに使う</para>
/// <para>敵の子の Entity に Hitbox（と Damage Source・Hitbox Telegraph）を置き、その名前を Hitbox の欄に書く</para>
/// </summary>
class StationaryAttack : public GroundAttackPart {
public:
	void OnBegin(const AttackPartContext& context) override;
	void OnStage(const AttackPartContext& context) override;
	void OnUpdate(const AttackPartContext& context) override;
	void OnEnd(const AttackPartContext& context, AttackPartEnd reason) override;
	void Fields(ComponentUI& ui) override;

private:
	// ===== 攻撃表に出す値 =====
	AnimationClipName windupClip_;   // 予備動作のアニメ（予備動作の秒でちょうど流しきる）
	AnimationClipName activeClip_;   // 攻撃のアニメ（攻撃の秒でちょうど流しきる）
	AnimationClipName recoveryClip_; // 硬直のアニメ（硬直の秒でちょうど流しきる）
	std::string hitboxName_;         // 窓を合わせる Hitbox の、子の Entity の名前（孫は探さない）
	bool faceTargetInWindup_ = false; // 予備動作の間、相手の方へ向き続けるか
	float turnSmoothTime_ = 0.2f;    // 向く速さ（秒。小さいほど速く向く。Face Target In Windup のときだけ使う）

	// ===== 実行中だけの値 =====
	GameObject hitbox_; // 窓を合わせる Hitbox の Entity（OnBegin で名前から探す）
};
ATTACK_PART(StationaryAttack, "その場の攻撃。段階ごとのアニメを段階の秒で流し、子の Hitbox の窓を段階に合わせる（予備動作＝予告・攻撃＝当たる・硬直＝閉じる）");


//=============================================================================
// 始め・段階・毎フレーム・終わり
//=============================================================================
// ===== 始め（攻撃に選ばれた）=====
void StationaryAttack::OnBegin(const AttackPartContext& context) {
	hitbox_ = context.self.FindChild(hitboxName_);
	if (!hitbox_) {
		if (!hitboxName_.empty()) {
			LogManager::Warning(std::format("StationaryAttack：{} の子に \"{}\" がありません（当たりなしで、アニメだけ流します）", context.self.GetName(), hitboxName_));
		}
		return;
	}
	// 撃った本人＝この敵（空のままだと Hitbox の Entity が本人になり、ジャスト回避・当たりの知らせで「誰の攻撃か」が敵にならない）
	if (HitboxComponent* hitbox = hitbox_.GetComponent<HitboxComponent>()) {
		hitbox->instigator = EntityManager::RefOf(context.self.handle);
	}
}

// ===== 段階に入った =====
void StationaryAttack::OnStage(const AttackPartContext& context) {
	SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f}); // その場の攻撃なので動かない
	if (hitbox_) {
		SyncHitbox(hitbox_, context); // 予備動作 → Warning、攻撃 → Active（新しい攻撃の回）、硬直 → Off
	}
	switch (context.stage) {
	case AttackStage::Windup:
		PlayFitted(context, windupClip_.View(), context.stageDuration);
		break;
	case AttackStage::Active:
		PlayFitted(context, activeClip_.View(), context.stageDuration);
		break;
	case AttackStage::Recovery:
		PlayFitted(context, recoveryClip_.View(), context.stageDuration);
		break;
	case AttackStage::None:
		break;
	}
}

// ===== 毎フレーム =====
void StationaryAttack::OnUpdate(const AttackPartContext& context) {
	if (context.stage != AttackStage::Windup || !faceTargetInWindup_) {
		return;
	}
	Vector3 direction = {0.0f, 0.0f, 0.0f};
	float distance = 0.0f;
	if (CalcToTarget(context, direction, distance)) {
		TurnToward(context, direction, turnSmoothTime_);
	}
}

// ===== 終わり（途中で止められたときも呼ばれる）=====
void StationaryAttack::OnEnd(const AttackPartContext& /*context*/, AttackPartEnd /*reason*/) {
	if (hitbox_) {
		HitSystem::Deactivate(hitbox_.handle); // 怯みで止められたときも、窓を開けっぱなしにしない
	}
}


//=============================================================================
// 攻撃表に出す値
//=============================================================================
// Tip は変数の右に書いたコメントと同じ文（2026-11-07）
void StationaryAttack::Fields(ComponentUI& ui) {
	ui.ClipField("Windup Clip###windupClip", windupClip_, Tip("予備動作のアニメーション"));
	ui.ClipField("Active Clip###activeClip", activeClip_, Tip("攻撃のアニメーション"));
	ui.ClipField("Recovery Clip###recoveryClip", recoveryClip_, Tip("硬直のアニメーション"));
	ui.Field("Hitbox###hitbox", hitboxName_, Tip("窓を合わせる Hitbox の子の Entity の名前"));
	ui.Field("Face Target In Windup###faceTargetInWindup", faceTargetInWindup_, Tip("予備動作の間、相手の方へ向き続けるか"));
	ui.Field("Turn Smooth Time###turnSmoothTime", turnSmoothTime_, AtLeast(0.0f).Tip("向く速さ"));
}
```

### D-3 `project/Source/Scripts/Enemy/AttackParts/Pounce.cpp`（飛びかかり）

**空中の速さの出し方（解説）**：放物線の上の位置を「跳んでからの秒 → 位置」の関数（`CalcLeapPosition`）にしておき、毎フレーム「このフレームの頭の位置」と「終わりの位置」の差 ÷ このフレームの秒を、Character Controller の velocity に書く。回避ジャンプの横の進み方（進んだ割合の差 ÷ 時間）と同じ考え方で、フレームの速さが変わっても、最後にちょうど着地点に着く。高さの式 `4 × H × s × (1 − s)` は、s（進んだ割合）が 0 と 1 で 0、0.5 で H になる2次式。

**空中の秒を数え直す理由**：攻撃表は Script より後に回り、`stageTime` はこのフレームの秒を足した後の値で部品に渡る。跳ぶフレームの頭から予備動作の終わりまでを空中にするので、`stageDuration − stageTime + deltaTime`。予備動作の終わりは攻撃の段階の頭なので、着地と「当たる」が同じフレームになる。

**Pounce Impact を攻撃の始めに作る理由**：`GameObject::Instantiate` で作った Entity の Component（Hitbox など）は、次のフレームの頭に付く。跳ぶフレームに作ると、そのフレームには Hitbox が無く、予告（Warn）を始められない。かがむ間に付くよう、攻撃の始め（OnBegin）に作り、跳ぶまでは Off のまま置いておく。

```cpp
#include <format>

#include <HTTEngine/Components/Combat/HitboxComponent.h>
#include <HTTEngine/Components/Physics/CharacterControllerComponent.h>
#include <HTTEngine/Diagnostics/LogManager.h>
#include <HTTEngine/Math/MathUtility.h>
#include <HTTEngine/Systems/Combat/HitSystem.h>

#include "GroundAttackPart.h"

// 攻撃部品はほかのファイルから使わない（攻撃表が名前から作る）ので、.h を作らずに .cpp の中にクラスを書く


/// <summary>
/// 飛びかかり：かがむ → 放物線で跳ぶ → 着地点に円の攻撃
/// <para>攻撃表の段階：予備動作 ＝ かがむ（予備動作の秒 − Leap Seconds）＋ 空中（Leap Seconds）、攻撃 ＝ 着地の瞬間、硬直 ＝ 戻り</para>
/// <para>跳ぶ瞬間に着地点（そのときの相手の位置。遠ければ Max Leap Distance まで）を決め、Impact Prefab をそこへ動かして予告する</para>
/// <para>空中は Character Controller の重力を切り、放物線の上の位置の差を速さにして縦も横も書く（着地が攻撃の段階の頭とぴったり合う）</para>
/// </summary>
class Pounce : public GroundAttackPart {
public:
	void OnBegin(const AttackPartContext& context) override;
	void OnStage(const AttackPartContext& context) override;
	void OnUpdate(const AttackPartContext& context) override;
	void OnEnd(const AttackPartContext& context, AttackPartEnd reason) override;
	void Fields(ComponentUI& ui) override;

private:
	void StartLeap(const AttackPartContext& context);  // 跳ぶ（着地点を決める・予告を置く・重力を切る）
	void UpdateLeap(const AttackPartContext& context); // 空中（放物線の上を進む速さを書く）
	void RestoreGravity(const AttackPartContext& context); // 切った重力を戻す（着地・途中で止められたとき）
	Vector3 CalcLeapPosition(float elapsed) const;     // 跳んでから elapsed 秒の位置（放物線）
	float CalcCrouchSeconds(const AttackPartContext& context) const; // かがむ秒（予備動作の秒 − Leap Seconds。0 より小さくならない）

	// ===== 攻撃表に出す値 =====
	float leapSeconds_ = 0.8f;        // 空中の秒（予備動作の終わりのこの秒が空中。ジャスト回避の読みやすさに効く）
	float leapHeight_ = 3.0f;         // 跳ぶ一番高い所（m。跳んだ床からの高さ）
	float maxLeapDistance_ = 14.0f;   // 届く距離（m。相手がこれより遠ければ、その向きのまま、この距離の所に降りる）
	float turnSmoothTime_ = 0.15f;    // かがむ間に相手の方へ向く速さ（秒。小さいほど速く向く）
	uint32_t impactPrefab_ = 0;       // 着地点に置く当たり（Hitbox の Circle・Damage Source・Hitbox Telegraph を付けた Prefab）
	AnimationClipName crouchClip_;    // かがむアニメ（かがむ秒で流しきる）
	AnimationClipName leapClip_;      // 空中のアニメ（空中の秒で流しきる。その場で跳ぶ姿勢だけ。位置はコードが動かす）
	AnimationClipName activeClip_;    // 着地のアニメ（攻撃の秒で流しきる）
	AnimationClipName recoveryClip_;  // 戻りのアニメ（硬直の秒で流しきる）

	// ===== 実行中だけの値 =====
	GameObject impact_;                       // 着地点の当たり（OnBegin で Prefab から作り、OnEnd で消す）
	bool hasLeapt_ = false;                   // もう跳んだか
	bool isGravityOff_ = false;               // 重力を切っているか（戻し忘れないように覚える）
	bool savedUseGravity_ = true;             // 切る前の Use Gravity
	Vector3 leapStart_ = {0.0f, 0.0f, 0.0f};  // 跳んだ所（ワールド）
	Vector3 leapEnd_ = {0.0f, 0.0f, 0.0f};    // 着地点（ワールド。高さは跳んだ床と同じ）
	float leapDuration_ = 0.0f;               // 実際の空中の秒（跳んだフレームから、予備動作の終わりまで）
	float leapElapsed_ = 0.0f;                // 跳んでからの秒
};
ATTACK_PART(Pounce, "飛びかかり。かがむ → 放物線で跳ぶ（着地点は跳ぶ瞬間に決める）→ 着地点に円の攻撃。予備動作 ＝ かがむ＋空中、攻撃 ＝ 着地の瞬間、硬直 ＝ 戻り");

namespace {
constexpr float kMinLeapDistance = 0.01f; // これより近ければ、その場で跳ぶ（向きは今のまま）
} // namespace


//=============================================================================
// 始め・段階・毎フレーム・終わり
//=============================================================================
// ===== 始め（攻撃に選ばれた）=====
// 着地点の当たりは、ここで先に作っておく。GameObject::Instantiate の Component（Hitbox など）が付くのは次のフレームの頭なので、
// 跳ぶフレームに作ると、そのフレームには Hitbox が無く、予告を始められない（かがむ間に付く）
void Pounce::OnBegin(const AttackPartContext& context) {
	hasLeapt_ = false;
	isGravityOff_ = false;
	impact_ = {};
	if (impactPrefab_ == 0) {
		LogManager::Warning("Pounce：Impact Prefab が選ばれていません（当たりなしで、跳ぶだけ）");
		return;
	}
	impact_ = GameObject::Instantiate(impactPrefab_, GetWorldPosition(context.self.transform)); // 跳ぶまでは Off なので、どこに置いても見えない・当たらない
}

// ===== 段階に入った =====
void Pounce::OnStage(const AttackPartContext& context) {
	switch (context.stage) {
	case AttackStage::Windup:
		SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f});
		PlayFitted(context, crouchClip_.View(), CalcCrouchSeconds(context));
		break;
	case AttackStage::Active:
		// 着地の瞬間。このフレームの空中の最後の速さは、もう書いてある（ここで消すと最後の一歩が進まない）。重力を戻して床に着かせる
		RestoreGravity(context);
		if (impact_) {
			SyncHitbox(impact_, context); // 当たる（新しい攻撃の回。攻撃の秒が過ぎたら、Hitbox の方でも閉じる）
		}
		PlayFitted(context, activeClip_.View(), context.stageDuration);
		break;
	case AttackStage::Recovery:
		if (impact_) {
			SyncHitbox(impact_, context); // 閉じる
		}
		SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f});
		PlayFitted(context, recoveryClip_.View(), context.stageDuration);
		break;
	case AttackStage::None:
		break;
	}
}

// ===== 毎フレーム =====
void Pounce::OnUpdate(const AttackPartContext& context) {
	if (context.stage != AttackStage::Windup) {
		SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f}); // 着地の後は動かない
		return;
	}
	// --- かがむ：相手の方へ向き続けて、かがむ秒が過ぎたら跳ぶ ---
	if (!hasLeapt_) {
		if (context.stageTime < CalcCrouchSeconds(context)) {
			Vector3 direction = {0.0f, 0.0f, 0.0f};
			float distance = 0.0f;
			if (CalcToTarget(context, direction, distance)) {
				TurnToward(context, direction, turnSmoothTime_);
			}
			return;
		}
		StartLeap(context);
	}
	// --- 空中 ---
	UpdateLeap(context);
}

// ===== 終わり（途中で止められたときも呼ばれる）=====
void Pounce::OnEnd(const AttackPartContext& context, AttackPartEnd /*reason*/) {
	RestoreGravity(context); // 空中で怯んだら、ここから重力で落ちる
	SetHorizontalVelocity(context, {0.0f, 0.0f, 0.0f});
	if (impact_) {
		HitSystem::Deactivate(impact_.handle);
		EntityManager::Destroy(impact_.handle); // 攻撃のたびに作るので、終わったら消す
	}
	impact_ = {};
}


//=============================================================================
// 跳ぶ・空中
//=============================================================================
// ===== 跳ぶ =====
void Pounce::StartLeap(const AttackPartContext& context) {
	hasLeapt_ = true;

	// --- 着地点：今の相手の位置（地面に平ら・高さは跳んだ床）。先読みはしない（円を見てから避ける遊び）---
	leapStart_ = GetWorldPosition(context.self.transform);
	Vector3 direction = MathUtility::YawToDirection(GetEulerAngles(context.self.transform).y); // 相手が居ない・真上に居るときは前へ
	float distance = 0.0f;
	CalcToTarget(context, direction, distance);
	distance = (std::min)(distance, maxLeapDistance_);
	leapEnd_ = leapStart_ + direction * distance;

	// --- 空中の秒：このフレームの頭から、予備動作の終わりまで（stageTime はこのフレームの分を足した後なので、1フレーム分を戻す）---
	// 予備動作の終わり＝攻撃の段階の頭なので、着地と「当たる」がぴったり合う
	leapDuration_ = (std::max)(context.stageDuration - context.stageTime + context.deltaTime, context.deltaTime);
	leapElapsed_ = 0.0f;

	// --- 着地点の向きへぴったり向く（空中では回らない）---
	if (distance > kMinLeapDistance) {
		SetEulerAngles(context.self.transform, {0.0f, MathUtility::DirectionToYaw(direction), 0.0f});
	}

	// --- 予告：着地点の当たりを動かして、空中の間 Warning（Just Lead Time 秒前から、ジャスト判定の範囲が有効）---
	if (impact_) {
		impact_.transform.translation = leapEnd_; // 一番上の Entity なので、translation がそのままワールドの位置
		if (HitboxComponent* hitbox = impact_.GetComponent<HitboxComponent>()) {
			hitbox->instigator = EntityManager::RefOf(context.self.handle); // 撃った本人＝この敵（敵とその子には当たらない・知らせが敵に届く）
		}
		HitSystem::Warn(impact_.handle, leapDuration_);
	}

	// --- 重力を切る（縦もこの部品が書く）---
	if (CharacterControllerComponent* controller = context.self.GetComponent<CharacterControllerComponent>()) {
		savedUseGravity_ = controller->useGravity;
		controller->useGravity = false;
		isGravityOff_ = true;
	}

	PlayFitted(context, leapClip_.View(), leapDuration_);
}

// ===== 空中 =====
// 放物線の上で「このフレームの頭の位置 → 終わりの位置」の差を、このフレームの秒で割った速さを書く（位置の差なので、フレームの速さに左右されない）
// 書いた速さで動かすのは、この後の Character Controller（壁・天井に当たれば、そこで止まる）
void Pounce::UpdateLeap(const AttackPartContext& context) {
	CharacterControllerComponent* controller = context.self.GetComponent<CharacterControllerComponent>();
	if (controller == nullptr || context.deltaTime <= 0.0f) {
		return;
	}
	const float next = (std::min)(leapElapsed_ + context.deltaTime, leapDuration_);
	controller->velocity = (CalcLeapPosition(next) - CalcLeapPosition(leapElapsed_)) / context.deltaTime;
	leapElapsed_ = next;
}

// ===== 跳んでから elapsed 秒の位置 =====
// 横は跳んだ所から着地点へ一定の速さ。高さは 4 × H × s × (1 − s)（s ＝ 進んだ割合 0〜1）。s ＝ 0.5 で H、0 と 1 で 0 の放物線
Vector3 Pounce::CalcLeapPosition(float elapsed) const {
	const float s = (leapDuration_ > 0.0f) ? (std::min)(elapsed / leapDuration_, 1.0f) : 1.0f;
	Vector3 position = Lerp(leapStart_, leapEnd_, s);
	position.y += 4.0f * leapHeight_ * s * (1.0f - s);
	return position;
}

// ===== 切った重力を戻す =====
void Pounce::RestoreGravity(const AttackPartContext& context) {
	if (!isGravityOff_) {
		return;
	}
	if (CharacterControllerComponent* controller = context.self.GetComponent<CharacterControllerComponent>()) {
		controller->useGravity = savedUseGravity_;
	}
	isGravityOff_ = false;
}

// ===== かがむ秒 =====
float Pounce::CalcCrouchSeconds(const AttackPartContext& context) const { return (std::max)(context.stageDuration - leapSeconds_, 0.0f); }


//=============================================================================
// 攻撃表に出す値
//=============================================================================
// Tip は変数の右に書いたコメントと同じ文（2026-11-07）
void Pounce::Fields(ComponentUI& ui) {
	ui.Field("Leap Seconds###leapSeconds", leapSeconds_, AtLeast(0.0f).Tip("空中にいる秒数"));
	ui.Field("Leap Height###leapHeight", leapHeight_, AtLeast(0.0f).Tip("跳ぶ一番高い所"));
	ui.Field("Max Leap Distance###maxLeapDistance", maxLeapDistance_, AtLeast(0.0f).Tip("届く距離"));
	ui.Field("Turn Smooth Time###turnSmoothTime", turnSmoothTime_, AtLeast(0.0f).Tip("かがむ間に相手の方へ向く速さ"));
	ui.AssetField("Impact Prefab###impactPrefab", impactPrefab_, AssetType::Prefab, Tip("着地点に置く当たり（Hitbox の Circle・Damage Source・Hitbox Telegraph を付けた Prefab）"));
	ui.ClipField("Crouch Clip###crouchClip", crouchClip_, Tip("かがむアニメーション"));
	ui.ClipField("Leap Clip###leapClip", leapClip_, Tip("空中のアニメーション"));
	ui.ClipField("Active Clip###activeClip", activeClip_, Tip("着地のアニメーション"));
	ui.ClipField("Recovery Clip###recoveryClip", recoveryClip_, Tip("戻りのアニメーション"));
}
```

---

## パート E：BossCat（新しいファイル）

攻撃表の状態を毎フレーム読み、変わった瞬間に怯み・フェーズの移行・撃破のアニメを流し、顔の表情を合わせる。今回は攻撃中＝怒り、それ以外＝普通（怯み＝当たった、撃破＝目が回る）。

**顔のずらし方**：顔の UV は BossFace.png の左上の1マス（U 0〜0.25・V 0〜0.5）に貼ってあるので、Model Renderer の uvTransform の translation を（列 × 0.25, 段 × 0.5）にすると、そのマスの顔になる。uvTransform はモデル全体に掛かるが、テクスチャがあるのは顔だけ（体は色だけ）なので、顔だけが変わる。

### E-1 `project/Source/Scripts/Enemy/BossCat.h`

```cpp
#pragma once
#include <HTTEngine/Components/Gameplay/AttackTableComponent.h>
#include <HTTEngine/ECS/Script.h>

// ボス（巨大ネコ）の体。ボスの Entity に付ける
// 受け持つのは「攻撃表の状態を読んで見せる」所だけ：顔の表情と、怯み・フェーズの移行・撃破のアニメ
// 攻撃の動き・待ちの間の動き（向く・近づく）は攻撃部品（Pounce・StationaryAttack・Chase）の仕事。ここでは動かさない
// 同じ Entity に付ける物：Model Renderer・Animator（Boss.gltf）・Character Controller・Collider（Capsule）・Health・Attack Table

/// <summary>
/// 顔の表情（BossFace.png の 4×2 のマス。上の段の左から 0〜3、下の段の左から 4〜7）
/// </summary>
enum class BossFace {
	Normal,    // 0 普通（半目）
	Angry,     // 1 怒り
	Sleep,     // 2 寝る
	Surprised, // 3 驚き
	MouthOpen, // 4 口を開ける
	Hurt,      // 5 当たった
	Dizzy,     // 6 目が回る
	Happy,     // 7 うれしい
};


/// <summary>
/// ボス（巨大ネコ）の体
/// </summary>
class BossCat : public Script {
public:
	void Start() override;
	void Update() override;

	// 顔の表情を変える（ほかの Script・演出からも呼べる）
	void SetFace(BossFace face);

private:
	void OnStateChanged(AttackTableState state, float duration); // 攻撃表の状態が変わった瞬間（怯み・移行・撃破のアニメ）
	static BossFace ChooseFace(AttackTableState state);          // 状態ごとの顔

	// ===== 実行中だけの値（保存しない）=====
	AttackTableState lastState_ = AttackTableState::Selecting; // 前のフレームの攻撃表の状態（変わった瞬間を知るため）
	BossFace face_ = BossFace::Normal;                         // 今の顔
};
```

### E-2 `project/Source/Scripts/Enemy/BossCat.cpp`

```cpp
#include "BossCat.h"

#include <string_view>

#include <HTTEngine/Components/Rendering3D/ModelRendererComponent.h>
#include <HTTEngine/Systems/Animation/AnimationSystem.h>
#include <HTTEngine/Systems/Gameplay/AttackTableSystem.h>

SCRIPT(BossCat, "Enemy");

namespace {
// --- アニメの名前（Boss.gltf の中の名前＝Blender のアクションの名前）。攻撃・待ちのアニメは攻撃表の部品の値に書く ---
constexpr std::string_view kClipStagger = "Stagger";         // 怯み（怯みの秒で流しきる）
constexpr std::string_view kClipPhaseChange = "PhaseChange"; // フェーズの移行（移行の秒で流しきる）
constexpr std::string_view kClipDown = "Down";               // 撃破（1回だけ。最後の形のまま止まる）

// --- 顔のマス（BossFace.png は横 4 × 縦 2。顔の UV は左上の 1 マスに貼ってあるので、マスの大きさずつずらす）---
constexpr int kFaceColumns = 4;                          // 横のマスの数
constexpr float kFaceCellWidth = 1.0f / kFaceColumns;    // 1 マスの横の大きさ（UV）
constexpr float kFaceCellHeight = 1.0f / 2.0f;           // 1 マスの縦の大きさ（UV。下の段は V が大きい方）
} // namespace


//=============================================================================
// エンジンが呼ぶ関数
//=============================================================================
// ===== 始め =====
void BossCat::Start() { SetFace(BossFace::Normal); }

// ===== 毎フレーム =====
// 攻撃表の状態を読んで、変わった瞬間にアニメを流し、顔を合わせる
// Script は攻撃表の AI より先に回るので、読むのは前のフレームの終わりの状態（見せるだけなので 1 フレーム遅れてよい）
void BossCat::Update() {
	const AttackTableStatus status = AttackTableSystem::GetStatus(gameObject.handle);
	if (!status.isRunning) {
		return; // まだ動き始めていない（攻撃表が選ばれていない・最初のフレーム）
	}
	if (status.state != lastState_) {
		lastState_ = status.state;
		OnStateChanged(status.state, status.duration);
	}
	SetFace(ChooseFace(status.state));
}


//=============================================================================
// 顔の表情
//=============================================================================
// ===== 顔を変える =====
// Model Renderer の uvTransform はモデル全体に掛かるが、テクスチャを貼ってあるのは顔だけ（体は色だけ）なので、顔だけがずれる
void BossCat::SetFace(BossFace face) {
	face_ = face;
	ModelRendererComponent* renderer = GetComponent<ModelRendererComponent>();
	if (renderer == nullptr) {
		return;
	}
	const int index = static_cast<int>(face);
	renderer->uvTransform.translation = {static_cast<float>(index % kFaceColumns) * kFaceCellWidth, static_cast<float>(index / kFaceColumns) * kFaceCellHeight, 0.0f};
}

// ===== 状態ごとの顔 =====
BossFace BossCat::ChooseFace(AttackTableState state) {
	switch (state) {
	case AttackTableState::Attacking:
		return BossFace::Angry;
	case AttackTableState::Staggered:
		return BossFace::Hurt;
	case AttackTableState::Defeated:
		return BossFace::Dizzy;
	case AttackTableState::Selecting:
	case AttackTableState::PhaseTransition:
		return BossFace::Normal;
	}
	return BossFace::Normal;
}


//=============================================================================
// 状態が変わった瞬間
//=============================================================================
// 攻撃・待ちの間のアニメは部品が流すので、ここは部品が動かない状態（怯み・フェーズの移行・撃破）だけ
void BossCat::OnStateChanged(AttackTableState state, float duration) {
	const Handle<Entity> self = gameObject.handle;
	switch (state) {
	case AttackTableState::Staggered:
		AnimationSystem::Play(self, kClipStagger, false, AnimationSystem::CalcSpeedToFit(self, kClipStagger, duration));
		AnimationSystem::Restart(self);
		break;
	case AttackTableState::PhaseTransition:
		AnimationSystem::Play(self, kClipPhaseChange, false, AnimationSystem::CalcSpeedToFit(self, kClipPhaseChange, duration));
		AnimationSystem::Restart(self);
		break;
	case AttackTableState::Defeated:
		AnimationSystem::Play(self, kClipDown, false, 1.0f);
		AnimationSystem::Restart(self);
		break;
	case AttackTableState::Selecting:
	case AttackTableState::Attacking:
		break;
	}
}
```

---

## パート E2：HitBlink（新しいファイル・2026-11-07 に追加）

被弾の後の無敵（Health の Invincible After Hit）の間、Model Renderer の **Enabled** を切り替えて点滅させる。

- **何を見るか**：Health の `invincibleTimer`（被弾の後の無敵の残りの秒）。エンジンの DamageSystem が、当たったフレームに `Invincible After Hit` を入れて、毎フレーム減らす。回避の無敵は `invincible`（PlayerController が書く）の方なので、点滅しない。
- **いつ動くか**：`LateUpdate`（System の後）。DamageSystem は System なので、`Update` だと当たったフレームにはまだ残りが入っておらず、1フレーム遅れて点滅が始まる。
- **倒れたら**：点滅をやめて表示に戻す（倒れる姿を消さない）。
- **Enabled を触るのは点滅中だけ**：点滅していないときに毎フレーム `true` を書くと、ほかの処理（演出で消すなど）が消した物を出してしまう。なので、点滅の終わりに1回だけ `true` に戻す。Script が無効になったとき（`OnDisable`）も戻す。
- 写した `hitChangeAlphaTime_`（0.15）は、ここの `blinkInterval_` になる。半透明ではなく消える・出るなので、名前を変えた。0.1 は前より少し速い点滅で、好みで変えてよい。

### E2-1 `project/Source/Scripts/Combat/HitBlink.h`

```cpp
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
```

### E2-2 `project/Source/Scripts/Combat/HitBlink.cpp`

```cpp
#include "HitBlink.h"

#include <cmath>

#include <HTTEngine/Components/Combat/HealthComponent.h>
#include <HTTEngine/Components/Rendering3D/ModelRendererComponent.h>

SCRIPT(HitBlink, "Combat");


//=============================================================================
// 更新
//=============================================================================
// ===== 毎フレーム（System の後）=====
// 被弾の後の無敵の残り（Health の invincibleTimer）は、DamageSystem が当たったフレームに入れる
// LateUpdate なら System の後なので、当たったそのフレームから点滅が始まる
void HitBlink::LateUpdate() {
	// --- 点滅するか ---
	const HealthComponent* health = GetComponent<HealthComponent>();
	const bool shouldBlink = (health != nullptr) && (health->invincibleTimer > 0.0f) && !health->IsDefeated() && (blinkInterval_ > 0.0f);
	if (!shouldBlink) {
		StopBlink(); // 無敵が切れた・倒れた（倒れた姿は消さない）
		return;
	}

	// --- 点滅の始め ---
	if (!isBlinking_) {
		isBlinking_ = true;
		blinkElapsed_ = 0.0f;
	}

	// --- 消える・出る ---
	// 2 区間で 1 回の点滅。前半は消す（当たってすぐ消えるので、当たったことが分かりやすい）、後半は出す
	const bool isVisible = std::fmod(blinkElapsed_, blinkInterval_ * 2.0f) >= blinkInterval_;
	SetVisible(isVisible);
	blinkElapsed_ += Time::GetDeltaTime(); // ヒットストップ中は 0 なので、点滅も止まる
}

// ===== 無効になった・消える前 =====
void HitBlink::OnDisable() { StopBlink(); } // 消えたまま残らないように


//=============================================================================
// 道具
//=============================================================================
// ===== 点滅を終える =====
void HitBlink::StopBlink() {
	if (!isBlinking_) {
		return; // 点滅していないときは Enabled を触らない（ほかの処理が消している物を出してしまわないように）
	}
	isBlinking_ = false;
	SetVisible(true);
}

// ===== 表示を切り替える =====
void HitBlink::SetVisible(bool visible) {
	if (ModelRendererComponent* renderer = GetComponent<ModelRendererComponent>()) {
		renderer->enabled = visible;
	}
}


//=============================================================================
// Inspector に出す・保存する値
//=============================================================================
void HitBlink::Fields(ComponentUI& ui) {
	ui.Field("Blink Interval###blinkInterval", blinkInterval_, AtLeast(0.0f).Tip("消える・出るを切り替える秒（小さいほど速く点滅する）"));
}
```

`std::fmod(a, b)` は「a を b で割った余り」の float 版（`%` は整数にしか使えない）。`blinkElapsed_` を「消える＋出る」の長さ（`blinkInterval_ × 2`）で割った余りが、今の1回の点滅の中で何秒目かになる。それが前半（`blinkInterval_` より小さい）なら消し、後半なら出す。

---

## パート F：VS に足して、ビルドする

1. ソリューションエクスプローラーで `Source/Scripts` の下にフィルター `Enemy`、その下に `AttackParts` を作る。`Source/Scripts` の下に `Combat` も作る。
2. 新しいファイルを **右クリック → 追加 → 既存の項目** で入れる（入れ忘れるとリンクエラー。部品は登録されず、攻撃表に「部品がありません」と出る）。

| フィルター | ファイル |
|---|---|
| Source/Scripts/Combat | `HitBlink.h`・`HitBlink.cpp` |
| Source/Scripts/Player | `JustCounter.h`・`JustCounter.cpp` |
| Source/Scripts/Enemy | `BossCat.h`・`BossCat.cpp` |
| Source/Scripts/Enemy/AttackParts | `GroundAttackPart.h`・`GroundAttackPart.cpp`・`Chase.cpp`・`StationaryAttack.cpp`・`Pounce.cpp` |

3. Debug・Development・Release でビルドする。

---

## パート G：シーン・Prefab・攻撃表（エディタ）

### G-1 レイヤー

1. **Settings → Layers** で、1 に `Player`、2 に `Enemy` と入れる（`project/Parameters/Layers/Layers.json` ができる）。
2. Player の Inspector の名前の下の **Layer** を `Player` にする。

### G-2 プレイヤー

Player の **PlayerController** の欄が変わったので確かめる（前の Acceleration・Turn Sharpness は読まれず、初めの値が入る。シーンを保存すると古い値は消える）。

| 欄 | 値 | 意味 |
|---|---|---|
| Move Smooth Time | 0.05 | 前の Acceleration 20 と同じ動き |
| Turn Smooth Time | 0.055 | 前の Turn Sharpness 18 と同じ動き |
| Hit Stun Time | 0.4 | のけぞりの秒 |
| Hit Knockback Distance | 1 | 押される距離（m） |

**Add Component** で次を足す。

| Component | 欄 | 値 | 意味 |
|---|---|---|---|
| Health | Max Health・Health | 5・5 | ハートの数 |
| | Invincible After Hit | 1.0 | 当たった後、攻撃を断る秒。**当たった瞬間から数える**ので、Hit Stun Time（0.4）より長くする。差の 0.6 秒が「動けるのに無敵」の時間 |
| Hurtbox | Shape・Center・Radius | Sphere・(0, 0.45, 0)・0.4 | 押し合う Collider（半径 0.45）より少し小さく |
| JustCounter（Player） | — | — | ジャスト回避の入口 |
| HitBlink（Combat） | Blink Interval | 0.1 | 被弾の後の無敵の間、消える・出るを切り替える秒 |

### G-3 ボス

1. 空の Entity `Boss` を作り、Layer を `Enemy` にする。位置・向きは G-6 の台地に合わせる（仮置きの値は G-6）。
2. 次の Component を足す。

| Component | 欄 | 値 | 意味 |
|---|---|---|---|
| Model Renderer | Model | `resources/Model/Boss/Boss.gltf` | |
| Animator | Animation・Clip・Loop | Boss.gltf・`Idle`・ON | 待ちの部品が入れば Chase が流す |
| Character Controller | Gravity Scale | 3 | プレイヤーと同じ落ち方。飛びかかりの空中だけ、部品が重力を切る |
| | Push Rigidbodies | OFF | 大きいので、押すと物が飛んでいく |
| Collider | Shape・Center・Radius・Height | Capsule・(0, 2.2, −0.6)・1.9・4.4 | 胴を包む立ったカプセル（下の端が足元） |
| Health | Max Health・Health | 1000・1000 | 今回は減らない（撃つのは次の作業）。攻撃表が HP の割合をここから読む |
| BossCat（Enemy） | — | — | 顔の表情・状態のアニメ |

3. 子の Entity を3つ作り、どれも **Layer を `Enemy`** にする（レイヤーは子に写らない）。Hurtbox を付ける。

| 名前 | Shape | Center | Radius |
|---|---|---|---|
| `Hurtbox Head` | Sphere | (0, 3.7, 2.7) | 1.2 |
| `Hurtbox Front` | Sphere | (0, 2.3, 0.3) | 1.8 |
| `Hurtbox Rear` | Sphere | (0, 2.1, −2.3) | 1.8 |

View → Gizmos の線で見て、ネコの体に合わせて直す（Boss.gltf：横 ±1.9・高さ 0〜4.9・鼻 +3.7〜しっぽ −4.3）。

4. 子の Entity `Tail Sweep Hitbox`（Layer `Enemy`）を作り、次を付ける。名前は攻撃表の Hitbox の欄と同じにする。

| Component | 欄 | 値 | 意味 |
|---|---|---|---|
| Hitbox | Shape | Circle・Radius 5・Height 3 | しっぽの届く所 |
| | Targets | Player | |
| | Phase | Off | 窓は部品が開ける |
| | Just Margin・Just Lead Time | 0.5・0.3 | 設計書の「狭めに」。遊びながら詰める |
| Damage Source | Damage | 1 | ハート1個 |
| | Hit Stop・Shake | 0.08・0.3 | 手応え |
| Hitbox Telegraph | Material | 空（エンジンの赤い塗り） | |

### G-4 飛びかかりの予告の Prefab

1. 空の Entity `Pounce Impact` を作る（どこでもよい）。Layer は `Enemy`。
2. 次を付ける。

| Component | 欄 | 値 |
|---|---|---|
| Hitbox | Shape | Circle・Radius 3.5・Height 3 |
| | Targets・Phase | Player・Off |
| | Just Margin・Just Lead Time | 0.5・0.25 |
| Damage Source | Damage・Hit Stop・Shake | 1・0.12・0.6 |
| Hitbox Telegraph | Material | 空 |

3. Hierarchy の右クリック → **Save As Prefab**（`resources/prefabs/Pounce Impact.prefab`）。シーンの物は消す。

Instigator は Pounce が跳ぶときにボスを書くので、Prefab では空のままでよい。

### G-5 攻撃表

1. Project ウィンドウで `resources/AttackTables/` を作り、**Create → Attack Table** で `Boss.attacktable` を作る。
2. 選んで Inspector で次のように書く（Sample Animation を先に選ぶと、Clip の欄に Boss.gltf の名前の候補が出る）。

| 欄 | 値 |
|---|---|
| Max Repeat | 2 |
| Sample Animation | `resources/Model/Boss/Boss.gltf` |

**Phase 1**

| 欄 | 値 | 意味 |
|---|---|---|
| Min Idle Seconds | 0.8 | 攻撃の後、次の攻撃を選ぶまでの最短の秒 |
| Idle Part | Chase | |
| Move Speed・Stop Distance・Resume Margin | 4・5・0.5 | しっぽの Max Distance（7）より手前で止まる |
| Turn Smooth Time | 0.2 | |
| Idle Clip・Walk Clip・Walk Anim Speed | `Idle`・`Walk`・1 | |

**Attack：Pounce**

| 欄 | 値 |
|---|---|
| Part | Pounce |
| Min Distance・Max Distance・Cooldown | 6・20・3 |
| Windup・Active・Recovery Seconds | 1.6・0.15・1.0（かがむ 0.8＋空中 0.8） |
| Leap Seconds・Leap Height・Max Leap Distance | 0.8・3・14 |
| Turn Smooth Time | 0.15 |
| Impact Prefab | `resources/prefabs/Pounce Impact.prefab` |
| Crouch・Leap・Active・Recovery Clip | `Pounce_Windup`・`Pounce_Leap`・`Pounce_Active`・`Pounce_Recovery` |

**Attack：Tail Sweep**

| 欄 | 値 |
|---|---|
| Part | StationaryAttack |
| Min Distance・Max Distance・Cooldown | 0・7・2 |
| Windup・Active・Recovery Seconds | 0.9・0.3・0.8 |
| Windup・Active・Recovery Clip | `TailSweep_Windup`・`TailSweep_Active`・`TailSweep_Recovery` |
| Hitbox | `Tail Sweep Hitbox` |
| Face Target In Windup | OFF |

3. **Save**。Boss に **Attack Table** を足し、Table に `Boss.attacktable`、Target に Player をドラッグする。

### G-6 ボス部屋の仮置き（作り方も大きさも、作りながら変える）

Polygon Shape は「横から見た X・Y の形を Z へ押し出す」形で、回せるのは Z のまわりだけなので、坂は X の向きに上る。多角形の Collider は球・カプセルとだけ当たる（プレイヤーは Sphere、ボスは Capsule なので乗れる）。

| Entity | 付ける物 | 最初の値 |
|---|---|---|
| `Boss Plateau` | Primitive Renderer（Box）・Collider（Box） | 位置 (0, 1.5, 0)・大きさ (32, 3, 32)。上の面は高さ 3 |
| `Boss Slope` | Polygon Shape・Polygon Renderer・Collider（Polygon） | 点 (−26, 0)・(−16, 0)・(−16, 3)・奥行き 6 |
| `Boss Wall N/S/E/W` | Collider（Box）だけ | 台地の縁に高さ 4・厚さ 1。坂の口（−X の側の Z −3〜3）は空ける |

Player は (−30, 0, 0)（坂の下）、Boss は (6, 3, 0)・回転 (0, −90, 0)（坂の方を向く）。

---

## パート H：確かめる

| 確かめること | やり方 | 見る所 |
|---|---|---|
| 近づいて向く | 坂を上る | Boss が Walk で寄ってきて、5 m くらいで止まってこちらを向く。顔は普通 |
| しっぽ | 近くで待つ | 床に半径 5 の円 → 1回転で当たる → Health が 1 減る・押し出される・Hit のアニメ。攻撃中の顔は怒り |
| 飛びかかり | 6 m より離れる | かがむ → 跳んだ瞬間に足元に円 → 着地で当たる |
| ただよける | 円の中で、早めに跳ぶ | 当たらない。ログにジャストは出ない |
| ジャスト回避 | 当たる直前（Just Lead Time の中）に跳ぶ | ログに「ジャスト回避 n 回目：（攻撃の名前）…」・JustCounter の欄の回数が増える・その攻撃には当たらない |
| 倒れる | Health を 1 にして当たる | Down のアニメ・操作できない・ログに「プレイヤーが倒れた」 |
| 回避の待ち時間 | 跳んで着地してすぐ Space | 1 秒たつまで跳ばない |
| 被弾の後の点滅 | わざと当たる | 当たった瞬間から点滅。のけぞりが終わって動けるようになっても少し点滅が続き、その間は当たらない。点滅が終わると表示に戻る。回避ジャンプでは点滅しない。倒れたときは点滅しない |

### 作業用コピーで確かめたこと（2026-11-07）

作業用コピーに平らな床・Player・Boss・G-3〜G-5 と同じ物を置いたシーンを作り、Release で動かした（入力はしていない。ジャスト回避は、作業用コピーだけに「ジャスト判定に入ったら自動で跳ぶ」仮のコードを入れて確かめ、確かめた後に外した）。

| 確かめたこと | 結果 |
|---|---|
| 待ち（Chase） | 始めに Min Idle Seconds（0.8 秒）の間プレイヤーへ歩き、その後に飛びかかりを選んだ |
| 飛びかかり | かがむ → 跳ぶ → 着地で `被弾：Pounce Impact（1 減って、残り 4）` |
| しっぽ | 床に半径 5 の赤い円 → 1回転 → `被弾：Tail Sweep Hitbox（1 減って、残り 3）`。そのまま続けて残り 2・1・0 |
| 押し戻し | 当たるたびに 1 m ずつ外へ（Hit Knockback Distance 1） |
| 倒れる | 残り 0 で `プレイヤーが倒れた…`。その後は動かない・当たらない |
| ジャスト回避 | 飛びかかり・しっぽの両方で `ジャスト回避 n 回目：（攻撃の名前）（Boss の攻撃・当たる 0.12 秒前）…`。その攻撃には当たらなかった |
| 顔 | 待ちの間は半目（普通）、攻撃中は怒り |
| ビルド | Debug・Development・Release で、このコードからの警告なし |

パート 0 で書き出し直した Boss.gltf でも、読み込み → 飛びかかり → しっぽ → 倒れる、まで同じに動いた。

**0.1 の追加分（2026-11-07）**：写した後のコード（自分で付けた名前・コメント）に、0.1 の表のとおり足して確かめた。Debug・Development・Release で、このコードからの警告なし。仮のログを入れて Release で動かした結果は次のとおり（ログは確かめた後に外した）。

| 確かめたこと | 結果 |
|---|---|
| 点滅の始め | 当たったフレームから消える（Health の残り 1.00 秒） |
| 点滅の速さ | 0.1 秒ごとに消える・出るが入れ替わる |
| のけぞりの後 | 0.4 秒でのけぞりが終わって動けるようになった後も、1.0 秒まで点滅が続いた |
| 点滅の終わり | 1.0 秒で表示に戻った（4回の被弾すべて） |
| 倒れたとき | 残り 0 の被弾では点滅しなかった |

確かめていないこと：本物のキー・パッドでの操作、エディタでの手順（G 章）、坂と台地（G-6）。

---

## 知っておくこと

- **しっぽの1回転は root を回すアニメ**なので、子の Entity の Hurtbox は回らない（回っている短い間、頭の球は前のまま）。骨に物を付ける機能が要るが、設計書で「ネコには要らない」と決めたので今回はこのまま。
- **ボスの Collider は立ったカプセル1つ**（Character Controller は球・カプセルしか動かせず、カプセルはいつも立ったまま）。横に長いネコの頭としっぽは、はみ出す。押し合いが気になったら、子の Entity に Collider を足す（自分の子とは当たらないので、ボスの動きは止まらない）。
- **飛びかかりで、ボスはプレイヤーの上には降りきれない**。ボスのカプセルとプレイヤーの球がぶつかり、Character Controller が「ほかのキャラの頭には立たない」ので滑り落ちる（確かめたときは、着地点の 1.4 m 手前に降りた）。当たりは着地点の Prefab なので、「当たる」時刻と場所はずれない。攻撃の間だけボスがプレイヤーをすり抜けるようにするには、Collider が「当たる相手」を選べる仕組み（Unity のレイヤーの当たりの表）がエンジンに要る。気になったら、エンジンのチャットで相談する。
- **エンジンの小さな間違い（見つけた物）**：Collider の Capsule の Height は、保存のキーが `"height "`（後ろに空白）になっている（`CollisionSystem.cpp` の `"Height###height "`）。エディタで入れれば困らないが、シーンの JSON を手で書くときは空白が要る。直すならエンジンのチャットで（直すと、今のシーンの値が読まれなくなるので、読み替えも要る）。

---

## 次の作業

- ジャスト反撃の中身（スロー・背中側のカメラ・縮む輪・A の判定・UI・ゲージ）→ `JustCounter` と `PlayerState::Counter`、`IsInvincible()` に「JustCounter が動いている間」を足す
- ハート・回避の輪の UI（Health・`GetDodgeCooldownRatio()`・`CanDodge()` を読む）
- 第2フェーズ・ほかの攻撃（毛玉・後ろへ跳ぶ・岩・猫パンチ・必殺）
- ボス部屋の作り方（地形の機能・モジュール）と大きさ
