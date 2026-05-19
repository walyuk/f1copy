F1COPY Ver 0.9 - Readme

■ ダウンロード
f1copy.exe は GitHub Releases（Assets）から入手できます:
https://github.com/yukioshibata/f1copy/releases

■ 概要
F1COPYは、Windowsキーボードの操作を効率化するためのユーティリティツールです。
F1/F2キーによるコピー＆ペースト、CapsLockのCtrl化などの機能を提供します。

■ 主な機能
・F1 キー        -> Ctrl+C (コピー) として動作
・F2 キー        -> Ctrl+V (貼り付け) として動作
・CapsLock キー  -> Ctrl キー として動作
・ScrollLock キー -> CapsLock キー として動作
・CapsLock + F1  -> 元々の F1 キー として動作
・CapsLock + F2  -> 元々の F2 キー として動作

■ 使用方法
1. 通常起動
   f1copy.exe を実行するとタスクトレイに常駐します。
   起動時に設定内容が画面中央に表示されます。

2. 終了方法
   タスクトレイのアイコンをダブルクリックすると終了します。

3. インストール（自動起動設定）
   コマンドプロンプト等から以下のコマンドを実行してください。
   f1copy.exe -install
   ※タスクスケジューラに登録され、PC起動時に自動で実行されるようになります。
   ※インストール後はPCを再起動してください。

4. アンインストール
   コマンドプロンプト等から以下のコマンドを実行してください。
   f1copy.exe -uninstall
   ※自動起動の設定が削除されます。

■ 動作環境
・Windows 11 64bit
