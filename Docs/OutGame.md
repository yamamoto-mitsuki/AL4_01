# 設計：タイトル・ステージ選択・クリア画面（2026-10-26）

参考：
- 前の設計：[ParticleTitleClear.md](ParticleTitleClear.md) の 4〜7章（**この md がその続き。ずれたらこちらが正**）
- エンジン：`MyEngine_v1/project/MyEngine/UI/UIScreen.h`・`Components/UIScreenComponent.h`・`Scene/SceneTransition.h`・`Components/TextComponent.h`・`Systems/SpriteRenderSystem.h`（`CalcScreenQuad`・`IsInsideQuad`）・`Systems/TextRenderSystem.h`（`CalcTextSize`）・`Components/DepthOfFieldComponent.h`・`Time/EngineTime.h`（`HitStop`）・`Window/WindowManager.h`（`GetMainHWND`）
- ゲーム：`Source/Scripts/UI/PauseMenu.*`・`LevelInfo.*`・`GoalDoor.*`・`IceCube.h`（`Clear`・`IsPlayable`）

> 写経用のコードは段階ごとに別の md：**段階1 → [OutGame1.md](OutGame1.md)**。段階2 からは、前の段階を写して動いたのを確かめてから書く。

---

## 0. 結論：アプリケーション（ゲーム）側だけで作る

**エンジンは変えない**。要る物はもう全部エンジンにある。

| 要る物 | エンジンにある物 |
|---|---|
| 画面を重ねる・消す（動き付き） | `UIScreen::Open / Close / IsOpen`・UI Screen Component（Fade・Slide・Scale・Stagger・Pause Game） |
| シーンを幕で切り替える | `SceneTransition::LoadScene / ReloadScene / IsBusy` |
| 文字 | Text（`FixedString<256>`。`text = std::format(...)` で変えられる。縁取り・影・Glow・Shine・Wave / Bounce / Shake） |
| マウスで項目を選ぶ | Text の大きさ（`CalcTextSize`）＋置き方（`CalcScreenQuad`）＋`IsInsideQuad`。**Image を置かなくても文字に当たる** |
| 背景をぼかす | Depth Of Field Component（Title シーンに置くだけ。クリアの瞬間はコードで値を動かす） |
| 気持ちよさ | `Time::HitStop`・Particle System・UI Screen の Stagger・Easing（OutBack・OutElastic など） |
| 保存のファイル | nlohmann（`<externals/nlohmann/json.hpp>`） |
| ゲームを終わる | 関数は無いので、`GetMainHWND()` のウィンドウに `WM_CLOSE` を送る（製品版で終わるのを確かめた） |

### 見た目の決まり
**世界観：おしゃれで、静か**（2026-10-26。ユーザーの Title：夕焼けと霧の森、Noto Sans JP の細い白い文字、縁取りなし、左寄せ、項目は英語の Start / Quit）。

| 決まり | 中身 |
|---|---|
| 言葉 | **「！」は使わない**。はしゃいだ言葉（クリア！・ベスト更新！）を出さない。UI の言葉は**短い英語**（Start・Quit・Select Stage・Best・Locked・Next・Title）。ステージの名前は一覧で自由に |
| 文字 | Noto Sans JP・白。違いは**明るさ（透明度）と、ほんの少しの Glow** で出す。縁取り・影・Shine は基本使わない |
| 動き | **フェードと、少しのスライド（10〜20px）だけ**。ゆっくり（0.5〜1 秒・OutSine / OutCubic）。**跳ねる・震える・ポンと縮む・数え上げは使わない**（Bounce・Shake・OutBack・OutElastic は使わない） |
| 画像 | **数字は Image で作らない。Text の後ろに背景の Image を敷かない**。読みやすさは背景のぼかし（被写界深度）で出す |
| 選んだ項目 | 明るくなって、氷色の Glow がうっすら付くだけ（SelectedStyle） |

- ステージ選択のマスも Text だけ（枠の絵・カーソルの絵を作らない）。
- クリア画面でゲームの絵の上に文字が乗って読みにくいときは、**画像ではなく被写界深度でゲームの絵をぼかす**（画面全体を暗くする黒い Image も使わない）。

---

## 1. 画面の流れ

