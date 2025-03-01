// PathPlanner.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "PathPlanner.h"

#include <windows.h>
#include <vector>

#define IDM_RIGHT_CREATE_CURVE      5001  // 新命令：创建二阶贝塞尔曲线
#define IDM_RIGHT_DELETE_CURVE      5002  // 新命令：删除曲线

// 全局变量
HBITMAP g_hCurrentBitmap = NULL;  // 当前显示的位图
int g_nCurrentImageID = 0;        // 当前图片资源ID（0=无）
bool g_bShowAxis = false;  // 初始显示坐标轴
RECT g_imageRect = { 0 };

bool bReloadImage = false;  // 标识是否需要重新加载图片

enum BezierType { Quadratic, Cubic, Quartic};
BezierType g_bezier_type = Quadratic;

bool g_quadraticCurveCreated = false;
POINT g_quadControlPoints[3]; // 三个控制点
std::vector<POINT> g_quadraticCurvePoints; // 曲线点集合
int g_draggedControlPoint = -1; // 正在拖动的控制点索引 (-1表示没有)
bool g_isDragging = false;

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

std::vector<POINT> CalculateQuadraticBezier(POINT p0, POINT p1, POINT p2, float step = 0.01f) {
    std::vector<POINT> result;
    for (float t = 0; t <= 1.0f; t += step) {
        float u = 1 - t;
        POINT pt;
        pt.x = (int)(u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x);
        pt.y = (int)(u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y);
        result.push_back(pt);
    }
    return result;
}

std::vector<POINT> CalculateCubicBezier(POINT p0, POINT p1, POINT p2, POINT p3, float step = 0.01f) {
    std::vector<POINT> result;
    for (float t = 0; t <= 1.0f; t += step) {
        float u = 1 - t;
        POINT pt;
        pt.x = (int)(u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x + t * t * t * p3.x);
        pt.y = (int)(u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y + t * t * t * p3.y);
        result.push_back(pt);
    }
    return result;
}

std::vector<POINT> CalculateQuarticBezier(POINT p0, POINT p1, POINT p2, POINT p3, POINT p4, float step = 0.01f) {
    std::vector<POINT> result;
    for (float t = 0; t <= 1.0f; t += step) {
        float u = 1 - t;
        POINT pt;
        pt.x = (int)(
            u * u * u * u * p0.x +
            4 * u * u * u * t * p1.x +
            6 * u * u * t * t * p2.x +
            4 * u * t * t * t * p3.x +
            t * t * t * t * p4.x
            );
        pt.y = (int)(
            u * u * u * u * p0.y +
            4 * u * u * u * t * p1.y +
            6 * u * u * t * t * p2.y +
            4 * u * t * t * t * p3.y +
            t * t * t * t * p4.y
            );
        result.push_back(pt);
    }
    return result;
}

double Distance(POINT a, POINT b) {
    int dx = a.x - b.x;
    int dy = a.y - b.y;
    return sqrt((double)(dx * dx + dy * dy));
}

