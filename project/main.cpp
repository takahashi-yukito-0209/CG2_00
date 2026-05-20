#include <Windows.h>
#include "application/Game.h"

/// Windowsアプリケーションのエントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // Game クラスのインスタンスを生成して実行する
    Framework* game = new Game();

    // Run() メソッドは初期化、メインループ、終了処理をまとめて行う
    int result = game->Run(hInstance, nCmdShow);

    // Game クラスのインスタンスを削除してリソースを解放する
    delete game;

    // アプリケーションの終了コードを返す
    return result;
}
