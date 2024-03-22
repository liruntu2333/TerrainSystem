// Dear ImGui: standalone example application for DirectX 11
// If you are new to Dear ImGui, read documentation from the docs/ dir + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#define NOMINMAX

#include <chrono>
#include <random>

#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "Camera.h"

#include "../HeightMapSplitter/ThreadPool.h"

#include "DebugRenderer.h"
#include "PlanetRenderer.h"

// Data
static ID3D11Device* g_pd3dDevice                     = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext       = NULL;
static IDXGISwapChain* g_pSwapChain                   = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

using namespace DirectX::SimpleMath;

ThreadPool g_ThreadPool(std::thread::hardware_concurrency());

namespace
{
    std::unique_ptr<DirectX::Texture2D> g_depthStencil = nullptr;

    std::shared_ptr<PassConstants> g_Constants                    = nullptr;
    std::shared_ptr<DirectX::ConstantBuffer<PassConstants>> g_Cb0 = nullptr;
    std::unique_ptr<Camera> g_Camera                              = nullptr;

    std::unique_ptr<DebugRenderer> g_DebugRenderer   = nullptr;
    std::unique_ptr<PlanetRenderer> g_PlanetRenderer = nullptr;

    constexpr Vector3 ViewInit = Vector3(-PlanetRenderer::kRadius * 0.75, 0.0f, (PlanetRenderer::kRadius + PlanetRenderer::kElevation) * 3.0f);
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
    RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Renderer", WS_OVERLAPPEDWINDOW, 100, 100, 1440, 900, NULL,
        NULL, wc.hInstance, NULL);

    // CreateSystem Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsClassic();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    // Load Fonts

    // Our state
    auto& darkSlateGray = DirectX::Colors::Black;
    ImVec4 clear_color  = ImVec4(darkSlateGray.f[0], darkSlateGray.f[1],
        darkSlateGray.f[2], darkSlateGray.f[3]);

    CreateSystem();

    bool wireFrame     = false;
    bool freezeFrustum = false;
    DirectX::BoundingFrustum frustum;
    float yaw  = 0.0;
    float spd  = PlanetRenderer::kRadius * 0.25f;
    bool done  = false, debug = false, rotate = false;
    float time = 0.0f;
    PlanetRenderer::Uniforms uniforms {};
    float roll        = -23.4f * DirectX::XM_PI / 180.0f;
    Matrix tilt       = Matrix::CreateRotationZ(roll);
    Vector3 earthAxis = (Vector3(cos(roll), sin(roll), 0).Cross(Vector3::UnitZ));
    earthAxis.Normalize();

    // seeding
    std::random_device randomDevice;
    std::default_random_engine randomEngine(randomDevice());

    auto rndVec4 = [&randomEngine]() -> Vector4
    {
        std::uniform_real_distribution distribution(-1.0, 1.0);
        return Vector4(
            distribution(randomEngine),
            distribution(randomEngine),
            distribution(randomEngine),
            distribution(randomEngine));
    };

    // "Uniform Random Rotations", Ken Shoemake, Graphics Gems III, pg. 124-132.
    auto rndQ = [&randomEngine]() -> Quaternion
    {
        std::uniform_real_distribution dist(0.0, 1.0);
        const double u1           = dist(randomEngine);
        const double u2           = dist(randomEngine);
        const double u3           = dist(randomEngine);
        const double sqrt1MinusU1 = std::sqrt(1 - u1);
        const double sqrtU1       = std::sqrt(u1);

        constexpr double pi = 3.14159265358979323846;
        Quaternion q;
        q.w = static_cast<float>(sqrt1MinusU1 * std::sin(2.0 * pi * u2));
        q.x = static_cast<float>(sqrt1MinusU1 * std::cos(2.0 * pi * u2));
        q.y = static_cast<float>(sqrtU1 * std::sin(2.0 * pi * u3));
        q.z = static_cast<float>(sqrtU1 * std::cos(2.0 * pi * u3));
        q.Normalize();
        return q;
    };
    g_PlanetRenderer->CreateWorldMap(g_pd3dDeviceContext, uniforms);

    // Main loop
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
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

        bool planetChanged = false;
        ImGui::Begin("Planet System");
        ImGui::Text("Frame Rate : %f", io.Framerate);

        const bool randAll = ImGui::Button("Rand");
        if (ImGui::Button("Rand Feature") || randAll)
        {
            uniforms.featureNoiseSeed     = rndVec4();
            uniforms.featureNoiseRotation = Matrix::CreateFromQuaternion(rndQ());
            planetChanged                 = true;
        }
        if (ImGui::Button("Rand Sharpness") || randAll)
        {
            uniforms.sharpnessNoiseSeed     = rndVec4();
            uniforms.sharpnessNoiseRotation = Matrix::CreateFromQuaternion(rndQ());
            planetChanged                   = true;
        }
        if (ImGui::Button("Rand Slope Erosion") || randAll)
        {
            uniforms.slopeErosionNoiseSeed     = rndVec4();
            uniforms.slopeErosionNoiseRotation = Matrix::CreateFromQuaternion(rndQ());
            planetChanged                      = true;
        }
        if (ImGui::Button("Rand Perturb") || randAll)
        {
            uniforms.perturbNoiseSeed     = rndVec4();
            uniforms.perturbNoiseRotation = Matrix::CreateFromQuaternion(rndQ());
            planetChanged                 = true;
        }

        // ImGui::Checkbox("interpolateNormal", reinterpret_cast<bool*>(&uniforms.interpolateNormal));
        // if (!uniforms.interpolateNormal)
        // {
        //     ImGui::SliderInt(UNIFORM(normalOctaves), 0, 16);
        // }
        ImGui::Text("Uber Noise");
