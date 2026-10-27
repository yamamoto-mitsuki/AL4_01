# ステージ12「強風の谷」と、風の強さを見せる（写経用）

- シーン `Resources/scenes/Level12.scene.json` はもう置いてある（StageCatalog.h の `Level12` の行は、もう入っている）。
- コードは **WindParticles に「Show Gust」を足す**だけ。風の今の強さで、風の筋の **長さ・明るさ・出る数** を変える。
  - 弱まると筋が短く・暗く・少なくなって、ほとんど消える。強まると長く・明るく・多くなる。
  - 写すまでは Show Gust の値は読まれない（ファイルに入っていても無視される）ので、風の筋はいつも同じ見た目のまま。

## パート A：`Source/Scripts/Effects/WindParticles.h`（3か所）

① クラスの説明に1行（`/// <para>風のゆれ（Gust）も…</para>` の下）
```cpp
/// <para>Show Gust：今の風の強さ（Wind Zone の Speed の何倍か）で、筋の長さ（Stretch）・明るさ（Intensity）・出る数を変える。明るさは強さの2乗。弱まると短く暗く少なく、強まると長く明るく多くなる</para>
```

② `public:` に Start
前：
```cpp
public:
	void Update() override;
```
後：
```cpp
public:
	void Start() override;
	void Update() override;
```

③ 値を3つ
前：
```cpp
	Vector2 lifeRate_ = {0.4f, 0.8f}; // 粒の寿命（範囲を風の速さで流れ切る秒数に掛ける。Min〜Max）

	// ===== 実行中だけの値（保存しない）=====
	float carry_ = 0.0f; // 出しきれなかった端数（0〜1 個分。次のフレームへ持ち越す）
```
後：
```cpp
	Vector2 lifeRate_ = {0.4f, 0.8f}; // 粒の寿命（範囲を風の速さで流れ切る秒数に掛ける。Min〜Max）
	bool showGust_ = false;           // 風の強さで、筋の長さ・明るさ・出る数を変える（強い風・弱い風が見て分かるように）

	// ===== 実行中だけの値（保存しない）=====
	float carry_ = 0.0f;         // 出しきれなかった端数（0〜1 個分。次のフレームへ持ち越す）
	float baseStretch_ = 1.0f;   // 最初の Stretch（Show Gust はこれに強さを掛ける）
	float baseIntensity_ = 1.0f; // 最初の Intensity（同じ）
```

## パート B：`Source/Scripts/Effects/WindParticles.cpp`（4か所）

① `SCRIPT(WindParticles, "Effects");` の下に
```cpp
namespace {
constexpr float kMaxGustStrength = 3.0f; // 強さの上限（風が重なって速くなりすぎても、筋が伸びすぎないように）
}
```

② `Fields` の最後（`Life Rate` の下）に1行
```cpp
	ui.Field("Show Gust", showGust_, Tip("今の風の強さ（Wind Zone の Speed の何倍か）で、筋の長さ（Stretch）・明るさ（Intensity）・出る数（Rate Over Time）を変える。弱まると短く暗く少なく、強まると長く明るく多くなる"));
```

③ `Fields` と `Update` の間に Start
```cpp
//=============================================================================
// 最初に1回
//=============================================================================
void WindParticles::Start() {
	// Show Gust で変える前の値を覚える（毎フレーム、ここに今の風の強さを掛けて書く）
	if (const ParticleSystemComponent* particle = GetComponent<ParticleSystemComponent>()) {
		baseStretch_ = particle->stretch;
		baseIntensity_ = particle->intensity;
	}
}


```