bool IsPointNearCurve(POINT pt) {
    const int threshold = 25;
    for (const auto& curvePt : g_quadraticCurvePoints) {
        if (Distance(pt, curvePt) <= threshold)
            return true;
    }
    return false;
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

        // 其他初始化代码...
        return 0;
    }
    case WM_ERASEBKGND: {
        // 禁用默认背景擦除，防止闪烁
        return 1;
    }
    case WM_DESTROY: {  // 窗口被销毁
        if (g_hCurrentBitmap) DeleteObject(g_hCurrentBitmap);
        PostQuitMessage(0);  // 退出消息循环
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // === 双缓冲初始化 ===
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, ps.rcPaint.right, ps.rcPaint.bottom);
        SelectObject(hdcMem, hbmMem);

        // === 1. 清空背景 ===
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        FillRect(hdcMem, &clientRect, (HBRUSH)(COLOR_WINDOW + 1));

        // === 2. 绘制图片 ===
        if (g_hCurrentBitmap) {
            HDC hdcImage = CreateCompatibleDC(hdcMem);
            SelectObject(hdcImage, g_hCurrentBitmap);
            BITMAP bmp;
            GetObject(g_hCurrentBitmap, sizeof(BITMAP), &bmp);

            // 计算图片显示区域
            float scale = min(
                (float)clientRect.right / bmp.bmWidth,
                (float)clientRect.bottom / bmp.bmHeight
            );
            g_imageRect.left = (clientRect.right - (int)(bmp.bmWidth * scale)) / 2;
            g_imageRect.top = (clientRect.bottom - (int)(bmp.bmHeight * scale)) / 2;
            g_imageRect.right = g_imageRect.left + (int)(bmp.bmWidth * scale);
            g_imageRect.bottom = g_imageRect.top + (int)(bmp.bmHeight * scale);

            // 绘制图片到内存DC
            SetStretchBltMode(hdcMem, HALFTONE);
            StretchBlt(
                hdcMem, g_imageRect.left, g_imageRect.top,
                g_imageRect.right - g_imageRect.left,
                g_imageRect.bottom - g_imageRect.top,
                hdcImage, 0, 0, bmp.bmWidth, bmp.bmHeight, SRCCOPY
            );
            DeleteDC(hdcImage);
        }

        if (g_bShowAxis && g_hCurrentBitmap) {

            // 计算图片中心点
            POINT origin = {
                g_imageRect.left + (g_imageRect.right - g_imageRect.left) / 2,
                g_imageRect.top + (g_imageRect.bottom - g_imageRect.top) / 2
            };

            // 计算图片宽高
            int imgWidth = g_imageRect.right - g_imageRect.left;
            int imgHeight = g_imageRect.bottom - g_imageRect.top;

            // 绘制坐标标签
            TextOut(hdcMem, origin.x + 5, origin.y + 5, L"(0,0)", 5);                      // 原点
            TextOut(hdcMem, origin.x + imgWidth / 2 - 50, origin.y + 5, L"(1784,0)", 8);   // X轴正方向
            TextOut(hdcMem, origin.x + 5, origin.y - imgHeight / 2 + 5, L"(0,1784)", 8);   // Y轴正方向
            TextOut(hdcMem, origin.x - imgWidth / 2 + 5, origin.y + 5, L"(-1784,0)", 8);   // X轴负方向
            TextOut(hdcMem, origin.x + 5, origin.y + imgHeight / 2 - 20, L"(0,-1784)", 8); // Y轴负方向

            // 创建画笔
            HPEN hAxisPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
            HPEN hOldPen = (HPEN)SelectObject(hdcMem, hAxisPen);

            // 绘制X轴（限制在图片区域内）
            MoveToEx(hdcMem, g_imageRect.left, origin.y, NULL);
            LineTo(hdcMem, g_imageRect.right, origin.y);

            // 绘制Y轴（限制在图片区域内）
            MoveToEx(hdcMem, origin.x, g_imageRect.top, NULL);
            LineTo(hdcMem, origin.x, g_imageRect.bottom);

            // 恢复资源
            SelectObject(hdcMem, hOldPen);
            DeleteObject(hAxisPen);
        }

        if (g_bezier_type == Quadratic && g_hCurrentBitmap && g_quadraticCurveCreated) {
            // 重新计算曲线点
            g_quadraticCurvePoints = CalculateQuadraticBezier(g_quadControlPoints[0], g_quadControlPoints[1], g_quadControlPoints[2]);
            // 绘制曲线
            HPEN hCurvePen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
            HPEN hOldPen = (HPEN)SelectObject(hdcMem, hCurvePen);
            if (!g_quadraticCurvePoints.empty())
                Polyline(hdcMem, &g_quadraticCurvePoints[0], (int)g_quadraticCurvePoints.size());
            SelectObject(hdcMem, hOldPen);
            DeleteObject(hCurvePen);

            // 绘制控制点（绘制为小圆点）
            HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 255));
            for (int i = 0; i < 3; i++) {
                int x = g_quadControlPoints[i].x;
                int y = g_quadControlPoints[i].y;
                Ellipse(hdcMem, x - 5, y - 5, x + 5, y + 5);
            }
            DeleteObject(hBrush);
        }
        else if (g_bezier_type == Cubic && g_hCurrentBitmap) {

        }
        else if (g_bezier_type == Quartic && g_hCurrentBitmap) {

        }

        if (g_hCurrentBitmap == NULL) {
            const wchar_t* tipText = L"点击设置选择场地";
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);
            SetTextColor(hdcMem, RGB(150, 150, 150));
            SetBkMode(hdcMem, TRANSPARENT);
            TextOut(hdcMem, 460, 500, tipText, wcslen(tipText));
            SelectObject(hdcMem, hOldFont);
        }

        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, hdcMem, 0, 0, SRCCOPY);

        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_CONTEXTMENU: {
        if (g_hCurrentBitmap) {
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);
            HMENU hPopup = CreatePopupMenu();
            if (g_quadraticCurveCreated && IsPointNearCurve(pt)) {
                AppendMenu(hPopup, MF_STRING, IDM_RIGHT_DELETE_CURVE, L"删除曲线");
            }
            else {
                AppendMenu(hPopup, MF_STRING, IDM_RIGHT_CREATE_CURVE, L"创建二阶贝塞尔曲线");
            }
            TrackPopupMenu(hPopup, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
            DestroyMenu(hPopup);
        }
        return 0;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        HMENU hMenu = GetMenu(hWnd);

        switch (wmId) {
        case IDM_ABOUT: {
            // 显示关于对话框（模态对话框）
            DialogBox(
                GetModuleHandle(NULL),  // 当前实例句柄
                MAKEINTRESOURCE(IDD_ABOUTBOX), // 对话框资源 ID
                hWnd,                  // 父窗口
                AboutDialogProc         // 对话框过程函数
            );
            break;
        }
        case IDM_V5RC_SKILL: {
            g_nCurrentImageID = IDB_BITMAP1;
            bReloadImage = true;
            break;
        }
        case IDM_V5RC_TOURNAMENT: {
            g_nCurrentImageID = IDB_BITMAP2;
            bReloadImage = true;
            break;
        }
        case IDM_VURC_SKILL: {
            g_nCurrentImageID = IDB_BITMAP3;
            bReloadImage = true;
            break;
        }
        case IDM_VURC_TOURNAMENT: {
            g_nCurrentImageID = IDB_BITMAP4;
            bReloadImage = true;
            break;
        }
        case IDM_AXIS: {  // 切换坐标轴显示
            g_bShowAxis = !g_bShowAxis;

            // 更新菜单勾选状态
            CheckMenuItem(hMenu, IDM_AXIS,
                g_bShowAxis ? MF_CHECKED : MF_UNCHECKED);

            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        case IDM_2BEZIER: { // 2阶贝塞尔
            g_bezier_type = Quadratic;

            // 更新菜单勾选状态
            CheckMenuItem(hMenu, IDM_2BEZIER, MF_CHECKED);
            CheckMenuItem(hMenu, IDM_3BEZIER, MF_UNCHECKED);
            CheckMenuItem(hMenu, IDM_4BEZIER, MF_UNCHECKED);

            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        case IDM_3BEZIER: { // 3阶贝塞尔
            g_bezier_type = Cubic;

            // 更新菜单勾选状态
            CheckMenuItem(hMenu, IDM_2BEZIER, MF_UNCHECKED);
            CheckMenuItem(hMenu, IDM_3BEZIER, MF_CHECKED);
            CheckMenuItem(hMenu, IDM_4BEZIER, MF_UNCHECKED);

            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        case IDM_4BEZIER:{ // 4阶贝塞尔
            g_bezier_type = Quartic;

            // 更新菜单勾选状态
            CheckMenuItem(hMenu, IDM_2BEZIER, MF_UNCHECKED);
            CheckMenuItem(hMenu, IDM_3BEZIER, MF_UNCHECKED);
            CheckMenuItem(hMenu, IDM_4BEZIER, MF_CHECKED);

            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        case IDM_RIGHT_CREATE_CURVE: {
            // 初始化控制点：在图片区域内居中分布
            if (!g_quadraticCurveCreated && g_hCurrentBitmap) {
                g_quadraticCurveCreated = true;
                int cx = (g_imageRect.left + g_imageRect.right) / 2;
                int cy = (g_imageRect.top + g_imageRect.bottom) / 2;
                g_quadControlPoints[0] = { cx - 100, cy };
                g_quadControlPoints[1] = { cx, cy - 100 };
                g_quadControlPoints[2] = { cx + 100, cy };
            }
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }
        case IDM_RIGHT_DELETE_CURVE: {
            g_quadraticCurveCreated = false;
            g_quadraticCurvePoints.clear();
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        }

        }

        // 只有在需要切换图片的时候才重新加载位图
        if (bReloadImage) {
            if (g_hCurrentBitmap) {
                DeleteObject(g_hCurrentBitmap);
                g_hCurrentBitmap = NULL;
            }
            if (g_nCurrentImageID != 0) {
                g_hCurrentBitmap = LoadBitmap(
                    GetModuleHandle(NULL),
                    MAKEINTRESOURCE(g_nCurrentImageID)
                );
            }
        }

        // 强制重绘
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt;
        pt.x = LOWORD(lParam);
        pt.y = HIWORD(lParam);
        // 检查是否点击到任一控制点（半径5像素）
        if (g_quadraticCurveCreated) {
            for (int i = 0; i < 3; i++) {
                if (Distance(pt, g_quadControlPoints[i]) <= 5) {
                    g_draggedControlPoint = i;
                    g_isDragging = true;
                    SetCapture(hWnd);
                    break;
                }
            }
        }
        break;
    }
    case WM_MOUSEMOVE: {
        if (g_isDragging && g_draggedControlPoint != -1) {
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);
            // 更新控制点位置
            g_quadControlPoints[g_draggedControlPoint] = pt;
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;
    }
    case WM_LBUTTONUP: {
        if (g_isDragging) {
            g_isDragging = false;
            g_draggedControlPoint = -1;
            ReleaseCapture();
        }
        break;
    }
                 // 其他消息处理...
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}