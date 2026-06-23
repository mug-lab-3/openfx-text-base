# Interaction Intent システム 設計ドキュメント

## 1. 概要

Intent（意図）システムは、複数の UseCase が互いの状態を直接参照せずに連携するための **疎結合な状態共有メカニズム** です。

あるUseCaseが「今こういう状態である」という意図をemit（発信）し、別のUseCaseがそれをconsume（受信）して振る舞いを変えます。UseCaseどうしが直接依存しないため、組み合わせを自由に変えられます。

### なぜ必要か

このサンプルプロジェクトのインタラクションでは、複数のUseCaseが同時に起動します。例えば：

- マウスがテキスト付近に来たとき → グラブエリアを表示したい
- ドラッグ中 → 十字カーソルを表示したい
- ドラッグ中かどうかは `DragUseCase` だけが知っている

もし `DragFeedbackUseCase` が `DragUseCase` のポインタを直接参照すると、両者が密結合になります。Intentシステムは、状態を「キー付きの意図」として仲介役（`UseCaseRouter`）に預けることで、この問題を解決します。

---

## 2. 設計の全体像

Intentの集約・配布は **`UseCaseRouter` が自動的に管理** します。各UseCaseは以下の3点だけを担えばよく、他UseCaseの存在やRouterの内部構造を知る必要はありません。

| UseCaseの責務 | 方法 |
|---|---|
| どのキーをemit/consumeするか**宣言**する | `declaredEmittedIntents` / `declaredConsumedIntents` をoverrideする |
| 状態が変わったときにintentを**送信**する | `UseCaseResult::intents_` にデータを入れて返す |
| 他UseCaseのintentを**受信**する | `SnapshotState::intents_` / `CurrentState::intents_` を参照する |

Routerは各UseCaseが返した `UseCaseResult::intents_` を回収して内部の `mergedIntents_`（`IntentRegistry`）に集約し、次のイベント時に各UseCaseへ自動的に配布します。配布時には、そのUseCaseが宣言したキーだけがフィルタリングされて渡されます。

```
[UseCase A] --emit--> UseCaseRouter (mergedIntents_) --consume--> [UseCase B]
                          ↑ 集約と配布を自動で仲介
```

---

## 3. データ構造

### Intentの値

```
IntentValue  = variant<BLBox, BLPoint, double, bool, SelectionItem>
IntentValues = vector<IntentValue>
IntentMap    = unordered_map<string key, optional<IntentValues>>
```

1つのキーに対して複数の値（`IntentValues`）を渡せるため、用途に応じて様々な情報を運べます。

| 型 | 用途例 |
|---|---|
| `bool` | フラグ（このプロジェクトでの使用例: `isDragging = true`、`isInGrabArea = true`） |
| `double` | 数値（進捗・スケール・距離など） |
| `BLPoint` | キャンバス上の座標（ホバー位置など） |
| `BLBox` | キャンバス上の矩形領域（選択範囲・ヒット領域など） |
| `SelectionItem` | 選択中の文字・パートのインデックス（`isInMarqueeSelection` での使用例あり） |

> **Note:** このプロジェクトでは `bool`（`true`）でフラグを立てる使い方しかしていませんが、上記のとおり座標・矩形・選択項目など豊富な情報を運べる設計になっています。また `IntentValues` はベクタなので、同じキーに複数の値を並べることもできます（例：範囲選択で複数の `SelectionItem` を一度にemit）。

### IntentRegistry

intentを管理するストア。`UseCaseRouter` が1つ `mergedIntents_` として保持します。

```
registry_[key][useCaseName] = IntentValues
```

- **キー**：intentの名前（例: `"isDragging"`, `"isInGrabArea"`）
- **所有者**：emitしたUseCaseの名前
- 同じキーに複数のUseCaseがemitできる（例: 複数オブジェクトのホバー状態）

主要メソッド：

| メソッド | 説明 |
|---|---|
| `emit(useCaseName, key, values)` | intentを登録・更新する |
| `clear(useCaseName)` | そのUseCaseのintentをすべて削除する |
| `clear(useCaseName, key)` | 特定キーのintentを削除する |
| `has(key)` | 指定キーにアクティブなintentがあるか |
| `getGroups(key)` | キーに紐づくIntentValuesのリストを返す（所有者ごとにグループ化） |
| `getFlattened(key)` | キーに紐づくすべての値をフラットに返す |

---

## 4. UseCase側の実装

セクション2で挙げた「宣言・送信・受信」の3つの責務を、具体的なコードで示します。

### 宣言（declaredEmittedIntents / declaredConsumedIntents）

各UseCaseは自分が発信・受信するintentを**事前に宣言**します。

