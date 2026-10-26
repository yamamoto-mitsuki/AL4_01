#pragma once
#include <optional>
#include <string>
#include <vector>

#include <MyEngine/Components/TextComponent.h>
#include <MyEngine/ECS/Script.h>

/// <summary>
/// メニュー（項目を選んで決める画面）の共通部分。これだけでは付けられない（SCRIPT で登録しない。JumpRing と同じ）
/// <para>項目は、子の Items の子（有効な物だけ。Hierarchy の上から順）。何をするかは派生クラスの Decide が、項目の名前で決める</para>
/// <para>選んだ項目には、見本の Text（一番上の直下の子の SelectedStyle）の飾り（色・縁取り・影・光・Shine・Motion）を写す。
/// 選んでいない項目は、元の飾りに戻す</para>
/// <para>キー：↑↓（Columns が 2 以上なら ←→ も）で選ぶ / 決める / 戻る。マウス：項目に乗ったら選ぶ・左クリックで決める・右クリックで戻る</para>
/// <para>開いている UI Screen の中に置けば、ポーズ中（ゲームが止まっている間）も呼ばれる</para>
/// </summary>
class MenuBase : public Script {
public:
	void Start() final;  // 入力を受ける準備（派生では書かない。最初に何かするなら OnStart）
	void Update() final; // 入力 → 選ぶ・決める・戻る（派生では書かない。何をするかは下の virtual で）
	void Fields(ComponentUI& ui) override;

protected:
	// ===== 派生クラスで書く =====
	// 最初の Update の前に1回（Start の代わり。最初に選ぶ項目を Select で決めるなど）
	virtual void OnStart() {}
	// 毎フレーム、入力を見る前に（演出を進めるなど。Update の代わり）
	virtual void OnUpdate() {}
	// 決めた。index は項目の番号（上から 0, 1, …）、itemName は項目の Entity の名前
	virtual void Decide(int index, const std::string& itemName) = 0;
	// 戻る（Esc・パッドの B・右クリック）。何もしない画面は書かない
	virtual void OnCancel() {}
	// index の項目を決めてよいか（開いていないステージなど）
	virtual bool CanDecide(int /*index*/) const { return true; }
	// 決められない項目で決めた（鍵を揺らすなど）
	virtual void OnDecideRejected(int /*index*/) {}
	// 選んでいる項目が変わった（最初の1回も呼ぶ）。説明を出す・カーソルを動かすなど
	virtual void OnSelect(int /*index*/) {}
	// 入力を受けない（上に別の画面が開いているなど）。幕の間は、これとは別に受けない
	virtual bool IsInputBlocked() const { return false; }

	// ===== 派生クラスから使う =====
	// 項目の並び（子の Items の、有効な子。Hierarchy の上から順）
	// Items の外の物も項目にしたい画面（ステージ選択の Back）は、派生クラスで書き換えて後ろに足す
	virtual std::vector<Handle<Entity>> CollectItems() const;
	// 選ぶ項目を決める（OnStart で最初の項目を変えたいとき。OnSelect は次の Update で呼ぶ）
	void Select(int index);
	int GetSelected() const { return selected_; }
	// 横に並べる数（1：縦の並び。2 以上：格子）
	void SetColumns(int columns) { columns_ = columns; }
	int GetColumns() const { return columns_; }

	bool isDecided_ = false; // もう決めた（消える動き・幕が閉じる間に、もう一度決めないように）

private:
	/// <summary>
	/// Text の飾りの部分（文字・フォント・大きさ・位置の合わせ方は入れない。選んだ・外したで入れ替える物だけ）
	/// </summary>
	struct TextStyle {
		Vector4 color;
		float outlineWidth = 0.0f;
		Vector4 outlineColor;
		bool shadow = false;
		Vector2 shadowOffset;
		Vector4 shadowColor;
		float glowWidth = 0.0f;
		Vector4 glowColor;
		bool shine = false;
		Vector4 shineColor;
		float shineWidth = 0.0f;
		float shineDuration = 0.0f;
		float shineInterval = 0.0f;
		TextMotion motion = TextMotion::None;
		float motionAmount = 0.0f;
		float motionSpeed = 0.0f;
		float motionSpread = 0.0f;

		// Text から飾りを取り出す・Text に飾りを書く
		static TextStyle From(const TextComponent& text);
		void ApplyTo(TextComponent& text) const;
	};

	// キー・パッド・スティックで、選ぶ項目を動かす（動かしたら true）
	bool MoveByInput(int count, const Vector2& stick);
	// マウスが乗っている項目（無ければ -1）
	int FindItemUnderMouse(const std::vector<Handle<Entity>>& items) const;
	// 項目の上にマウスがあるか（Image があれば、その形。無ければ Text の文字のかたまり＋Hit Padding）
	bool IsMouseOverItem(Handle<Entity> item) const;
	// 選んでいる項目を決める（決められなければ OnDecideRejected）
	void TryDecide(const std::vector<Handle<Entity>>& items, int index);
	// 選んでいる項目に見本の飾りを、ほかの項目に元の飾りを書く
	void ApplyLook(const std::vector<Handle<Entity>>& items);
	// 選んでいる項目を Selected Offset だけ、ゆっくりずらす（外れた項目は元の位置へ戻る）。Selected Offset が 0 なら何もしない
	void ApplySlide(const std::vector<Handle<Entity>>& items);

	// ===== Inspector に出す・保存する値 =====
	std::string styleName_ = "SelectedStyle"; // 選んだ項目の見本の Text の名前（一番上の直下の子。Text の Enabled は外して置く）
	int columns_ = 1;                         // 横に並べる数（1：縦の並び・端から反対の端へ回る。2 以上：格子・端で止まる）
	bool useMouse_ = true;                    // マウスでも選ぶ・決める
	float hitPadding_ = 12.0f;                // マウスの当たりを、文字のかたまりより広げる（ピクセル。Image の項目には効かない）
	Vector2 selectedOffset_ = {0.0f, 0.0f};   // 選んだ項目をずらす量（ピクセル。右・上が＋。0 ならずらさない）
	float slideTime_ = 0.12f;                 // ずれる・戻るのにかかる秒数の目安（小さいほど、すばやく動く）

	// ===== 実行中だけの値 =====
	int selected_ = 0;                                     // 選んでいる項目（上から 0, 1, …）
	int notifiedSelected_ = -1;                            // 最後に OnSelect で知らせた項目（変わったときだけ知らせる）
	bool isReady_ = false;                                 // 入力を受けてよい（最初の1フレームは受けない。この画面を開いた Enter で、そのまま決めないように）
	Vector2 previousStick_ = {0.0f, 0.0f};                 // 前のフレームのスティック（倒した瞬間だけ動かす）
	std::vector<std::optional<TextStyle>> originalStyles_; // 項目の元の飾り（Text が付いている項目の分だけ覚える。外れた項目をこれに戻す）
	std::vector<std::optional<Vector3>> basePositions_;    // 項目の元の位置（ずらす前。最初のフレームに覚える）
	std::vector<float> slideAmounts_;                      // 項目のずれ具合（0：元の位置 〜 1：Selected Offset だけずれた）
};