```
起動（製品版：最初のシーンは "Title"。エディタは前回開いていたシーンから）
 │
 ▼
[Title シーン]  背景はステージの背景をぼかして止めたまま
 │  Start ──→ ┌ StageSelect 画面（UI Screen。Title の上に重ねる。シーンは替えない）┐
 │              │  選ぶ・決める：そのステージへ（幕）   戻る：閉じてタイトルの項目へ   │
 │              └───────────────────────────────────────────┘
 │  Quit  ──→ 終了（エディタではログだけ）
 ▼
[Level01〜 シーン]  遊んだ時間を数える（ポーズ中・幕の間は数えない）
 ├ Esc ──→ Pause 画面：つづける / やりなおす / タイトルへ
 └ ゴールの扉に触れる → クリアの演出（6章）→ Clear 画面：Next / Title
       Next  → 次のステージ（幕）。最後のステージ・次のシーンが無いときは出さない
       Title → Title シーン（幕）。ステージ選択は開かない（Q1）
```

| 画面 | 種類 | ファイル | ゲームを止めるか |
|---|---|---|---|
| タイトル | シーン | `resources/scenes/Title.scene.json` | — |
| ステージ選択 | UI Screen | `resources/ui/StageSelect.uiscreen` | 止めない |
| ポーズ | UI Screen（今ある） | `resources/ui/Pause.uiscreen` | 止める |
| クリア | UI Screen | `resources/ui/Clear.uiscreen` | 止めない（ゴールのパーティクル・ぼかしの動きを止めないため。キューブはもう操作を受けない） |

---

## 2. データ

### 2.1 ゲームの名前 `GameTitle.h`（段階1）
```cpp
inline constexpr std::string_view kGameTitle = "森の中の氷";
```
- タイトルのロゴの Text と、ウィンドウのタイトルバーの両方がここを読む。**名前を変えるときはここだけ**。
- 名前の候補（森・氷・霧・ジャンプから）：

| 候補 | 読み | ひとこと |
|---|---|---|
| 霧氷ジャンプ | むひょうジャンプ | 霧氷＝霧が木について凍った物。森・氷・霧が1語に入る |
| 霧氷の森 | むひょうのもり | 場所の名前っぽい。静かな感じ |
| こおりの森を跳べ | こおりのもりをとべ | 子どもにも分かる。動きが伝わる |
| 霧の森とアイスキューブ | きりのもりとアイスキューブ | 主人公が分かる |
| とけるまえに | — | 森・霧は入らないが、熱湯で溶けるルールが伝わる |

### 2.2 ステージの一覧 `StageCatalog.h`（段階2）
```cpp
struct StageEntry {
	std::string_view scene; // シーンの名前（resources/scenes/<scene>.scene.json）
	std::string_view title; // ステージ選択・クリア画面に出す名前
};
// 数は書かない（std::to_array が並べた数から決める）。増やす・減らすのは、ここに1行足す・消すだけ（Q2）
inline constexpr auto kStages = std::to_array<StageEntry>({
    {"Level01", "ステージ 1"},
    {"Level02", "ステージ 2"},
    // …
});
```
- 次のステージは「一覧の次」。**LevelInfo の Next Scene はやめる**。
- LevelInfo の Number（1〜）が一覧の何番目か（シーンの名前からは探さない。エディタの Play 中に LoadScene しても `GetOpenSceneName` は開いたシーンのままなので）。
- シーンがまだ無いステージは、ステージ選択で「じゅんびちゅう」（`SceneManager::HasScene`）。

### 2.3 進み具合 `GameProgress`（段階2。保存する）
```cpp
class GameProgress {
public:
	static bool IsCleared(size_t index);
	static bool IsUnlocked(size_t index);           // 0 番はいつも。ほかは1つ前をクリアしたら
	static std::optional<float> GetBestTime(size_t index); // 1位のタイムだけ（秒。無ければ空）
	// クリアした。タイムが1位なら覚える。すぐファイルに書く。前の1位（無ければ空）を返す（新記録の演出に使う）
	static std::optional<float> RecordClear(size_t index, float time);
	static void UnlockAll(); // デバッグ（エディタだけ）
	static void ResetAll();  // デバッグ（エディタだけ）
};
```
- ファイル：`saves/Progress.json`
  ```json
  {"version": 1, "stages": {"Level01": {"cleared": true, "bestTime": 32.45}}}
  ```
  - **シーンの名前で書く**：一覧の順番を入れ替える・ステージを足す・消すしても、記録がずれない（Q2）。一覧から消したステージの記録は、読むときに無視する（ファイルには残る）。
  - 読めない・壊れている → 何もクリアしていない扱い（落ちない。ログに警告）。