④ `Update` の中、2か所
前：
```cpp
	const Vector3 windVelocity = sample.IsBlowing() ? sample.velocity : GetWorldRight(transform) * wind->speed;

	// ===== 出る場所：Wind Zone と同じ箱（Entity の Scale・Rotation は Particle System も同じように掛ける）=====
```
後：
```cpp
	const Vector3 windVelocity = sample.IsBlowing() ? sample.velocity : GetWorldRight(transform) * wind->speed;

	// ===== 風の強さを見た目に出す（Show Gust）=====
	// 強さ＝今の風の速さ ÷ Wind Zone の Speed（ゆれが無ければ 1。Gust 0.8 なら 0.2〜1.8 の間で変わる）
	// Stretch と Intensity は描くときに読まれるので、もう出ている粒も、すぐに伸び縮み・明るさが変わる
	const float gustStrength = std::clamp(Length(windVelocity) / wind->speed, 0.0f, kMaxGustStrength);
	if (showGust_) {
		particle->stretch = (std::max)(baseStretch_ * gustStrength, 1.0f); // 1 より短いと、筋ではなく丸い粒になる
		// 明るさは強さの2乗（弱い風では筋がほとんど消え、強い風でははっきり光る。止んだ・吹いたが一目で分かる）
		particle->intensity = baseIntensity_ * gustStrength * gustStrength;
	}

	// ===== 出る場所：Wind Zone と同じ箱（Entity の Scale・Rotation は Particle System も同じように掛ける）=====
```

前：
```cpp
	carry_ += (std::max)(0.0f, particle->rateOverTime) * deltaTime;
```
後：
```cpp
	const float rateScale = showGust_ ? gustStrength : 1.0f; // Show Gust なら、強い風ほど多く出す
	carry_ += (std::max)(0.0f, particle->rateOverTime) * rateScale * deltaTime;
```

### 解説
- **強さ**：`Sample` の速さにはゆれ（Gust）が入っているので、それを Wind Zone の Speed で割ると「いつもの何倍か」になる。`std::clamp`（`<algorithm>`）で 0〜3 に収める。
- **すぐ変わる**：Stretch と Intensity は Particle System が粒を描くときに読む値なので、今流れている粒もその場で伸び縮み・明るさが変わる（色の A は出た時に決まるので、すぐには変わらない。だから明るさの方を使う）。
- **2乗**：強さ 0.2 なら明るさ 0.04（ほぼ見えない）、1.8 なら 3.2。1乗だと弱い風でも筋が残って「止んだ」が分かりにくかった。
- **最初の値を覚える**：毎フレーム Stretch と Intensity を書き換えるので、Inspector で決めた元の値を Start で覚えておき、それに掛ける（元の値に掛け続けると、どんどん小さく・大きくなってしまう）。
- 前からある風（Level09・10・11）は Show Gust が外れているので、見た目は変わらない。

## ステージ12 の中身（Level12.scene.json）
風は左向き・Speed 4.5・Gust 0.9・Gust Frequency 0.08（1回りが約12秒）。強い時は 8.6m/s（走る 7m/s より強いので押し戻される。全体の約15%の時間）、弱い時は 0.5m/s くらい。歩いて進める時間は平均5秒（短い時で約1.7秒）。
（最初は Speed 7・Gust 0.85・0.15 だったが、弱い時間が平均1.2秒しかなく、最初の練習から通れなかったので変えた）
1. **練習**：平らな地面の上の強風。押し戻されても、風の無い所（岩の陰）に戻るだけ。筋が消えた時に進む。
2. **吹っ飛びは風が効かない**：緑の円で強風の谷を飛び越える。飛んだ先の着地の所はまだ風の中なので、強い時に着くと押し戻されて落ちる。筋が消えた時に飛ぶ。
3. **下向きの風**：ジャンプが 1.2m くらいしか上がらない。水色の円 2つで谷を渡る（2つ目は円の右半分で跳ぶ）。
4. **強風の中の 2.6m の穴を3つ**：風が弱い時でないと届かない。着いた岩の左半分は風の中、右の端は風が無い。

## 確かめたこと（作業用コピー・2026-10-27）
- 今の AL4_01（StageCatalog の 11〜14、Audio を含む）にパート A・B を入れて、Release のビルドが通った。
- Level12 の岩の陰に置いて 1.2 秒ごとに撮った。強い時は長く明るい筋がたくさん、弱い時はほぼ消える。隣の風（範囲ごとにゆれの時間がずれる）が強い・弱いのも、筋で分かった。
- 下向きの風・穴の続く所も表示を撮った。キーやパッドで通して遊んではいない（届くかは、ジャンプと風の動きの計算で決めた）。
- 粒の値を毎フレーム書き換えるのは前からと同じ作り。エディタで再生を止めた後に Stretch・Intensity が変わったままなら、シーンを開き直す（保存しない）。
