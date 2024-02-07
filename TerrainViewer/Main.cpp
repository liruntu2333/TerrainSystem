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
#include <d3d11_3.h>

#include "AssetImporter.h"
#include "DebugRenderer.h"
#include "GrassRenderer.h"
#include "ModelRenderer.h"
#include "StructuredBuffer.h"

// Data
static ID3D11Device* g_pd3dDevice                     = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext       = NULL;
static IDXGISwapChain* g_pSwapChain                   = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

using namespace DirectX::SimpleMath;

ThreadPool g_ThreadPool(std::thread::hardware_concurrency());

namespace
{
    std::unique_ptr<DirectX::Texture2D> g_depthStencil            = nullptr;
    std::unique_ptr<DirectX::Texture2D> g_depthLookup             = nullptr;
    std::shared_ptr<PassConstants> g_Constants                    = nullptr;
    std::shared_ptr<DirectX::ConstantBuffer<PassConstants>> g_Cb0 = nullptr;
    std::unique_ptr<Camera> g_Camera                              = nullptr;

    std::unique_ptr<DirectX::StructuredBuffer<DirectX::VertexPositionNormalTexture>> g_BaseVertices = nullptr;
    std::unique_ptr<DirectX::StructuredBuffer<uint32_t>> g_BaseIndices                              = nullptr;
    DirectX::BoundingBox g_Bound;
    float g_BaseArea                               = 0.0f;
    std::unique_ptr<DirectX::Texture2D> g_Albedo   = nullptr;
    std::unique_ptr<ModelRenderer> g_ModelRenderer = nullptr;

    std::unique_ptr<GrassRenderer> g_GrassRenderer    = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer> g_GrassVb    = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Buffer> g_GrassIb    = nullptr;
    std::unique_ptr<DirectX::Texture2D> g_GrassAlbedo = nullptr;
    uint32_t g_GrassIdxCnt                            = 0;

    std::unique_ptr<DebugRenderer> g_DebugRenderer = nullptr;

    constexpr Vector3 ViewInit = Vector3(0.0f, 0.0f, 150.0f);
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
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    CreateSystem();

    bool wireFrame     = false;
    bool freezeFrustum = false;
    DirectX::BoundingFrustum frustum;
    Matrix vpForCull;
    float spd     = 100.0f;
    float density = 40.0f;
    Vector2 height(1.4f, 2.5f);
    Vector2 width(0.1f, 0.14f);
    Vector2 bend(0.5f, 0.9f);
    Vector4 gravity(0, -1, 0, 1);
    Vector3 windDir;
    (Vector3::Forward + Vector3::Right + Vector3::Up).Normalize(windDir);
    float windStrength    = 2.0;
    float windPeriod      = 2.0;
    float orientThreshold = 0.98f;
    bool orientCulling    = true, frustumCulling = true, distCulling = true, occlusionCulling = true;
    float lod0Dist        = 50.0f;
    bool showDebug        = false;
    bool done             = false;
    float time            = 0.0f;
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

        bool modeChanged = false;

        ImGui::Begin("Grass System");
        ImGui::Text("Frame Rate : %f", io.Framerate);
        //ImGui::Text("Visible Patch : %d", pr.Patches.size());
        //ImGui::SliderFloat("Sun Theta", &lTheta, 0.0f, DirectX::XM_PIDIV2);
        //ImGui::SliderFloat("Sun Phi", &lPhi, 0.0, DirectX::XM_2PI);
        //ImGui::SliderFloat("Sun Intensity", &lInt, 0.0, 5.0);
        //ImGui::SliderFloat("Ambient Intensity", &ambient, 0.0, 1.0);
        ImGui::DragFloat("Camera Speed", &spd, 1.0, 0.0, 5000.0);
        ImGui::SliderFloat("Grass Density", &density, 0, 70);
        ImGui::DragFloatRange2("Height", &height.x, &height.y, 0.01, 0.01, 10.0, "%.5f", nullptr);
        ImGui::DragFloatRange2("Width", &width.x, &width.y, 0.001, 0.01, 1.0, "%.5f", nullptr);
        ImGui::DragFloatRange2("Bend", &bend.x, &bend.y, 0.001, 0.001, 5.0, "%.5f", nullptr);
        ImGui::SliderFloat("Gravity", &gravity.w, 0.0, 10.0, "%.3f");
        ImGui::SliderFloat("Wind Strength", &windStrength, 0.0, 10.0, "%.3f");
        ImGui::SliderFloat("Wind Period", &windPeriod, 0.0, 10.0, "%.3f");
        ImGui::SliderFloat("LOD Dist", &lod0Dist, 0.0, 1000.0, "%.3f");
        ImGui::Checkbox("Show Debug", &showDebug);
        ImGui::Checkbox("Distance Culling", &distCulling);
        ImGui::Checkbox("Orient Culling", &orientCulling);
        ImGui::SliderFloat("Orient Threshold", &orientThreshold, 0.9, 1.0, "%.3f");
        ImGui::Checkbox("Frustum Culling", &frustumCulling);
        ImGui::Checkbox("Occlusion Culling", &occlusionCulling);
        ImGui::Checkbox("Wire Frame", &wireFrame);
        ImGui::Checkbox("Freeze Frustum", &freezeFrustum);
        ImGui::End();