#define UNIFORM(x) #x, &uniforms.x
        planetChanged |= ImGui::DragFloat(UNIFORM(baseFrequency), 0.001f, 0.01f, 4.0f);
        planetChanged |= ImGui::SliderFloat(UNIFORM(baseAmplitude), 0.0f, 2.0f);
        planetChanged |= ImGui::SliderFloat(UNIFORM(lacunarity), 1.01f, 4.0f);
        planetChanged |= ImGui::SliderFloat(UNIFORM(gain), 0.5f, 0.70710678118654752440084436210485f);

        planetChanged |= ImGui::DragFloatRange2("sharpness", &uniforms.sharpness[0], &uniforms.sharpness[1],
            0.01f, -1.0f, 1.0f);
        planetChanged |= ImGui::SliderFloat(UNIFORM(sharpnessBaseFrequency), 0.01f, 4.0f);
        planetChanged |= ImGui::SliderFloat(UNIFORM(sharpnessLacunarity), 1.01f, 4.0f);

        planetChanged |= ImGui::DragFloatRange2("slopeErosion", &uniforms.slopeErosion[0], &uniforms.slopeErosion[1],
            0.01f, 0.0f, 1.0f);
        planetChanged |= ImGui::SliderFloat(UNIFORM(slopeErosionBaseFrequency), 0.01f, 4.0f);
        planetChanged |= ImGui::SliderFloat(UNIFORM(slopeErosionLacunarity), 1.01f, 4.0f);

        // planetChanged |= ImGui::DragFloatRange2("perturb", &uniforms.perturb[0], &uniforms.perturb[1],
        //     0.001f, -1.0f, 1.0f);
        // planetChanged |= ImGui::SliderFloat(UNIFORM(perturbBaseFrequency), 0.01f, 4.0f);
        // planetChanged |= ImGui::SliderFloat(UNIFORM(perturbLacunarity), 1.01f, 4.0f);
        // planetChanged |= ImGui::SliderFloat(UNIFORM(altitudeErosion), 0.0f, 1.0f);
        // planetChanged |= ImGui::SliderFloat(UNIFORM(ridgeErosion), -1.0f, 1.0f);
        const auto windowSize = ImGui::GetWindowSize();
        ImGui::Image(g_PlanetRenderer->GetWorldMapSrv(), ImVec2(windowSize.x, windowSize.x * 0.5f));
        ImGui::End();

        ImGui::Begin("Planet Geometry");
        // ImGui::SliderInt(UNIFORM(geometryOctaves), 0, 16);
        ImGui::DragFloat(UNIFORM(radius), PlanetRenderer::kRadius * 0.0001f);
        ImGui::DragFloat(UNIFORM(elevation), PlanetRenderer::kElevation * 0.001f, 0.0, PlanetRenderer::kRadius * 0.5f);
        planetChanged |= ImGui::SliderFloat(UNIFORM(oceanLevel), -0.1f, 2.0f);
        ImGui::Checkbox("Rotate", &rotate);
        ImGui::End();
