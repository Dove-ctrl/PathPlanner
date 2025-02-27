// PathPlanner.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "PathPlanner.h"

#include <windows.h>

// 全局变量
HBITMAP g_hCurrentBitmap = NULL;  // 当前显示的位图
int g_nCurrentImageID = 0;        // 当前图片资源ID（0=无）
bool g_bShowAxis = false;  // 初始显示坐标轴
struct {
    int x;      // 图片显示区域左上角X坐标
    int y;      // 图片显示区域左上角Y坐标
    int width;  // 图片显示宽度
    int height; // 图片显示高度
} g_imageRect = { 0 };


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
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, // 关键样式       // 窗口样式

        // 初始位置和大小
        CW_USEDEFAULT, CW_USEDEFAULT,
        1080, 1080,

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

void DrawCoordinateAxis(HDC hdc) {
    if (!g_hCurrentBitmap) return; // 无图片时不绘制

    // 计算图片中心点
    POINT origin = {
        g_imageRect.x + g_imageRect.width / 2,
        g_imageRect.y + g_imageRect.height / 2
    };

    // 创建画笔
    HPEN hAxisPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hAxisPen);

    // 绘制X轴（横跨整个窗口）
    MoveToEx(hdc, 0, origin.y, NULL);
    LineTo(hdc, GetSystemMetrics(SM_CXSCREEN), origin.y); // 或使用窗口宽度

    // 绘制Y轴（横跨整个窗口）
    MoveToEx(hdc, origin.x, 0, NULL);
    LineTo(hdc, origin.x, GetSystemMetrics(SM_CYSCREEN));

    // 恢复资源
    SelectObject(hdc, hOldPen);
    DeleteObject(hAxisPen);
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
    case WM_CREATE: {
        // 获取窗口菜单句柄
        HMENU hMenu = GetMenu(hWnd);

        // 设置初始未勾选状态
        CheckMenuItem(
            hMenu,              // 菜单句柄
            IDM_AXIS,     // 菜单项ID
            MF_BYCOMMAND | MF_UNCHECKED // 取消勾选
        );

        // 其他初始化代码...
        return 0;
    }
    case WM_DESTROY:  // 窗口被销毁
        if (g_hCurrentBitmap) DeleteObject(g_hCurrentBitmap);
        PostQuitMessage(0);  // 退出消息循环
        return 0;

    case WM_PAINT: {  // 窗口需要重绘
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // 在此处添加绘图代码
        // 清空背景
        RECT rect;
        GetClientRect(hWnd, &rect);
        FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));

        // 如果标志为真，绘制图片
        if (g_hCurrentBitmap) {
            HDC hdcMem = CreateCompatibleDC(hdc);
            SelectObject(hdcMem, g_hCurrentBitmap);

            BITMAP bmp;
            GetObject(g_hCurrentBitmap, sizeof(BITMAP), &bmp);

            // 居中绘制图片
            int srcWidth = bmp.bmWidth;
            int srcHeight = bmp.bmHeight;
            int destWidth, destHeight;
            int offsetX = 0, offsetY = 0;

            // 计算缩放比例
            float scaleX = (float)rect.right / srcWidth;
            float scaleY = (float)rect.bottom / srcHeight;
            float scale = min(scaleX, scaleY); // 按最小比例缩放（保持宽高比）

            destWidth = (int)(srcWidth * scale);
            destHeight = (int)(srcHeight * scale);

            // 计算居中位置
            offsetX = (rect.right - destWidth) / 2;
            offsetY = (rect.bottom - destHeight) / 2;

            SetStretchBltMode(hdc, HALFTONE);  // 启用高质量缩放
            SetBrushOrgEx(hdc, 0, 0, NULL);   // HALFTONE 模式需要重置画刷原点

            float _scale = min(
                (float)rect.right / bmp.bmWidth,
                (float)rect.bottom / bmp.bmHeight
            );
            g_imageRect.width = (int)(bmp.bmWidth * _scale);
            g_imageRect.height = (int)(bmp.bmHeight * _scale);
            g_imageRect.x = (rect.right - g_imageRect.width) / 2;
            g_imageRect.y = (rect.bottom - g_imageRect.height) / 2;

            // 绘制（带缩放）
            StretchBlt(
                hdc,
                offsetX, offsetY,
                destWidth, destHeight,
                hdcMem,
                0, 0,
                srcWidth, srcHeight,
                SRCCOPY
            );

            DeleteDC(hdcMem);
        }

        if (g_bShowAxis) {
            DrawCoordinateAxis(hdc);
        }
        
        if(g_hCurrentBitmap == 0){
            const wchar_t* tipText = L"点击设置选择场地";

            // 设置字体（使用系统默认GUI字体）
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SelectObject(hdc, hFont);

            // 设置文字颜色
            SetTextColor(hdc, RGB(150, 150, 150));  // 灰色
            SetBkMode(hdc, TRANSPARENT);  // 透明背景

            // 计算文字位置（居中）
            SIZE textSize;
            GetTextExtentPoint32(hdc, tipText, wcslen(tipText), &textSize);
            int x = (rect.right - textSize.cx) / 2;
            int y = (rect.bottom - textSize.cy) / 2;

            // 绘制文字
            TextOut(hdc, x, y, tipText, wcslen(tipText));
        }

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

        case IDM_V5RC_SKILL:  // 菜单项ID（假设为4001）
            g_nCurrentImageID = IDB_BITMAP1;
            break;

        case IDM_V5RC_TOURNAMENT:  // 菜单项ID（假设为4001）
            g_nCurrentImageID = IDB_BITMAP2;
            break;

        case IDM_VURC_SKILL:  // 菜单项ID（假设为4001）
            g_nCurrentImageID = IDB_BITMAP3;
            break;

        case IDM_VURC_TOURNAMENT:  // 菜单项ID（假设为4001）
            g_nCurrentImageID = IDB_BITMAP4;
            break;

        case IDM_AXIS:  // 切换坐标轴显示
            g_bShowAxis = !g_bShowAxis;

            // 更新菜单勾选状态
            HMENU hMenu = GetMenu(hWnd);
            CheckMenuItem(hMenu, IDM_AXIS,
                g_bShowAxis ? MF_CHECKED : MF_UNCHECKED);

            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }

        // 释放旧位图
        if (g_hCurrentBitmap) {
            DeleteObject(g_hCurrentBitmap);
            g_hCurrentBitmap = NULL;
        }

        // 加载新位图
        if (g_nCurrentImageID != 0) {
            g_hCurrentBitmap = LoadBitmap(
                GetModuleHandle(NULL),
                MAKEINTRESOURCE(g_nCurrentImageID)
            );
        }

        // 强制重绘
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;
    }

                 // 其他消息处理...
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}