- **残すのは1位のタイムだけ**（ランキングは作らない）。
- タイトルで **F1 ＝全部開く・F2 ＝全部消す**（`#ifdef USE_IMGUI` の中だけ）。

### 2.4 遊んだ時間（段階2）
- `LevelInfo` が数える：`Time::GetDeltaTime()`（ポーズ中は 0）を足していく。ゴールした瞬間に止める。やりなおす（シーンを読み直す）と 0 から。
- 出し方は1か所の関数：`FormatTime(32.456f)` → `"0:32.45"`（分:秒.1/100 秒。切り捨て）。

### 2.5 画面をまたぐ値 `OutGameState`（段階2。保存しない）
```cpp
struct OutGameState {
	size_t lastStage = 0; // 最後に遊んだ・次に遊ぶステージ（ステージ選択で、最初にここを選ぶ）
};
inline OutGameState gOutGame;
```

---

## 3. メニューの共通部分 `MenuBase`（段階1）

`PauseMenu` の「項目を集める・選ぶ・見本の飾りを写す・決める」を親クラスに取り出す。ポーズ・タイトル・ステージ選択・クリアの4つが派生。

| 決まり | 中身 |
|---|---|
| 項目 | 子の `Items` の、**有効な**子（Hierarchy の上から順）。`SetActive(false)` にした項目は選べない |
| 選んだ見た目 | 見本の Text（`SelectedStyle`）の飾りを写す。外れた項目は元の飾りに戻す |
| キー | ↑↓（W S・十字・スティック）。Columns が 2 以上なら ←→ も。決める（Enter・Space・A）・戻る（Esc・Start・B） |
| マウス | **Text の文字のかたまり＋Hit Padding** に乗ったら選ぶ（動かしたときだけ）・左クリックで決める・右クリックで戻る。項目に Image があれば、その形で当たる |
| 並び | Columns 1：縦・端から反対の端へ回る／2 以上：格子・端で止まる |
| 入力を受けない時 | 最初の1フレーム（開いた Enter でそのまま決めない）・決めた後・幕の間・派生クラスが止めている間 |
| 派生クラスで書く物 | `OnStart`・`OnUpdate`・`Decide`・`OnCancel`・`CanDecide`・`OnDecideRejected`・`OnSelect`・`IsInputBlocked`（`Start`・`Update` は MenuBase が持つので書かない） |

---

## 4. タイトル（段階1）

### 4.1 見た目
```
┌──────────────────────────────────────┐
│  （Level01 の背景・足場を、被写界深度で全部ぼかして止めたまま）  │
│                                      │
│              森 の 中 の 氷                 │  ← 細い白・うっすら Glow（ユーザーの Title）
│                                      │
│                 Start                      │  ← 選んだ項目は明るく・うっすら光る
│                 Quit                       │
└──────────────────────────────────────┘
```
- 背景は Level01 の物をそのまま（Save As で写して、キューブ・LevelInfo・説明を消す）。Camera Follow は外す。
- **ぼかし**：Depth Of Field を Distance・Focus Distance 2・Focus Range 0・Far Transition 4・Near Transition 0（全部の奥がぼける。作業用コピーで見た目を確かめた）。
- 円のパーティクル・霧は動いたまま（Q7：背景は動かさないが、粒は勝手に動く）。

### 4.2 Entity
| Entity | Component | メモ |
|---|---|---|
| TitleUI（空） | `TitleMenu` | |
| ├ Logo | Text | 文字は TitleMenu が `kGameTitle` で書く |
| ├ Items → Start・Quit | Text「Start」「Quit」 | 名前は英語（Decide が名前で分ける） |
| └ SelectedStyle | Text（Enabled を外す） | 選んだ項目の見本 |

### 4.3 TitleMenu
- Start → `UIScreen::Open("StageSelect")`（段階2 で画面ができるまでは、Console に警告が出るだけ）。
- Quit → 製品版：`PostMessage(GetMainHWND(), WM_CLOSE)`／エディタ：ログだけ。
- ステージ選択が出ている間は入力を受けない（`IsInputBlocked`）。

---

## 5. ステージ選択（段階2）