```cpp
// DragUseCase: isDragging を emit し、isInGrabArea を consume する
auto declaredEmittedIntents()  const -> vector<string_view> { return {"isDragging"}; }
auto declaredConsumedIntents() const -> vector<string_view> { return {"isInGrabArea"}; }
```

宣言されていないキーをemitしようとすると、`UseCaseRouter::applyIntents` がエラーログを出して無視します。これにより意図しない副作用を防ぎます。

### 送信（Emit）

UseCaseのハンドラ（`penDown`, `penMotion`, `penUp`, `onDraw`）が返す `UseCaseResult::intents_` にデータを入れます。

```cpp
// DragUseCase::penDown より
result.intents_["isDragging"] = IntentValues{true};

// DragUseCase::penUp より（クリアする場合は nullopt）
result.intents_["isDragging"] = std::nullopt;
```

`UseCaseRouter::dispatchEvents` がこの戻り値を受け取り `applyIntents` を呼び出すことで `mergedIntents_` に反映されます。

### 受信（Consume）

RouterはUseCaseに渡す `SnapshotState` / `CurrentState` に、そのUseCaseが宣言したキーのみをフィルタリングして注入します（`snapshotWithIntents` / `currentWithIntents`）。

```cpp
// DragUseCase::canHandle より（SnapshotState.intents_ を参照）
return current.intents_.has("isInGrabArea") ? HandlingRole::Primary : HandlingRole::None;

// DragFeedbackUseCase::canHandle より（同様）
return current.intents_.has("isDragging") ? HandlingRole::Passive : HandlingRole::None;
```

UseCaseは `mergedIntents_` 全体にはアクセスできず、**自分が宣言したキーのみ** が見えます。

---

## 5. このサンプルプロジェクトでの使用例

### 登場するintent一覧

| キー | Emitter | Consumer |
|---|---|---|
| `isInGrabArea` | `GrabableAreaDisplayUseCase` | `DragUseCase` |
| `isDragging` | `DragUseCase` | `DragFeedbackUseCase` |

### フロー図

```
マウス移動（PenMotion / onDraw）
  │
  ▼
GrabableAreaDisplayUseCase::onDraw
  ├─ テキスト位置からX・Y各±0.15（正規化座標）の矩形範囲内ならオレンジの枠を描画
  └─ isInGrabArea = true を emit
        │
        ▼
  mergedIntents_["isInGrabArea"]["GrabableAreaDisplayUseCase"] = [true]
        │
        ▼
DragUseCase::canHandle
  └─ intents_.has("isInGrabArea") == true → HandlingRole::Primary を返す
        │
        ▼
PenDown 発生 → DragUseCase が起動
  ├─ penDown: isDragging = true を emit
  ├─ penMotion: 座標を計算してパラメータを更新 / isDragging = true を emit
  └─ penUp: isDragging = nullopt（クリア）を emit
        │
        ▼
  mergedIntents_["isDragging"]["DragUseCase"] = [true]
        │
        ▼
DragFeedbackUseCase::canHandle
  └─ intents_.has("isDragging") == true → HandlingRole::Passive を返す
        │
        ▼
DragFeedbackUseCase::onDraw
  └─ 白い十字カーソルを描画
```

### テキストがない場所では

`GrabableAreaDisplayUseCase` が `isInGrabArea` をemitしないため、`DragUseCase::canHandle` は `HandlingRole::None` を返し、ドラッグが起動しません。

---

## 6. Router内部のライフサイクル

UseCaseからは見えない、`UseCaseRouter` が裏側で行っている処理の流れです。

1. **activateUseCases** 呼び出し時、`snapshotWithIntents` で各候補に現在のintentsを注入してから `canHandle` を呼ぶ
2. UseCase がイベントハンドラから `UseCaseResult` を返す
3. `dispatchEvents` 内の `applyIntents` が `UseCaseResult::intents_` を `mergedIntents_` に反映する
4. UseCase がTerminateすると `clearIntents(useCaseName)` でそのUseCaseのすべてのintentsが削除される
5. `penMotion` などの各ハンドラの戻り値はいずれも `applyIntents` を通る。マウス移動のたびに毎回emitし、外れたら `nullopt` でclearすることで「マウスがエリア内にいる間だけ有効」という状態を自然に表現できる

---

## 7. まとめ

| 概念 | 役割 |
|---|---|
| `IntentRegistry` | key × useCaseName の2層マップでintentsを管理するストア |
| `declaredEmittedIntents` | このUseCaseがemitするキーの宣言（未宣言キーの送信を防止） |
| `declaredConsumedIntents` | このUseCaseが参照するキーの宣言（他UseCaseのintentsを遮断） |
| `UseCaseResult::intents_` | イベントハンドラからの返却値としてemit/clearを指示する |
| `snapshotWithIntents` / `currentWithIntents` | declaredConsumedIntentsのみをフィルタして各UseCaseに渡す |
