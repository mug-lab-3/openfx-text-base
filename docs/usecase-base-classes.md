# UseCase ベースクラス／仕組み 設計ドキュメント

> このドキュメントは2部構成です。
> **前半（§1〜§3）= 設計思想** … なぜこの形なのかを理解するための読み物。
> **後半（§4〜§10）= リファレンス** … 各ベースクラスの具体的なメンバ・ライフサイクル・実装手順。

---

## 1. 概要

このプロジェクトのインタラクション層（`src/interaction/`）は、入力イベント（ペン／マウス／キーボード／ボタン）への振る舞いを **UseCase** という単位に分割して実装します。

UseCase には目的の異なる **4 つのベースクラス** があり、すべて `UseCaseRouter`（`src/interaction/UseCaseRouter.h/cc`）が一元的にライフサイクルとイベントルーティングを管理します。

| ベースクラス | 主な役割 | パラメータ変更 | 起動契機 | リファレンス |
|---|---|---|---|---|
| `InteractionUseCase` | ペン／マウス操作の主役。ドラッグ等で**パラメータを書き換える** | できる | `activationTrigger` + `canHandle`（ペン/マウス） | [§4](#4-interactionusecase) |
| `PassiveUseCase` | 視覚フィードバック・内部状態更新。**パラメータには触れない** | させない（型で禁止） | `canHandle`（Passive ロール） | [§5](#5-passiveusecase) |
| `CommandUseCase` | ボタン押下で実行する**一発のアクション** | できる | 対象パラメータ ID の変更 | [§6](#6-commandusecase) |
| `KeyUseCase` | **キーボードショートカット** | できる | `canHandle` + KeyDown/Repeat/Up | [§7](#7-keyusecase) |

いずれも「**いつ動くか**」と「**何をするか**」を UseCase 側が宣言し、それらの調停・呼び出しは `UseCaseRouter` が行う、という分業になっています。この「宣言」と「役割分担」の背後にある考え方を、まず §2・§3 で説明します。

---

## 2. 設計を貫く2つの原則

個々のクラスに入る前に、4つのベースクラスすべてに共通する2つの設計思想を押さえると、以降が理解しやすくなります。

### 2.1 原則A: 挙動は「明示的に宣言」する

UseCase は、ベースクラスの種類を問わず **自身の挙動を明示的に宣言する** よう設計されています。「いつ・どんな条件で起動しうるのか」「何を発信し、何を参照するのか」を、暗黙の実装詳細に埋め込まず、専用のメソッドとして**外から見える形**にします。

各ベースクラスの宣言系メンバは、すべてこの原則の現れです。

| 宣言しているもの | メソッド | 何が明確になるか |
|---|---|---|
| いつ起動候補になるか | `activationTrigger()` | どの入力イベントで起こりうるか |
| どんな条件で処理するか | `canHandle()` | 起動するシチュエーション・ロール |
| どのボタンで動くか | `targetParameterIds()`（Command） | トリガとなる UI |
| 何を発信／参照するか | `declaredEmittedIntents()` / `declaredConsumedIntents()` | 他 UseCase との連携関係 |

**なぜ明示するのか:**

- **解析しやすさ。** ある UseCase が「いつ起動する可能性があるのか」「他とどう連携するのか」を、ハンドラの中身を追わずに宣言だけで把握できます。挙動が実装に潜らず宣言に出ているため、全体像をスキャンで読み取れます。
- **意図しない副作用の抑止。** 宣言にないことはできません。例えば未宣言のキーを emit しようとすると `UseCaseRouter` が弾きます。宣言が、振る舞いの境界そのものになります。
- **AI 補助との相性。** 宣言が明示されていると、AI による解析・コード生成・レビューを受ける際にも、各 UseCase の起動条件や依存関係を正確に渡せます。このテンプレートは「リポジトリ全体を AI に渡して把握してもらう」運用を想定しており（→ [README](../README.md)）、明示的宣言はその前提と噛み合っています。

> 別紙で解説している **Intent システムも同じ原則**です。emit/consume するキーを事前宣言させることで連携関係を明示し、未宣言の送受信を遮断します（→ [interaction-intent.md](interaction-intent.md)）。

### 2.2 原則B: 「関心事」を軸にした3層の役割モデル

UseCase の役割分担は、**「ユーザーの関心事（今やろうとしている操作）」を軸にした3層**で整理されています。

| 層 | 何か | 同時に動ける数 | パラメータ変更 |
|---|---|---|---|
| **プライマリ（Primary）** | ユーザーの関心事そのもの（移動・回転など） | 基本ひとつ | する |
| **セカンダリ（Secondary）** | その操作を複数文字へ横展開したもの | プライマリと同型のみ複数 | する |
| **パッシブ（Passive）** | 関心事ではない、UX を支える脇役（描画など） | いくつでも | しない（型で禁止） |

> この3層は `InteractionUseCase` 系（ペン／マウス）の役割分担です。`CommandUseCase`（ボタン）と `KeyUseCase`（キー）は別系統で、この調停には参加しません。

#### プライマリ: 関心事は常にひとつ

根底にあるのは **「ある瞬間にプライマリの UseCase は基本ひとつ」** という考え方です。これは技術的制約ではなく、**「ユーザーの関心事は常にひとつ」** というインタラクション設計上のモデルを反映しています。ユーザーは「今まさに何をしているか（移動なのか、回転なのか、転がして移動なのか）」をひとつだけ持っている、という前提です。`HandlingRole` の Primary 勝者選定（→ [§8](#8-handlingrole-による調停)）がこの「ひとつだけ」を担保します。

ここで言う「ひとつ」は **関心事がひとつ**という意味で、**変更するパラメータがひとつ**という意味ではありません。

> **例:** 「転がして移動する」操作では、関心事は「転がし移動」というひとつですが、それを表現するために **移動（Position）と回転（Rotation）の2パラメータ**を、ひとつの UseCase が `penMotion` 内で同時に更新します。UseCase の粒度は「パラメータ」ではなく「**ユーザーがやろうとしているひとまとまりの操作**」で切ります。

#### セカンダリ: 同じ操作を複数文字へ

このテンプレートが対象とする **テキスト制御** では、**ひとつの操作を複数の文字に同時適用したい**場面があります（複数選択した文字のまとめ移動、リンク文字への波及など）。これを Primary/Secondary で扱います。

- **インスタンスは文字ごとに分ける** — 1 文字 = 1 インスタンス。状態は各自が持つ。
- **ユーザーが直接操作している文字** = **プライマリ**（＝関心事ひとつ）。
- **同時選択・リンクされた他の文字** = **セカンダリ**。プライマリと連動して同じ操作を受ける。

```
ユーザーがドラッグしている文字 'A' ──▶ DragUseCase (Primary)
同時選択中の文字 'B'           ──▶ DragUseCase (Secondary)  ┐ 同じ型の
同時選択中の文字 'C'           ──▶ DragUseCase (Secondary)  ┘ インスタンス
```

重要な制限として、**セカンダリになれるのはプライマリと同じ型の UseCase だけ**です（Router が `Primary 勝者と同型のときだけ`併走を許す。`UseCaseRouter.cc:548` 付近、`typeid(*useCasePointer) == primaryType`）。これは技術都合ではなく **シンプルな UseCase 設計を矯正するための意図的なルール**です。「プライマリで起きることはセカンダリでも同じロジックで起こる」と保証されるため、主役と連動文字を別クラスに書いて齟齬が出ることがなく、ひとつの UseCase を書けばそのまま複数文字へスケールできます。

> **このSDKでの状態:** 文字／パート選択を中心に据えた派生元プロジェクト由来の仕組みで、選択概念を持たない本テンプレートでは実際にはほとんど稼働しません（`targetSelectionItem` が `kAllIndices` を返すため）。`HandlingRole::Secondary` / `SecondaryIfOtherOwned` の調停コードは、テキスト制御プラグインへ発展させる際の土台として残されています。

#### パッシブ: 関心事ではない UX の脇役

`PassiveUseCase` はプライマリの対極です。

- **ユーザーの関心事ではない。** 「今やろうとしている操作」そのものではなく、それを**分かりやすく見せて UX を向上させる補助**です（カーソルのハイライト、グラブ可能エリアの提示、ドラッグ中フィードバックなど）。
- **いくつでも同時に起動できる。** プライマリの「ひとつだけ」制約はなく、`HandlingRole::Passive` は勝者選定に加わらず、プライマリ／セカンダリの裏で何個でも並行して動けます。
- **だからパラメータ変更を許可しない。** 「関心事＝パラメータを変える操作」はプライマリの責務であり、関心事ではない脇役が触れるべきではない、という考え方です。この禁止は**型レベルで強制**されます（→ [§3 理由2](#理由-2-触れてよいものを型で強制する)）。
- **主な仕事はオーバーレイ描画。** `onDraw` での視覚表現が中心です。

---

## 3. なぜベースクラスを分けているのか

ひとつの巨大な「インタラクション処理」を書く代わりに、**入力の種類と責務ごとに型を分けている**のが、この設計の核心です。

### 理由 1: 入力モデルが根本的に違う

- ペン操作は **PenDown → PenMotion(連続) → PenUp** という状態を持つストロークで、その間 UseCase が「アクティブ」であり続ける。
- キーは **KeyDown / Repeat / Up** で、押下ごとに独立に判定される。
- ボタンは **パラメータ値の変化** という単発イベントで、ストロークも継続状態もない。

これらを同じインターフェースに押し込むと、使わないメソッドだらけの肥大したクラスになります。`UseCaseRouter` 側も `currentUseCases_`（アクティブな InteractionUseCase）・`commandUseCases_`・`keyUseCases_` と**別のコンテナで保持**しており、ディスパッチ経路自体が分かれています。

> ただし、この「入力モデルが違うから分ける」が当てはまるのは `CommandUseCase` までです。`KeyUseCase` は後付けの経緯で独立していますが、設計上の必然ではありません（→ [§7](#7-keyusecase)）。

### 理由 2: 「触れてよいもの」を型で強制する

`PassiveUseCase` は **`ParameterManager` を見られないようにするためだけ**に存在します。基底の `InteractionUseCase` のハンドラ（`penDown` 等）を `final` で塞ぎ、`ParameterManager` 引数を取り除いた `onPassivePenDown` 等へ委譲します。

```cpp
// PassiveUseCase.h より
auto penDown(const InteractionInput& input, SnapshotState& snapshot, ParameterManager&,
             const std::vector<std::string_view>& activeUseCases) -> UseCaseResult final {
    return onPassivePenDown(input, snapshot, activeUseCases);  // ParameterManager を渡さない
}
```

これにより「フィードバック描画用のはずの UseCase が誤ってパラメータを書き換える」事故を**コンパイル時に**防げます。可否がコメントではなく型で表現されているのがポイントです（→ [§2.2 パッシブ](#パッシブ-関心事ではない-ux-の脇役)）。

### 理由 3: 調停（どれを動かすか）の一元化

複数の InteractionUseCase が同時に「自分が処理できる」と主張したとき、誰を勝者にするかは `HandlingRole` と `UseCaseRouter::findWinnerPrimary` が決めます（→ [§8](#8-handlingrole-による調停)）。この調停ロジックを Router に集約することで、各 UseCase は自分の都合だけを `canHandle` で表明すればよくなります。

---

## 4. `InteractionUseCase`

`src/interaction/InteractionUseCase.h`。すべての「能動的」インタラクションの基底であり、`PassiveUseCase` の親でもあります。

ペン／マウスのストローク（PenDown → Motion → Up）に応じて **パラメータを書き換える主役**。サンプルでは `DragUseCase`（テキスト位置のドラッグ移動）が代表例です。設計上の位置づけ（プライマリ／セカンダリ）は [§2.2](#22-原則b-関心事を軸にした3層の役割モデル) を参照してください。

### 主要メンバ

| メンバ | 役割 |
|---|---|
| `name()` | UseCase の一意名（ログ・アクティブ一覧・型比較に使用） |
| `activationTrigger()` | 起動契機（`PenDown` / `PenMotion` / `Any`）。詳細は下記 |
| `canHandle(input, current, activationSnapshot, activeUseCases)` | このイベントを処理できるか・どのロールで処理するかを返す（`HandlingRole`） |
| `onStart` / `onFinish` | アクティブ化／終了時のフック |
| `penDown` / `penMotion` / `penUp` | ストローク中の各イベント。戻り値 `UseCaseResult` で**パラメータ更新・選択操作・intent 送信・継続/終了**を `UseCaseRouter` に伝える |
| `onDraw` | オーバーレイ描画（アクティブな間、毎フレーム呼ばれる） |
| `declaredEmittedIntents` / `declaredConsumedIntents` | Intent の宣言（→ [interaction-intent.md](interaction-intent.md)） |
| `targetSelectionItem` | 文字／パート選択の対象（このSDKでは基本不使用。`kAllIndices` がデフォルト） |

### 起動タイミング（`activationTrigger`）

起動判定は、現状 `activationTrigger()` → `canHandle()` の順に評価されます。

| ゲート | 何で絞るか | 性質 |
|---|---|---|
| `activationTrigger()` | **入力イベントの種別**（PenDown / PenMotion / Any） | 状態に依存しない静的フィルタ。まず候補に挙げるかどうか |
| `canHandle()` | **そのときの状況**（intent・カーソル位置・選択など）＋どのロールか | 動的判定。残った候補のロールを決める |

`activationTrigger()` の値：

| 戻り値 | いつ起動候補になるか | 使い分けの目安 |
|---|---|---|
| `PenDown` | PenDown のときだけ | クリックで始まる操作（例: `DragUseCase`） |
| `PenMotion` | PenMotion（押していない移動含む）のとき | マウスが動いている間ずっと判定したい（ホバー） |
| `Any` | どのイベントでも常に | 常にフィードバックを出したい脇役（多くのパッシブ） |

`UseCaseRouter` は各入力イベントに対応する `ActivationTrigger` で `activateUseCases` を呼びます。候補集約（`collectCandidatesFromList`）では、まず `activationTrigger()` で**そのトリガと一致するか `Any` のものだけ**を残し、**そのうえで** `canHandle()` を呼んでロールを判定します（`UseCaseRouter.cc:466`〜`477` 付近）。

```cpp
// 1段目: イベント種別で足切り
if (useCase->activationTrigger() != trigger && useCase->activationTrigger() != ActivationTrigger::Any) {
    continue;  // トリガが一致せず、Any でもなければ候補から外す
}
// ...
// 2段目: 状況に応じてロール判定
HandlingRole role = useCase->canHandle(input, snapshot, nullptr, activeNames);
if (role != HandlingRole::None) { /* 候補に追加 */ }
```

> **設計上の注意（歴史的経緯による負債）:** 上記は現状の実装であって、**本来の思想としては、起動契機の判定は `canHandle()` に一本化すべき**です。起動条件がひとつのメソッドに集約されるほうが「いつ起動するのか」が明確になり、分かりやすさ・品質の面で有利だからです（→ [§2.1 明示的宣言](#21-原則a-挙動は明示的に宣言する)）。2段ゲートに分散しているのは設計判断ではなく、**この `UseCaseRouter` がプロダクトからの切り出しであり、プロダクト側の歴史的経緯で分散したまま持ち込まれている**ためです。
>
> `canHandle()` の中で `input` のイベント種別を見れば `activationTrigger()` の役割は吸収できます。新たに UseCase を書く際は、可能な限り起動条件を `canHandle()` 側に寄せると、本来の思想に沿った明確な設計になります。

> **サンプルでの実態:** サンプルは `activationTrigger` をあまり使い分けておらず、`DragUseCase` だけが `PenDown`、フィードバック系はすべて `Any` です（`PenMotion` の実例なし）。3値の使い分けは、より複雑なインタラクションを実装するときのための拡張ポイントと捉えてください。

### ライフサイクル

`UseCaseRouter` が `factories_`（`std::function<unique_ptr<InteractionUseCase>()>`）から**起動のたびに新しいインスタンスを生成**し、PenUp で `Terminate` するまで `currentUseCases_` に保持します。つまり 1 ストローク = 1 インスタンスで、ストローク中の状態（ドラッグ開始位置など）をメンバに持てます。

戻り値 `UseCaseResult::lifecycle_` が `Terminate` になると、`UseCaseRouter` が `onFinish` を呼び、intent をクリアしてインスタンスを破棄します。

---

## 5. `PassiveUseCase`

`src/interaction/PassiveUseCase.h`。`InteractionUseCase` を継承。

**パラメータを変更しない**、視覚フィードバックや内部状態更新のための UseCase。設計上の位置づけ（関心事ではない UX の脇役・何個でも同時起動）は [§2.2 パッシブ](#パッシブ-関心事ではない-ux-の脇役) を参照。サンプル：

- `CursorHighlightUseCase` — カーソル位置のハイライト
- `DragFeedbackUseCase` — ドラッグ中の十字カーソル表示
- `GrabableAreaDisplayUseCase` — グラブ可能エリアの枠表示

### `InteractionUseCase` との違い

- ハンドラ（`onStart` / `onFinish` / `penDown` / `penMotion` / `penUp`）が **`final`** で、`ParameterManager` を渡さない `onPassive*` 系へ委譲する。サブクラスはこの `onPassive*` を override する。
- `canHandle` では通常 `HandlingRole::Passive` を返す。Passive ロールは Primary 争いに加わらず、**勝者と無関係に並行して動ける**（→ [§8](#8-handlingrole-による調停)）。

> 「触ってはいけないもの（パラメータ）に物理的にアクセスできない」という制約を型で得るのが目的です（→ [§3 理由2](#理由-2-触れてよいものを型で強制する)）。

---

## 6. `CommandUseCase`

`src/interaction/CommandUseCase.h`。**`InteractionUseCase` とは継承関係のない独立した基底**です（ペンイベントを持たないため）。

UI ボタン（PushButton パラメータ）の押下＝**パラメータ値の変化**で起動する、一発のアクション。サンプルでは `ResetPositionAndSizeCommand`（位置・サイズを初期値に戻す）。プライマリ／パッシブの3層モデルには参加しません。

### 主要メンバ

| メンバ | 役割 |
|---|---|
| `name()` | 一意名 |
| `targetParameterIds()` | 自分を起動するボタンの ID 集合（`unordered_set`）。コピー/ペーストのように複数ボタンで1つの UseCase を駆動する場合は複数返す |
| `execute(snapshot, parameterManager, triggeredParameter)` | 実行本体。`triggeredParameter` で「どのボタンが押されたか」を受け取る |

### ライフサイクル

`commandUseCases_` に**最初に一度だけ生成して `UseCaseRouter` の寿命のあいだ保持**します（InteractionUseCase のような毎回再生成ではない）。技術的には保持されるためメンバ変数に状態を持たせることも可能ですが、**`CommandUseCase` はステートレスに設計するのが基本**です。

> **設計上の経緯と方針（プロダクト由来）:** この「常駐」は設計上の必然ではなく、`UseCaseRouter` の切り出し元であるプロダクト側の実装都合を引き継いだものです。`CommandUseCase` は `execute` しか持たないため、**状態を必要とするボタンアクション**（例: コピーした内容を覚えてペーストする）をメンバ変数で保持できるよう常駐にした、という経緯があります。
>
> ただし**状態を持たせるのは本来の設計思想に反します**（各 UseCase の挙動は宣言と入力から決まるべき、という明示的宣言の原則に反し、隠れた状態が解析しにくさを生む → [§2.1](#21-原則a-挙動は明示的に宣言する)）。したがって、新たに書く `CommandUseCase` は**ステートレスで設計するのを基本**とし、常駐はあくまで実装上の事実として扱ってください。

`UseCaseRouter` は起動時に全 Command の `targetParameterIds()` を `commandTriggerIds_` に集約します。`isCommandTrigger(paramId)` で「この ID は何らかの Command の引き金か」を、ID をハードコードせずに判定できます。パラメータ変更を受けた呼び出し側が `onTriggerCommand` を呼ぶと、対応する Command の `execute` が走ります。

---

## 7. `KeyUseCase`

`src/interaction/KeyUseCase.h`。これも **独立した基底**（キーイベント専用）。

キーボードショートカット。サンプルでは `ResetPositionAndSizeKeyUseCase`（Alt+N で Command と同じリセットを実行）。

> **設計上の位置づけ:** `KeyUseCase` は後から追加された機能のため、現状は独立クラスに分割されています。これは `CommandUseCase` のように「入力モデルが根本的に違うから分けざるを得ない」ケースとは異なり、**設計上の必然ではありません**。`UseCaseRouter` のディスパッチ設計を見直し、キーイベントを `InteractionUseCase` のイベントモデル（`canHandle` + 各ハンドラ + `HandlingRole` 調停）に取り込めば、`InteractionUseCase` に統合できます。将来リファクタリングする際の候補です。

### 主要メンバ

| メンバ | 役割 |
|---|---|
| `name()` | 一意名 |
| `canHandle(input, snapshot)` | このキー入力を処理するか（`bool`。InteractionUseCase のような `HandlingRole` ではない） |
| `onKeyDown` / `onKeyRepeat` / `onKeyUp` | 各キーイベント。戻り値 `bool` は「消費したか」 |
| `onDraw` | オーバーレイ描画 |

### ライフサイクル

`keyUseCases_` に最初に一度だけ生成して保持。各キーイベントごとに `canHandle` を評価し、true のものを `activeKeyUseCases_` に集めてディスパッチします。Primary/Secondary の勝者選定はなく、`canHandle` がそのままゲートになります。

---

## 8. `HandlingRole` による調停

`src/interaction/HandlingRole.h`。`InteractionUseCase::canHandle` の戻り値で、複数候補が競合したときの扱いを決めます。**`InteractionUseCase` 系専用**で、`CommandUseCase` / `KeyUseCase` には関係しません。設計上の意味は [§2.2](#22-原則b-関心事を軸にした3層の役割モデル) を参照。

| ロール | 意味 |
|---|---|
| `None` | このイベントは処理しない（候補から外れる） |
| `Primary` | 主処理を担いたい。候補のうち **1つだけが勝者**（`findWinnerPrimary`）になる |
| `Secondary` | Primary 勝者と**同じ型**のときに併走する補助。複数文字への同時適用の実体 |
| `SecondaryIfOtherOwned` | 選択所有権に応じて Secondary 化／非活性を切り替える（派生元プロジェクト由来。本SDKでは出番が少ない） |
| `Passive` | 勝者と無関係に並行して動ける（`PassiveUseCase` が返す） |

`UseCaseRouter::activateUseCases` が候補を集め、`findWinnerPrimary` で最初の `Primary` 候補を勝者に選び、各候補のロールに応じてアクティブ化します（`UseCaseRouter.cc:531` 付近）。

---

## 9. `UseCaseRouter` から見た全体像

```
            ┌────────────────── UseCaseRouter ──────────────────┐
            │                                                    │
 PenDown ──▶│ factories_  ─(毎回生成)→ currentUseCases_          │ InteractionUseCase
 PenMotion  │   canHandle → HandlingRole → findWinnerPrimary     │  / PassiveUseCase
 PenUp ────▶│   penDown/Motion/Up → UseCaseResult → 反映/終了     │
            │                                                    │
 KeyDown ──▶│ keyUseCases_ ─(常駐)→ canHandle(bool) → onKeyXxx   │ KeyUseCase
            │                                                    │
 ボタン変更─▶│ commandUseCases_ ─(常駐)→ targetParameterIds       │ CommandUseCase
            │   isCommandTrigger → onTriggerCommand → execute    │
            └────────────────────────────────────────────────────┘
```

- **保持の単位**: InteractionUseCase は**起動ごとに生成・破棄**。Command と Key は**常駐**。
- **調停**: InteractionUseCase だけが `HandlingRole` で勝者選定を受ける。Key と Command は `canHandle`/`targetParameterIds` がそのままゲート。
- **パラメータ権限**: Passive だけが型レベルで `ParameterManager` を遮断。他は書き換え可能。
- **連携**: UseCase 同士は直接依存せず、Intent システム経由で疎結合に状態を共有する（→ [interaction-intent.md](interaction-intent.md)）。

---

## 10. 新しい UseCase を追加するには

1. 目的に合うベースクラスを選ぶ（パラメータを変える能動操作 → `InteractionUseCase` / フィードバックのみ → `PassiveUseCase` / ボタン → `CommandUseCase` / ショートカット → `KeyUseCase`）。
2. `src/interaction/usecases/` に `.h/.cc` を追加し、必要なメソッドを実装する。
3. `UseCaseRouter` のコンストラクタ（`UseCaseRouter.cc:24` 付近）で対応するコンテナに登録する：
   - InteractionUseCase / PassiveUseCase → `factories_.emplace_back([]{ return std::make_unique<Xxx>(); });`
   - CommandUseCase → `commandUseCases_.emplace_back(std::make_unique<Xxx>());`
   - KeyUseCase → `keyUseCases_.emplace_back(std::make_unique<Xxx>());`
4. 他 UseCase と連携する場合は `declaredEmittedIntents` / `declaredConsumedIntents` を宣言する（→ [interaction-intent.md](interaction-intent.md)）。

---

## 関連ドキュメント

- [interaction-intent.md](interaction-intent.md) — UseCase 間の疎結合な状態共有（Intent システム）