        auto statueRotTrans = Matrix::CreateTranslation(0, -50, 0);
        auto statueWorld    = statueRotTrans;

        // Updating
        std::vector<DirectX::BoundingBox> bbs;
        DirectX::BoundingBox bbWorld;
        g_Bound.Transform(bbWorld, statueWorld);
        bbs.emplace_back(bbWorld);
        g_Camera->Update(io, spd);
        if (!freezeFrustum)
        {
            frustum   = g_Camera->GetFrustum();
            vpForCull = g_Camera->GetViewProjection();
        }
        std::array<Vector4, 6> planes;
        DirectX::XMVECTOR vPlanes[6];
        frustum.GetPlanes(&vPlanes[0], &vPlanes[1], &vPlanes[2], &vPlanes[3], &vPlanes[4], &vPlanes[5]);
        for (int i = 0; i < 6; ++i)
        {
            XMStoreFloat4(&planes[i], vPlanes[i]);
        }

        time += io.DeltaTime;
        float windWave = windPeriod * time;
        const Vector4 wind(windDir.x, windDir.y, windDir.z, windStrength);

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = {
            clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w
        };

        auto dsv = showDebug ? g_depthLookup->GetDsv() : g_depthStencil->GetDsv();
        auto rtv = showDebug ?  nullptr : g_mainRenderTargetView;
        g_pd3dDeviceContext->OMSetRenderTargets(1, &rtv, dsv);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        g_pd3dDeviceContext->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
        g_Camera->SetViewPort(g_pd3dDeviceContext);
        //g_Cb0->SetData(g_pd3dDeviceContext, *g_Constants);

        g_ModelRenderer->Render(g_pd3dDeviceContext, statueWorld, g_Camera->GetViewProjection(), *g_BaseVertices, *g_BaseIndices,
            *g_Albedo, g_Camera->GetPosition(), wireFrame);
        g_DebugRenderer->DrawSphere(Matrix::CreateScale(10) * Matrix::CreateTranslation(0, 0, 20), g_Camera->GetView(), g_Camera->GetProjection());