### 5.1 見た目（Text だけ）
```
┌──────────────────────────────────────┐
│ （タイトルの背景。ぼけたまま。タイトルの文字は静かに消える）     │
│                 Select Stage                     │
│                                      │
│          01   02   03   04   05                  │  ← 01 は選んでいる（明るく・うっすら光る）
│          06   07   08   09   10                  │     クリア＝白・遊べる＝少し薄い・まだ＝ほとんど見えない
│                                      │
│                  Stage 01                        │  ← 選んでいるステージの名前
│                 Best  0:32.45                    │  ← 1位のタイムだけ（無ければ --:--.--）
└──────────────────────────────────────┘
```
（作業用コピーで、この形の画面を出して確かめた）
- マスは2桁の数字の Text だけ。違いは明るさだけ（Cleared / Open / Locked Color）。
- 遊べないマスで決めると、タイムの行がしばらく「Clear the previous stage」になる（震わせない）。シーンがまだ無いマスは「Coming soon」。
- ステージ選択が出ている間、タイトルの文字（ロゴ・Start・Quit）は Alpha Group で 0.5 秒かけて消え、閉じると戻る。

### 5.2 マスは一覧の数だけコードで作る（Q2：増える・減るので）
- StageSelect.uiscreen には**見本のマスを1つ**（`CellTemplate`：Text。Enabled を外して置く。この位置が1つ目のマス）。
- `StageSelectMenu::OnStart` で、`kStages` の数だけ `Items` の子に Text を作る（見本の TextComponent をそのまま写して、文字・色・位置だけ変える）。
- 並べ方は Inspector：Columns（MenuBase の値。5）・Cell Spacing（横・縦のピクセル）。**一覧に1行足すだけで、マスも1つ増える**。

### 5.3 StageSelectMenu
| いつ | すること |
|---|---|
| `OnStart` | マスを作る。最初に選ぶのは `gOutGame.lastStage`（遊べなければ、遊べる中で一番後ろ） |
| `OnSelect(i)` | 下の2行に名前と1位のタイム |
| `CanDecide(i)` | 遊べる、かつシーンがある |
| `OnDecideRejected(i)` | 遊べないマスなら、タイムの行を 1.5 秒「Clear the previous stage」に |
| `Decide(i)` | `gOutGame.lastStage = i` → `SceneTransition::LoadScene(kStages[i].scene)` |
| `OnCancel` | `UIScreen::Close("StageSelect")` |

---

## 6. クリア画面と演出（段階3。静かに余韻を残す）

### 6.1 演出の流れ（時間は止めない時間で数える。数字は Inspector で直せる）

「やった！」ではなく、**森が静かになって、記録がふっと浮かぶ**感じ。

| 時間 | 起きること | 作り |
|---|---|---|
| 0.0 | 扉に触れる。ゴールのパーティクルが飛ぶ・タイムが止まる・キューブが扉へ吸い込まれる（今のまま） | 今の GoalDoor・Particle System。LevelInfo::Clear（段階2 で入れた） |
| 0.0〜1.4 | **ゲームの絵がゆっくりぼけていく**（InOutSine）。音があれば BGM も少し下げる | Depth Of Field の値を「全部ぼけ」へ動かす |
| 1.0 | Clear 画面が開く。**「Stage 01」**（小さく・字間を広く）がフェードしながら 12px 上がってくる | UI Screen の出し方（Slide＋Fade、Stagger） |
| 1.3 | **タイム「0:32.45」**（大きく・細く）がフェードで出る。数え上げはしない | 同上（Stagger で少し遅れる） |
| 1.7 | 1位なら **「New Best」**（小さく・氷色のうっすらした Glow）。1位でなければ「Best  0:30.12」（薄く） | ClearMenu が文字と色を書く |
| 2.1 | **「Next」「Title」**がゆっくり出る。ここから入力を受ける | UI Screen Element の Delay |
| いつでも | 決めるキー・クリックで、演出を最後まで飛ばす（待たせない） | ClearMenu |

- 使わない物：ヒットストップ・「！」・大きくなる／縮む・跳ねる・数え上げ・画面を暗くする Image（見た目の決まり）。
- 最後のステージ（次が無い）なら「Next」を出さない。

### 6.2 Entity（Clear.uiscreen）
| Entity | Component | メモ |
|---|---|---|
| Clear（一番上） | UI Screen（Pause Game を外す・出し方 Slide＋Fade・Stagger 0.3）・`ClearMenu` | |
| ├ StageText | Text「Stage 01」 | ClearMenu が一覧の名前を書く |
| ├ TimeText | Text「0:00.00」 | ClearMenu が LevelInfo のタイムを書く |
| ├ RecordText | Text「New Best」／「Best  0:30.12」 | ClearMenu が書く |
| ├ Items → Next・Title | Text「Next」「Title」 | UI Screen Element（Delay 1.1） |
| └ SelectedStyle | Text（Enabled を外す） | |

