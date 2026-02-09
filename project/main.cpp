#include <Windows.h>
#include "project/Game.h"

// 最小限の WinMain: Framework のポリモーフィズムを使って Game を実行する
// - ポインタで管理することで派生クラスを抽象基底として扱える
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // Framework を基底ポインタで扱う
    Framework* game = new Game();
    int result = game->Run(hInstance, nCmdShow);
    delete game; // 仮想デストラクタにより安全に破棄される
    return result;
}