        if (showDebug)
        {
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, g_depthStencil->GetDsv());
            g_pd3dDeviceContext->ClearDepthStencilView(g_depthStencil->GetDsv(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        }

        if (occlusionCulling && !showDebug)
        {
            g_pd3dDeviceContext->CopyResource(g_depthLookup->GetTexture(), g_depthStencil->GetTexture());
        }

        GrassRenderer::Uniforms uniforms;
        uniforms.ViewProj         = g_Camera->GetViewProjection().Transpose();
        uniforms.BaseWorld        = statueWorld.Transpose();
        uniforms.VPForCull        = vpForCull.Transpose();
        uniforms.NumBaseTriangle  = g_BaseIndices->m_Capacity / 3;
        uniforms.Density          = density;
        uniforms.Height           = height;
        uniforms.Width            = width;
        uniforms.BendFactor       = bend;
        uniforms.Gravity          = gravity;
        uniforms.Wind             = wind;
        uniforms.WindWave         = windWave;
        uniforms.DistCulling      = distCulling;
        uniforms.OrientCulling    = orientCulling;
        uniforms.FrustumCulling   = frustumCulling;
        uniforms.OcclusionCulling = occlusionCulling;
        uniforms.NearFar          = Vector2(g_Camera->GetNear(), g_Camera->GetFar());
        uniforms.CamPos           = g_Camera->GetPosition();
        uniforms.OrientThreshold  = orientThreshold;
        uniforms.Lod0Dist         = lod0Dist;
        uniforms.Debug            = showDebug;
        //std::copy_n(planes.begin(), 6, uniforms.Planes);
        g_GrassRenderer->Render(g_pd3dDeviceContext, *g_BaseVertices, *g_BaseIndices, g_GrassAlbedo->GetSrv(),
            g_depthLookup->GetSrv(), uniforms, wireFrame);

        g_DebugRenderer->DrawBounding(bbs, g_Camera->GetView(), g_Camera->GetProjection());

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        //g_pSwapChain->Present(1, 0); // Present with vsync
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
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
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

    dsDesc.Format    = DXGI_FORMAT_R32_TYPELESS;
    dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
    g_depthLookup    = std::make_unique<DirectX::Texture2D>(g_pd3dDevice, dsDesc);
    g_depthLookup->CreateViews(g_pd3dDevice);

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

    {
        //auto [meshes, mats, bounding, areaSum] = AssetImporter::LoadAsset(R"(asset\model\Rock1\Rock1.obj)");
        auto [meshes, mats, bounding, areaSum] = AssetImporter::LoadAsset(R"(asset\model\BingMaYong\12341_Statue_v2_l1.obj)", true);
        std::vector<ModelRenderer::Vertex> vertices;
        vertices.reserve(meshes[0].Vertices.size());
        std::transform(meshes[0].Vertices.begin(), meshes[0].Vertices.end(), std::back_inserter(vertices),
            [](const VertexPositionNormalTangentTexture& v) { return ModelRenderer::Vertex(v.Pos * 2, v.Nor, v.Tc); });
        bounding.Transform(g_Bound, Matrix::CreateScale(2));
        g_BaseArea = areaSum;

        CD3D11_BUFFER_DESC desc(0, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_IMMUTABLE,
            0, D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, sizeof(ModelRenderer::Vertex));
        g_BaseVertices = std::make_unique<DirectX::StructuredBuffer<DirectX::VertexPositionNormalTexture>>(g_pd3dDevice,
            vertices.data(), vertices.size(), desc);
        desc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
        desc.StructureByteStride = sizeof(uint32_t);
        g_BaseIndices            = std::make_unique<DirectX::StructuredBuffer<uint32_t>>(g_pd3dDevice, meshes[0].Indices.data(),
            meshes[0].Indices.size(), desc);
        g_Albedo = std::make_unique<DirectX::Texture2D>(g_pd3dDevice, mats[meshes[0].MaterialIndex].TexturePath);
    }

    g_ModelRenderer = std::make_unique<ModelRenderer>(g_pd3dDevice);
    g_ModelRenderer->Initialize(R"(shader)");

    g_GrassRenderer = std::make_unique<GrassRenderer>(g_pd3dDevice);
    g_GrassRenderer->Initialize(R"(shader)");

    {
        auto [meshes, mats, bounding, areaSum] = AssetImporter::LoadAsset(R"(asset\model\DeadTree\DeadTree.obj)");
        std::vector<ModelRenderer::Vertex> vertices;
        vertices.reserve(meshes[0].Vertices.size());
        g_GrassIdxCnt = meshes[0].Indices.size();
        std::transform(meshes[0].Vertices.begin(), meshes[0].Vertices.end(), std::back_inserter(vertices),
            [](const VertexPositionNormalTangentTexture& v) { return ModelRenderer::Vertex(v.Pos, v.Nor, v.Tc); });
        DirectX::CreateStaticBuffer(g_pd3dDevice, vertices, D3D11_BIND_VERTEX_BUFFER, &g_GrassVb);
        DirectX::CreateStaticBuffer(g_pd3dDevice, meshes[0].Indices, D3D11_BIND_INDEX_BUFFER, &g_GrassIb);
        g_GrassAlbedo = std::make_unique<DirectX::Texture2D>(g_pd3dDevice, R"(asset\model\DeadTree\grass.png)");
    }
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