#undef UNIFORM
        ImGui::Begin("Camera");
        ImGui::DragFloat("Speed", &spd, PlanetRenderer::kRadius * 0.0001f);
        ImGui::Checkbox("Wire Frame", &wireFrame);
        ImGui::Checkbox("Freeze Frustum", &freezeFrustum);

        // ImGui::Checkbox("Debug", &debug);
        ImGui::End();
        // Updating
        std::vector<DirectX::BoundingBox> bbs;
        g_Camera->Update(io, spd);
        if (!freezeFrustum)
        {
            frustum = g_Camera->GetFrustum();
        }
        if (io.MouseDown[ImGuiMouseButton_Left] && !io.WantCaptureMouse)
        {
            yaw -= io.MouseDelta.x * 0.01f;
        }
        if (rotate)
        {
            yaw -= io.DeltaTime * 0.05f;
        }
        yaw = std::fmod(yaw, -DirectX::XM_2PI);

        Matrix world = tilt * Matrix::CreateFromAxisAngle(earthAxis, yaw);

        uniforms.worldInvTrans = world.Invert().Transpose().Transpose();
        uniforms.world         = world.Transpose();
        uniforms.worldViewProj = (world * g_Camera->GetViewProjection()).Transpose();
        uniforms.viewProj      = g_Camera->GetViewProjection().Transpose();
        uniforms.camPos        = g_Camera->GetPosition();

        time += io.DeltaTime;

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = {
            clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w
        };

        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, g_depthStencil->GetDsv());
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        g_pd3dDeviceContext->ClearDepthStencilView(g_depthStencil->GetDsv(), D3D11_CLEAR_DEPTH, 0.0f, 0);
        g_Camera->SetViewPort(g_pd3dDeviceContext);
        //g_Cb0->SetData(g_pd3dDeviceContext, *g_Constants);
        // g_DebugRenderer->DrawSphere(Matrix::CreateScale(1) * Matrix::CreateTranslation(g_Camera->GetPosition() + g_Camera->GetForward() * 5.0f),
        //     g_Camera->GetView(), g_Camera->GetProjection());

        ID3D11ShaderResourceView* srv = nullptr;
        g_pd3dDeviceContext->PSSetShaderResources(0, 1, &srv);
        if (planetChanged)
        {
            g_PlanetRenderer->CreateWorldMap(g_pd3dDeviceContext, uniforms);
        }

        g_PlanetRenderer->Render(g_pd3dDeviceContext, uniforms, frustum, Quaternion::CreateFromRotationMatrix(world),
            Vector3::Zero, wireFrame, freezeFrustum);

        // g_DebugRenderer->DrawBounding(bbs, g_Camera->GetView(), g_Camera->GetProjection());

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // g_pSwapChain->Present(1, 0); // Present with vsync
        g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hWnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;

    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, };
    HRESULT res                                 = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
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
    CD3D11_TEXTURE2D_DESC dsDesc(DXGI_FORMAT_D32_FLOAT,
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
    g_Cb0       = std::make_unique<DirectX::ConstantBuffer<PassConstants>>(g_pd3dDevice);

    g_Camera = std::make_unique<Camera>(ViewInit);

    g_DebugRenderer = std::make_unique<DebugRenderer>(g_pd3dDeviceContext, g_pd3dDevice);

    g_PlanetRenderer = std::make_unique<PlanetRenderer>(g_pd3dDevice);
    g_PlanetRenderer->Initialize("./shader");
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
