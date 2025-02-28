// PathPlanner.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "PathPlanner.h"

#include <windows.h>
#include <vector>

#define IDM_RIGHT_CREATE_POINT     5001  
#define IDM_RIGHT_DELETE_POINT      5002  
#define IDM_RIGHT_CREATE_CURVE      5002  

// 全局变量
HBITMAP g_hCurrentBitmap = NULL;  // 当前显示的位图
int g_nCurrentImageID = 0;        // 当前图片资源ID（0=无）
bool g_bShowAxis = false;  // 初始显示坐标轴
RECT g_imageRect = { 0 };

enum BezierType { Quadratic, Cubic, Quartic};
BezierType g_bezier_type = Quadratic;

struct ControlPoint {
    POINT pos;
    COLORREF color;
};

std::vector<ControlPoint> g_controlPoints; // 存储所有控制点
int g_selectedPoint = -1;                 // 当前选中的点索引（-1表示未选中）
bool g_dragging = false;                  // 是否正在拖动
const int POINT_RADIUS = 5;               // 控制点半径

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

        if (g_bezier_type == Quadratic && g_hCurrentBitmap) {

            if (g_controlPoints.size() >= 3) {
                // 转换为图片相对坐标（假设曲线坐标基于图片区域）
                std::vector<POINT> imgPoints;
                for (const auto& cp : g_controlPoints) {
                    imgPoints.push_back({
                        cp.pos.x - g_imageRect.left,
                        cp.pos.y - g_imageRect.top
                        });
                }

                // 计算曲线路径
                auto curvePoints = CalculateQuadraticBezier(imgPoints[0], imgPoints[1], imgPoints[2]);

                // 转换为窗口绝对坐标
                HPEN hCurvePen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
                SelectObject(hdcMem, hCurvePen);
                for (size_t i = 0; i < curvePoints.size(); i++) {
                    curvePoints[i].x += g_imageRect.left;
                    curvePoints[i].y += g_imageRect.top;
                }
                Polyline(hdcMem, curvePoints.data(), (int)curvePoints.size());
                DeleteObject(hCurvePen);
            }
        }
        else if (g_bezier_type == Cubic && g_hCurrentBitmap) {

        }
        else if (g_bezier_type == Quartic && g_hCurrentBitmap) {

        }

        for (const auto& cp : g_controlPoints) {
            HBRUSH hBrush = CreateSolidBrush(cp.color);
            SelectObject(hdcMem, hBrush);
            Ellipse(hdcMem,
                cp.pos.x - POINT_RADIUS, cp.pos.y - POINT_RADIUS,
                cp.pos.x + POINT_RADIUS, cp.pos.y + POINT_RADIUS
            );
            DeleteObject(hBrush);
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
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        // 检测是否点击了已有控制点
        for (size_t i = 0; i < g_controlPoints.size(); i++) {
            int dx = g_controlPoints[i].pos.x - x;
            int dy = g_controlPoints[i].pos.y - y;
            if (dx * dx + dy * dy <= POINT_RADIUS * POINT_RADIUS) {
                g_selectedPoint = i;
                g_dragging = true;
                SetCapture(hWnd); // 捕获鼠标
                break;
            }
        }

        // 未选中点时添加新控制点
        if (!g_dragging) {
            ControlPoint newPoint = { {x, y}, RGB(0, 255, 0) }; // 绿色默认
            if (g_controlPoints.size() >= 1) {
                newPoint.color = RGB(255, 0, 0); // 第二个及之后设为红色
            }
            g_controlPoints.push_back(newPoint);
            InvalidateRect(hWnd, NULL, TRUE);
        }

        return 0;
    }
    case WM_MOUSEMOVE: { // 鼠标移动：拖动控制点
        if (g_dragging && g_selectedPoint != -1) {
            g_controlPoints[g_selectedPoint].pos.x = LOWORD(lParam);
            g_controlPoints[g_selectedPoint].pos.y = HIWORD(lParam);
        }
        return 0;
    }
    case WM_LBUTTONUP: { // 左键释放：结束拖动
        if (g_dragging) {
            g_dragging = false;
            g_selectedPoint = -1;
        }
        return 0;
    }
    case WM_RBUTTONDOWN: {

        if (g_hCurrentBitmap) {
            // 创建右键弹出菜单
            HMENU hpopMenu = CreatePopupMenu();
            if (g_bezier_type == Quadratic) {
                AppendMenuW(
                    hpopMenu, 
                    MF_STRING | MF_DISABLED,
                    0, 
                    L"当前：2阶曲线"
                );
            }
            else if (g_bezier_type == Cubic) {
                AppendMenuW(
                    hpopMenu,
                    MF_STRING | MF_DISABLED,
                    0,
                    L"当前：3阶曲线"
                );
            }
            else if (g_bezier_type == Quartic) {
                AppendMenuW(
                    hpopMenu,
                    MF_STRING | MF_DISABLED,
                    0,
                    L"当前：4阶曲线"
                );
            }
            
            AppendMenuW(hpopMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hpopMenu, MF_STRING, IDM_RIGHT_CREATE_CURVE, L"创建曲线");
            AppendMenuW(hpopMenu, MF_STRING, IDM_RIGHT_CREATE_POINT, L"创建控制点");
            AppendMenuW(hpopMenu, MF_STRING, IDM_RIGHT_DELETE_POINT, L"删除控制点/曲线");

            // 获取鼠标位置（屏幕坐标）
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            ClientToScreen(hWnd, &pt);

            // 显示菜单并跟踪选项
            TrackPopupMenu(
                hpopMenu,
                TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
                pt.x,
                pt.y,
                0,
                hWnd,
                NULL
            );

            DestroyMenu(hpopMenu);
        }
        
        return 0;
    }
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        HMENU hMenu = GetMenu(hWnd);
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

        case IDM_V5RC_SKILL:  
            g_nCurrentImageID = IDB_BITMAP1;
            break;

        case IDM_V5RC_TOURNAMENT:  
            g_nCurrentImageID = IDB_BITMAP2;
            break;

        case IDM_VURC_SKILL:  
            g_nCurrentImageID = IDB_BITMAP3;
            break;

        case IDM_VURC_TOURNAMENT:  
            g_nCurrentImageID = IDB_BITMAP4;
            break;

        case IDM_AXIS:  // 切换坐标轴显示
            g_bShowAxis = !g_bShowAxis;

            // 更新菜单勾选状态
            CheckMenuItem(hMenu, IDM_AXIS,
                g_bShowAxis ? MF_CHECKED : MF_UNCHECKED);

            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case IDM_2BEZIER: // 2阶贝塞尔
            g_bezier_type = Quadratic;
            
            // 更新菜单勾选状态
            CheckMenuItem(hMenu, IDM_2BEZIER, MF_CHECKED);
            CheckMenuItem(hMenu, IDM_3BEZIER, MF_UNCHECKED);
            CheckMenuItem(hMenu, IDM_4BEZIER, MF_UNCHECKED);

            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case IDM_3BEZIER: // 3阶贝塞尔
            g_bezier_type = Cubic;

            // 更新菜单勾选状态
            CheckMenuItem(hMenu, IDM_2BEZIER, MF_UNCHECKED);
            CheckMenuItem(hMenu, IDM_3BEZIER, MF_CHECKED);
            CheckMenuItem(hMenu, IDM_4BEZIER, MF_UNCHECKED);

            InvalidateRect(hWnd, NULL, TRUE);
            break;

        case IDM_4BEZIER: // 4阶贝塞尔
            g_bezier_type = Quartic;

            // 更新菜单勾选状态
            CheckMenuItem(hMenu, IDM_2BEZIER, MF_UNCHECKED);
            CheckMenuItem(hMenu, IDM_3BEZIER, MF_UNCHECKED);
            CheckMenuItem(hMenu, IDM_4BEZIER, MF_CHECKED);

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