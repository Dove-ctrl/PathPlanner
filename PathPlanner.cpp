// PathPlanner.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "PathPlanner.h"

#include <windows.h>

// 声明窗口过程函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// 程序入口点
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,      // 应用程序实例句柄
    _In_opt_ HINSTANCE hPrevInstance,  // 旧式 Windows 参数（已废弃）
    _In_ LPSTR lpCmdLine,          // 命令行参数
    _In_ int nCmdShow              // 窗口显示方式
) {
    // 1. 注册窗口类
    const wchar_t CLASS_NAME[] = L"PathPlanner";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PATHPLANNER)); // 加载图标
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, L"窗口类注册失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // 2. 创建窗口
    HWND hWnd = CreateWindow(
        CLASS_NAME,                // 窗口类名称
        L"PathPlanner",         // 窗口标题
        WS_OVERLAPPEDWINDOW,       // 窗口样式

        // 初始位置和大小
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,

        NULL,       // 父窗口句柄
        LoadMenu(hInstance, MAKEINTRESOURCE(IDC_PATHPLANNER)),
        hInstance,  // 应用程序实例
        NULL        // 附加数据
    );

    if (hWnd == NULL) {
        MessageBox(NULL, L"窗口创建失败!", L"错误", MB_ICONERROR);
        return 0;
    }

    // 3. 显示窗口
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);  // 强制立即重绘

    // 4. 消息循环
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);  // 转换键盘消息
        DispatchMessage(&msg);  // 分发到窗口过程
    }

    return (int)msg.wParam;
}

// 对话框过程函数（处理关于窗口的消息）
INT_PTR CALLBACK AboutDialogProc(
    HWND hDlg,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (msg) {
    case WM_INITDIALOG:
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam)); // 关闭对话框
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// 5. 窗口过程函数
LRESULT CALLBACK WndProc(
    HWND hWnd, UINT message,
    WPARAM wParam, LPARAM lParam
) {
    switch (message) {
    case WM_DESTROY:  // 窗口被销毁
        PostQuitMessage(0);  // 退出消息循环
        return 0;

    case WM_PAINT: {  // 窗口需要重绘
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // 在此处添加绘图代码
        TextOut(hdc, 50, 50, L"Hello, Win32!", 12);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDM_ABOUT:
            // 显示关于对话框（模态对话框）
            DialogBox(
                GetModuleHandle(NULL),  // 当前实例句柄
                MAKEINTRESOURCE(IDD_ABOUTBOX), // 对话框资源 ID
                hWnd,                  // 父窗口
                AboutDialogProc         // 对话框过程函数
            );
            break;
        }
        return 0;
    }

                 // 其他消息处理...
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}