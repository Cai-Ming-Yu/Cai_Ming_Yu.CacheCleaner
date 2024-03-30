# 彩銘羽 CacheCleaner
アプリの起動時にキャッシュをクリアするよ！

このリポジトリは[ここ](https://github.com/Xposed-Modules-Repo/Cai_Ming_Yu.CacheCleaner)と同じです。

Xposedを使用してアプリの起動時にキャッシュをクリアします。

This repo is exactly the same as [this one](https://github.com/Xposed-Modules-Repo/Cai_Ming_Yu.CacheCleaner).

Use Xposed to clear the app's cache data when it starts.

# Magisk(KernelSU/APatch) module is already supported!
It supports more functions and is easier to use, it is highly recommended to use it with the Xposed version, please refer to the [config file](https://github.com/Cai-Ming-Yu/CMY-CacheCleaner/blob/C-M-Y/mod/source/config.yaml) for specific function support.

## Env
- LSPosed or any other framework that can use the Xposed module is installed
- LSPosedまたはXposed、その他フレームワークがインストールされている環境が必要です

## How to use
Install the latest version of the Xposed module released [here](https://github.com/Cai-Ming-Yu/CMY-CacheCleaner/releases), or package it into other app via frameworks such as LSPatch.

ここでリリースをしているXposedモジュールの最新版をインストールするか、LSPatchなどのフレームワークを使用して他のアプリに埋め込んでください。

If you are not packaging into other app using a tool like LSPatch, enable this module in an Xposed framework like LSPosed and select the required app.

LSPatchのようなツールを使って他のアプリに埋め込んでいない場合は、LSPosedのようなXposedフレームワークでこのモジュールを有効化し、必要なアプリを選択してください。

You can also build the app yourself by downloading the source code for this project(App builds via e.g. ```./gradlew assembleRelease```).

このプロジェクトのソースコードをダウンロードして、自分自身でアプリをビルドする事もできます。(アプリのビルドは、```./gradlew assembleRelease``` などで行います)

CI Version: https://github.com/Cai-Ming-Yu/CMY-CacheCleaner/actions

## [License](https://github.com/Cai-Ming-Yu/CMY-CacheCleaner/blob/C-M-Y/LICENSE)
