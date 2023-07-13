// Dear ImGui: standalone example application for DirectX 11
// If you are new to Dear ImGui, read documentation from the docs/ dir + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#define NOMINMAX

#include <chrono>
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "TINRenderer.h"
#include "ClipmapRenderer.h"
#include "Camera.h"

#include "Texture2D.h"
#include "TerrainSystem.h"
#include "../HeightMapSplitter/ThreadPool.h"
#include <directxtk/GeometricPrimitive.h>
#include <d3d11_3.h>

#include "DebugRenderer.h"

// Data
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

using namespace DirectX::SimpleMath;

ThreadPool g_ThreadPool(std::thread::hardware_concurrency());

namespace
{
    std::unique_ptr<DirectX::Texture2D> g_depthStencil = nullptr;
    std::unique_ptr<TINRenderer> g_MeshRenderer = nullptr;
    std::unique_ptr<ClipmapRenderer> g_GridRenderer = nullptr;
    std::shared_ptr<PassConstants> g_Constants = nullptr;
    std::shared_ptr<DirectX::ConstantBuffer<PassConstants>> g_Cb0 = nullptr;
    std::unique_ptr<Camera> g_Camera = nullptr;
    std::unique_ptr<TerrainSystem> g_System = nullptr;

    std::unique_ptr<DebugRenderer> g_DebugRenderer = nullptr;