### 6.3 変える所
- `GoalDoor`：触れた瞬間にぼかし始める。Delay（1.0 秒）の後に `UIScreen::Open("Clear")`（今の LoadNextLevel の代わり）。
- `ClearMenu`：文字を書く・Next（`LevelInfo::LoadNextLevel`）・Title（`LoadScene("Title")`）・演出を飛ばす。最後のステージなら Next を `SetActive(false)`。
- `LevelInfo` は段階2 で済み（時間・記録・クリア後は Esc と R を見ない・一覧の次へ）。

---

## 7. 段階

| 段階 | 中身 | 写したら確かめること | md |
|---|---|---|---|
| **1** | GameTitle・MenuBase・PauseMenu（派生に）・TitleMenu・main.cpp・Title シーン | ポーズが今までどおり（マウスでも）・タイトルへで Title・Quit で終わる（製品版） | [OutGame1.md](OutGame1.md) |
| **2** | StageCatalog・GameProgress・OutGameState・FormatTime・LevelInfo（一覧の次・時間・クリアの記録）・GoalDoor（1行）・MenuBase（小さい直し）・TitleMenu（文字を消す・F1/F2）・StageSelect.uiscreen・StageSelectMenu | 遊べる所だけ入れる・1位のタイム・F1 / F2・ゴールで記録が増える | [OutGame2.md](OutGame2.md) |
| **3** | GoalDoor・Clear.uiscreen・ClearMenu・静かな演出 | ゴール → 演出 → Next／Title・New Best・演出を飛ばせる | 段階2 の後 |
| 4（仕上げ） | 効果音（選ぶ・決める・入れない・クリア。小さく柔らかい音）・ロゴがゆっくり浮かぶ・ステージの名前 | | 好みで |

---

## 8. 気をつける所

| 場面 | 起きること | 決め |
|---|---|---|
| 決めた後、幕が閉じきるまで | 連打で2回 LoadScene | `isDecided_`＋幕の間は入力なし（MenuBase） |
| 画面を開いた Enter | 同じ Enter で1つ目を決める | 最初の1フレームは入力なし（MenuBase） |
| ゴールの瞬間に Esc | クリア中にポーズが開く | LevelInfo がクリア後は Esc・R を見ない（段階3） |
| ポーズ中に「タイトルへ」 | 止まったまま幕 | 幕は止めない時間で動くので、そのまま（Restart と同じ） |
| エディタで Level05 から Play | タイトルを通らない | 一覧は LevelInfo の Number で引くので動く |
| エディタの起動 | main.cpp の "Title" が効かない | エディタは前回開いていたシーンから始まる（エンジンの決まり）。"Title" が効くのは製品版 |
| 保存ファイルが壊れた | 読めない | 何もクリアしていない扱い・ログに警告 |
| シーンがまだ無いステージ | LoadScene が失敗 | ステージ選択で「じゅんびちゅう」・クリア画面の Next を出さない |
| 演出の途中で決める | 待たされる | 決めるキーで演出を最後へ飛ばす（項目は次の決めるキーで選ぶ） |

---

## 9. 決めたこと（2026-10-26）

| | 決めたこと |
|---|---|
| Q1 | タイトルへ戻ったら、ステージ選択は**開かない** |
| Q2 | ステージは 10 個の予定だが増減する → 一覧は数を書かない・マスはコードで作る・記録はシーンの名前で |
| Q3 | ゲームの名前は日本語で、あとで変えられるように → `GameTitle.h` の1か所。「森の中の氷」に決めた（候補の表は 2.1） |
| Q4 | クリアのタイムを出す・気持ちいい演出（6.1） |
| Q5 | 「もう一度」は入れない |
| Q6 | マウスでも選べる（Text に当たる。Image は置かない） |
| Q7 | タイトルの背景は動かさない・ぼかす・ステージの背景を見せる |
| — | ステージ選択へはシーンを替えない（UI Screen を重ねる） |
| — | ステージ選択で見られるのは1位のタイムだけ |
| — | 数字の Image・Text の背景の Image は作らない |
| — | 一気に作らず、段階に分ける（7章） |
| — | 世界観はおしゃれで静か：「！」なし・英語の短い言葉・フェードとわずかなスライドだけ（0章の見た目の決まり）。クリア画面の演出もそれに合わせた（6章） |
| — | ゲームの名前は「森の中の氷」（ユーザーが GameTitle.h に入れた） |