    constexpr Vector2 ViewInit2 = Vector2(4096.0f, 4096.0f);
    constexpr Vector3 ViewInit3 = Vector3(ViewInit2.x, 2000.0f, ViewInit2.y);
}

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void CreateSystem();

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = {
        sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"ImGui Example", NULL
    };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Renderer", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL,
        NULL, wc.hInstance, NULL);

    // CreateSystem Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsClassic();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    // Load Fonts

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    CreateSystem();

    bool wireFrame = false;
    float lPhi = DirectX::XM_PI + DirectX::XM_PIDIV2;
    float lTheta = 0.6f;
    float lInt = 1.0f;
    bool freezeFrustum = false;
    bool drawBb = false;
    bool drawClip = false;
    DirectX::BoundingFrustum frustum {};
    Vector3 view(ViewInit3);
    float spd = 30.0f;
    float hScale = 2600.0f;
    float transition = 25.4f;

    bool done = false;
    // Main loop
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        done = io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_Escape)];

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        g_Camera->Update(io, spd);

        // Vector3 dView;
        if (!freezeFrustum)
        {
            frustum = g_Camera->GetFrustum();
            // dView = g_Camera->GetPosition() - view;
            view = g_Camera->GetPosition();
        }
        const auto& cmr = g_System->GetClipmapResources(frustum, hScale, g_pd3dDeviceContext);

        std::vector<DirectX::BoundingBox> bbs;
        //const auto& pr = g_System->GetPatchResources(
        // camCullingXy, frustumLocal, yScale, bounding, g_pd3dDevice);

        Vector3 lDir(
            std::sin(lTheta) * std::cos(lPhi),
            std::cos(lTheta),
            std::sin(lTheta) * std::sin(lPhi));
        lDir.Normalize();

        g_Constants->ViewProjection = g_Camera->GetViewProjection().Transpose();
        g_Constants->ViewPosition = view;
        g_Constants->Light = Vector4(lDir.x, lDir.y, lDir.z, lInt);
        g_Constants->HeightScale = hScale;
        g_Constants->AlphaOffset = Vector2(126.0f - transition);
        g_Constants->OneOverTransition = 1.0f / transition;

        ImGui::Begin("Terrain System");
        ImGui::Text("Frame Rate : %f", io.Framerate);
        ImGui::DragFloat("Height Scale", &hScale, 1, 0.0, 10000.0);
        //ImGui::Text("Visible Patch : %d", pr.Patches.size());
        ImGui::SliderFloat("Sun Theta", &lTheta, 0.0f, DirectX::XM_PIDIV2);
        ImGui::SliderFloat("Sun Phi", &lPhi, 0.0, DirectX::XM_2PI);
        ImGui::SliderFloat("Sun Intensity", &lInt, 0.0f, 1.0);
        ImGui::DragFloat("Camera Speed", &spd, 1.0, 0.0, 5000.0);
        ImGui::SliderFloat("Transition Width", &transition, 0.1, 26.0);
        ImGui::Checkbox("Wire Frame", &wireFrame);
        ImGui::Checkbox("Freeze Frustum", &freezeFrustum);
        ImGui::Checkbox("Draw Bounding Box", &drawBb);
        ImGui::Checkbox("Show Clipmap Texture", &drawClip);
        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = {
            clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w
        };

        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, g_depthStencil->GetDsv());
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        g_pd3dDeviceContext->ClearDepthStencilView(g_depthStencil->GetDsv(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        g_Camera->SetViewPort(g_pd3dDeviceContext);
        g_Cb0->SetData(g_pd3dDeviceContext, *g_Constants);
        // if (drawBb)
        //     g_DebugRenderer->DrawBounding(bbs, g_Camera->GetView(), g_Camera->GetProjection());

        //g_MeshRenderer->Render(g_pd3dDeviceContext, pr, wireFramed);
        g_GridRenderer->Render(g_pd3dDeviceContext, cmr);
        if (wireFrame) g_GridRenderer->Render(g_pd3dDeviceContext, cmr, wireFrame);

        if (drawClip)
        {
            g_DebugRenderer->DrawClippedHeight(Vector2::Zero, g_System->GetHeightClip(), g_pd3dDeviceContext);
            g_DebugRenderer->DrawClippedAlbedo(Vector2(0, 260), g_System->GetAlbedoClip(), g_pd3dDeviceContext);
            g_DebugRenderer->DrawClippedAlbedo(Vector2(0, 520), g_System->GetNormalClip(), g_pd3dDeviceContext);
        }

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // g_pSwapChain->Present(1, 0); // Present with vsync
        g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;

    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
        featureLevelArray, _countof(featureLevelArray), D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice,
        &featureLevel,
        &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, createDeviceFlags, featureLevelArray, 2,
            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain)
    {
        g_pSwapChain->Release();
        g_pSwapChain = NULL;
    }
    if (g_pd3dDeviceContext)
    {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = NULL;
    }
    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
    }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);

    D3D11_TEXTURE2D_DESC texDesc;
    pBackBuffer->GetDesc(&texDesc);
    CD3D11_TEXTURE2D_DESC dsDesc(DXGI_FORMAT_D24_UNORM_S8_UINT,
        texDesc.Width, texDesc.Height, 1, 0, D3D11_BIND_DEPTH_STENCIL,
        D3D11_USAGE_DEFAULT);
    g_depthStencil = std::make_unique<DirectX::Texture2D>(g_pd3dDevice, dsDesc);
    g_depthStencil->CreateViews(g_pd3dDevice);

    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = NULL;
    }
}

void CreateSystem()
{
    g_Constants = std::make_unique<PassConstants>();
    g_Cb0 = std::make_unique<DirectX::ConstantBuffer<PassConstants>>(g_pd3dDevice);

    g_MeshRenderer = std::make_unique<TINRenderer>(g_pd3dDevice, g_Cb0);
    g_MeshRenderer->Initialize(g_pd3dDeviceContext, "shader");

    g_GridRenderer = std::make_unique<ClipmapRenderer>(g_pd3dDevice, g_Cb0);
    g_GridRenderer->Initialize("shader");

    g_Camera = std::make_unique<Camera>(ViewInit3);

    g_System = std::make_unique<TerrainSystem>(ViewInit2, "asset", g_pd3dDevice);

    g_DebugRenderer = std::make_unique<DebugRenderer>(g_pd3dDeviceContext, g_pd3dDevice);
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE: if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND: if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY: ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
