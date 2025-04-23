// Dear ImGui: standalone example application for DirectX 11
// If you are new to Dear ImGui, read documentation from the docs/ dir + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#define NOMINMAX

#include <chrono>
#include <corecrt_math_defines.h>
#include <string>
#include <directxtk/WICTextureLoader.h>

#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "Camera.h"

#include "../HeightMapSplitter/ThreadPool.h"

#include "DebugRenderer.h"
#include "CompressedTerrainRenderer.h"
#include "DirectXTex.h"
#include "WaveletTransform.h"
#include "FiniteStateEntropy/huf.h"
#include "FiniteStateEntropy/fse.h"
#include "FiniteStateEntropy/fseU16.h"

#include "zlib.h"

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

    std::unique_ptr<Camera> g_Camera                = nullptr;
    std::unique_ptr<DebugRenderer> g_DebugRenderer  = nullptr;
    std::unique_ptr<CompressedTerrainRenderer> g_TR = nullptr;
    DirectX::ScratchImage g_OriginEle;
    std::unique_ptr<DirectX::Texture2D> g_fOriginEleTex    = nullptr;
    std::unique_ptr<DirectX::Texture2D> g_OriginEleTex     = nullptr;
    std::unique_ptr<DirectX::Texture2D> g_CompressedEleTex = nullptr;
    DirectX::ScratchImage g_Coefficients;
    DirectX::ScratchImage g_Quantized;
    std::unique_ptr<DirectX::Texture2D> g_CoefficientsTex = nullptr;
    size_t g_OriginAvg                                    = 0;

    constexpr Vector3 ViewInit = Vector3(0, 80, 500.0f);
    constexpr int IteInit      = 6;
    constexpr int ThresInit    = 0;
}

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void CreateSystem();
void ProcessChinaMap();

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
    auto& darkSlateGray = DirectX::Colors::DarkGray;
    ImVec4 clear_color  = ImVec4(darkSlateGray.f[0], darkSlateGray.f[1],
        darkSlateGray.f[2], darkSlateGray.f[3]);

    CreateSystem();

    bool wireFrame     = false;
    bool freezeFrustum = false;
    DirectX::BoundingFrustum frustum;
    float spd      = 20.0f;
    bool done      = false, debug = false, renderBound = false, sphereReference = false, showOrigin = false, floatOri = false;
    float time     = 0.0f;
    float ratio    = 50.0f;
    int iteration  = IteInit;
    int threshold  = ThresInit;
    float error    = 4.0f;
    float bitRate  = 4.0f;
    int map        = 0;
    bool showError = true;
    // float relative

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

        ImGui::Begin("Origin");
        static const char* maps[] =
        {
            "guanzhong",
            "beijing",
            "shanxi",
            "sichuan",
            "shanghai",
            "xinjiang",
            "tianjin",
            "mountains512",
        };
        bool reload = ImGui::Combo("Map", &map, maps, std::size(maps));
        if (ImGui::RadioButton("Render Origin", showOrigin)) { showOrigin = !showOrigin; }
        ImGui::Checkbox("Float Origin", &floatOri);
        ImGui::Image(g_OriginEleTex->GetSrv(), ImVec2(512, 512));
        ImGui::End();

        ImGui::Begin("Wavelet Coefficients");
        ImGui::Image(g_CoefficientsTex->GetSrv(), ImVec2(512, 512));
        ImGui::End();

        ImGui::Begin("Compressed");
        bool transform = ImGui::SliderInt("Transform Iteration", &iteration, 0, 8);
        bool quantize  = ImGui::SliderInt("Threshold", &threshold, 0, 64);
        bool compress  = false;
        ImGui::Checkbox(showError ? "##Show Error" : "Show Error", &showError);
        if (showError)
        {
            ImGui::SameLine();
            error = std::clamp(error, 1e-6f, 20.0f);
            ImGui::SliderFloat("Error", &error, 1e-6f, 20.0f);
        }
        else
        {
            error = std::numeric_limits<float>::infinity();
        }
        ImGui::Text("Bit Rate : %f", bitRate);
        ImGui::Text("Compression Ratio : %f", sizeof(uint8_t) * 8.0f / bitRate);
        ImGui::Image(g_CompressedEleTex->GetSrv(), ImVec2(512, 512));
        ImGui::End();

        if (reload)
        {
            std::wstring path(L"./asset/");
            std::string str(maps[map]);
            path += std::wstring(str.begin(), str.end());
            path += L".png";
            LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, g_OriginEle);

            auto* image   = &g_OriginEle.GetImages()[0];
            size_t width  = image->width;
            size_t height = image->height;
            size_t size   = width * height;
            std::vector<uint8_t> ele(size);
            std::memcpy(ele.data(), image->pixels, size);
            std::nth_element(ele.begin(), ele.begin() + size / 2, ele.end());

            g_OriginAvg = ele[ele.size() / 2];

            g_pd3dDeviceContext->UpdateSubresource(g_OriginEleTex->GetTexture(), 0, nullptr,
                image->pixels, g_OriginEle.GetMetadata().width * sizeof(uint8_t), 0);

            transform = true;
        }

        if (transform)
        {
            WaveletTransform::LeGall53<uint8_t>(g_OriginEle.GetImages()[0], g_Coefficients, iteration, g_OriginAvg);
            quantize = true;
        }

        if (quantize)
        {
            auto coefficients = g_Coefficients.GetImages()[0];
            auto qPix         = reinterpret_cast<int16_t*>(g_Quantized.GetImages()[0].pixels);
            WaveletTransform::FilterOut<int16_t>(reinterpret_cast<int16_t*>(coefficients.pixels),
                qPix, coefficients.width * coefficients.height, threshold);

            auto size = g_OriginEle.GetMetadata().width * g_OriginEle.GetMetadata().height;
            std::vector<uint8_t> uCoff;
            uCoff.reserve(size);
            auto [pMin, pMax] = std::minmax_element(qPix, qPix + size);
            int16_t m         = *pMin, M = *pMax;
            // assert(M - m <= 255);
            std::transform(qPix, qPix + size, std::back_inserter(uCoff),
                [m](int16_t v) { return WaveletTransform::SaturatedCast<int16_t, uint8_t>(v - m); });

            g_pd3dDeviceContext->UpdateSubresource(g_CoefficientsTex->GetTexture(), 0, nullptr,
                uCoff.data(), g_OriginEle.GetMetadata().width * sizeof(int8_t), 0);

            DirectX::ScratchImage reconstructed;
            WaveletTransform::InvLeGall53<int16_t>(g_Quantized.GetImages()[0],
                reconstructed, iteration, static_cast<int16_t>(g_OriginAvg));
            g_pd3dDeviceContext->UpdateSubresource(g_CompressedEleTex->GetTexture(), 0, nullptr,
                reconstructed.GetImages()[0].pixels, reconstructed.GetImages()[0].rowPitch, 0);

            compress = true;
        }

        if (compress)
        {
            size_t width  = g_Quantized.GetMetadata().width;
            size_t height = g_Quantized.GetMetadata().height;
            size_t size   = width * height;

            auto qPix = reinterpret_cast<int16_t*>(g_Quantized.GetImages()[0].pixels);
            // auto subbands    = WaveletTransform::InterleaveSubBand(qPix, width, height, iteration);
            auto subbands    = WaveletTransform::InterleaveSubBand(qPix, width, height, iteration);
            size_t totalSize = 0;

            std::string file;
            file = maps[map];
            file += "_coefficients.bin";
            std::ofstream out(file, std::ios::binary | std::ios::out);
            for (auto& subband : subbands)
            {
                size_t srcSize      = subband.size();
                size_t dstSize      = FSE_compressBound(srcSize);
                auto* const encoded = std::malloc(dstSize);
                dstSize             = FSE_compressU16(encoded, dstSize, reinterpret_cast<const unsigned short*>(subband.data()), srcSize, 511, 0);
                if (FSE_isError(dstSize))
                {
                    auto err = FSE_getErrorName(dstSize);
                    std::printf("Compression error : %s\n", err);
                }
                totalSize += dstSize;

                std::vector<int16_t> decompressed(srcSize);

                auto srcSize2 = FSE_decompressU16(reinterpret_cast<unsigned short*>(decompressed.data()), srcSize, encoded, dstSize);
                if (FSE_isError(srcSize2))
                {
                    auto err = FSE_getErrorName(srcSize2);
                    std::printf("Compression error : %s\n", err);
                }
                assert(srcSize == srcSize2);
                assert(subband == decompressed);

                out.write(static_cast<const char*>(encoded), subband.size());
                std::free(encoded);
            }
            out.close();
            bitRate = 8.0 * static_cast<double>(totalSize) / size;
        }

        // Updating
        g_Camera->Update(io, spd);
        if (!freezeFrustum)
        {
            frustum = g_Camera->GetFrustum();
        }
        ImGui::Begin("Camera");
        ImGui::Text("Position : %f %f %f", g_Camera->GetPosition().x, g_Camera->GetPosition().y, g_Camera->GetPosition().z);
        ImGui::Text("Forward : %f %f %f", g_Camera->GetForward().x, g_Camera->GetForward().y, g_Camera->GetForward().z);
        ImGui::DragFloat("Speed", &spd);
        ImGui::SliderFloat("Ratio", &ratio, 0, 150);
        ImGui::Checkbox("Wire Frame", &wireFrame);
        ImGui::Checkbox("Freeze Frustum", &freezeFrustum);
        ImGui::Checkbox("Debug", &debug);
        ImGui::Checkbox("Bound", &renderBound);
        ImGui::Checkbox("1 m Sphere Reference", &sphereReference);
        ImGui::End();

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

        auto originEle = floatOri ? g_fOriginEleTex->GetSrv() : g_OriginEleTex->GetSrv();

        if (showOrigin)
        {
            g_TR->Render(g_pd3dDeviceContext, originEle, originEle,
                g_Camera->GetViewProjection(), ratio, error, wireFrame);
        }
        else
        {
            g_TR->Render(g_pd3dDeviceContext,
                originEle,
                g_CompressedEleTex->GetSrv(),
                g_Camera->GetViewProjection(), ratio, error, wireFrame);
        }

        if (sphereReference)
        {
            g_DebugRenderer->DrawSphere(Matrix::CreateTranslation(g_Camera->GetPosition() + g_Camera->GetForward() * 10.0),
                g_Camera->GetView(), g_Camera->GetProjection());
        }

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        // g_pSwapChain->Present(0, 0); // Present without vsync
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
    g_Camera = std::make_unique<Camera>(ViewInit);

    g_DebugRenderer = std::make_unique<DebugRenderer>(g_pd3dDeviceContext, g_pd3dDevice);

    LoadFromWICFile(L"./asset/guanzhong.png", DirectX::WIC_FLAGS_NONE, nullptr, g_OriginEle);

    auto* image   = &g_OriginEle.GetImages()[0];
    size_t sum    = 0;
    size_t width  = image->width;
    size_t height = image->height;
    size_t size   = width * height;
    for (int i = 0; i < size; ++i)
    {
        sum += image->pixels[i];
    }
    g_OriginAvg = static_cast<size_t>(std::round(static_cast<double>(sum) / size));

    CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R8_UNORM, width, height, 1, 1);
    g_OriginEleTex = std::make_unique<DirectX::Texture2D>(g_pd3dDevice, desc, image->pixels, image->rowPitch);
    g_OriginEleTex->CreateViews(g_pd3dDevice);

    g_fOriginEleTex = std::make_unique<DirectX::Texture2D>(g_pd3dDevice, L"./asset/guanzhong.tif");
    g_fOriginEleTex->CreateViews(g_pd3dDevice);

    int iteration = IteInit;
    WaveletTransform::LeGall53<uint8_t>(image[0], g_Coefficients, iteration, g_OriginAvg);
    auto coe = g_Coefficients.GetImages()[0];
    g_Quantized.Initialize2D(g_Coefficients.GetMetadata().format, width, height, 1, 1);

    DirectX::ScratchImage quantized;
    quantized.Initialize2D(coe.format, coe.width, coe.height, 1, 1);
    auto qnt = reinterpret_cast<int16_t*>(quantized.GetImages()[0].pixels);
    WaveletTransform::FilterOut<int16_t>(reinterpret_cast<int16_t*>(coe.pixels), qnt, size, ThresInit);

    std::vector<uint8_t> uCoff;
    uCoff.reserve(size);
    auto bound = std::minmax_element(qnt, qnt + size);
    int16_t m  = *bound.first;
    std::transform(qnt, qnt + size, std::back_inserter(uCoff),
        [m](int16_t v) { return WaveletTransform::SaturatedCast<int16_t, uint8_t>(v - m); });

    g_CoefficientsTex = std::make_unique<DirectX::Texture2D>(g_pd3dDevice, desc, uCoff.data(), image->rowPitch);
    g_CoefficientsTex->CreateViews(g_pd3dDevice);

    DirectX::ScratchImage reconstructed;
    WaveletTransform::InvLeGall53<int16_t>(quantized.GetImages()[0],
        reconstructed, iteration, static_cast<int16_t>(g_OriginAvg));

    desc.Format        = reconstructed.GetImages()[0].format;
    g_CompressedEleTex = std::make_unique<DirectX::Texture2D>(g_pd3dDevice, desc, reconstructed.GetImages()[0].pixels, image->rowPitch);
    g_CompressedEleTex->CreateViews(g_pd3dDevice);

    g_TR = std::make_unique<CompressedTerrainRenderer>(g_pd3dDevice);
    g_TR->Initialize(g_pd3dDeviceContext, L"./shader");

    // ProcessChinaMap();
}

void ProcessChinaMap()
{
    DirectX::ScratchImage chinaMap;
    LoadFromWICFile(L"C:/Users/lizizhen/Desktop/china30.tif", DirectX::WIC_FLAGS_NONE, nullptr, chinaMap);
    auto w   = chinaMap.GetMetadata().width;
    auto h   = chinaMap.GetMetadata().height;
    auto src = chinaMap.GetImages()[0].pixels;

    constexpr int res = 512;
    for (int i = 0; res * (i + 1) < h; ++i)
    {
        for (int j = 0; res * (j + 1) < w; ++j)
        {
            std::vector<float> data;
            data.resize(res * res);
            for (int y = 0; y < res; ++y)
            {
                for (int x = 0; x < res; ++x)
                {
                    float val = reinterpret_cast<float*>(src)[(i * res + y) * w + j * res + x];
                    if (std::isnan(val)) val = 0.0f;
                    data[y * res + x] = val;
                }
            }

            auto minMax   = std::minmax_element(data.begin(), data.end());
            auto m        = *minMax.first, M = *minMax.second;
            auto invRange = 1.0f / (M - m);
            DirectX::ScratchImage r8uImage;
            r8uImage.Initialize2D(DXGI_FORMAT_R8_UNORM, res, res, 1, 1);
            auto uPix = reinterpret_cast<uint8_t*>(r8uImage.GetImages()[0].pixels);
            for (int idx = 0; idx < res * res; ++idx)
            {
                float val     = data[idx];
                float rounded = std::clamp(std::round(255.0f * (val - m) * invRange), 0.0f, 255.0f);
                uPix[idx]     = static_cast<uint8_t>(rounded);
            }
            {
                std::wstring file(L"./asset/r8e/china");
                file += std::to_wstring(i) + L"_" + std::to_wstring(j) + L".dds";
                SaveToDDSFile(r8uImage.GetImages()[0], DirectX::DDS_FLAGS_NONE, file.c_str());
            }

            std::vector q2(uPix, uPix + res * res);
            std::nth_element(q2.begin(), q2.begin() + res * res / 2, q2.end());
            auto median = q2[q2.size() / 2];
            DirectX::ScratchImage coeImage;
            int iteration = IteInit;
            WaveletTransform::LeGall53<uint8_t>(r8uImage.GetImages()[0], coeImage, iteration, median);

            auto coefficients = reinterpret_cast<int16_t*>(coeImage.GetImages()[0].pixels);
            WaveletTransform::FilterOut<int16_t>(coefficients, coefficients, res * res, 16);

            auto subbands = WaveletTransform::InterleaveSubBand(coefficients, res, res, iteration);

            std::string file("./asset/wti/china");
            file += std::to_string(i) + "_" + std::to_string(j) + "_coefficients.bin";
            std::ofstream out(file, std::ios::binary | std::ios::out);
            for (auto& subband : subbands)
            {
                //uLongf srcSize      = subband.size() * sizeof(decltype(subbands)::value_type::value_type);
                //uLongf dstSize      = compressBound(srcSize);
                //auto* const encoded = std::malloc(dstSize);
                //if (Z_OK != compress2(static_cast<Bytef*>(encoded), &dstSize, reinterpret_cast<const Bytef*>(subband.data()), srcSize, Z_BEST_COMPRESSION))
                //{
                //    std::printf("Compression error\n");
                //}
                size_t srcSize      = subband.size() * sizeof(decltype(subbands)::value_type::value_type);
                size_t dstSize      = FSE_compressBound(srcSize);
                auto* const encoded = std::malloc(dstSize);
                dstSize             = FSE_compressU16(encoded, dstSize, reinterpret_cast<const unsigned short*>(subband.data()), srcSize / sizeof(uint16_t), 511, 0);
                if (FSE_isError(dstSize))
                {
                    auto err = FSE_getErrorName(dstSize);
                    std::printf("Compression error : %s\n", err);
                    continue;
                }
                out.write(static_cast<const char*>(encoded), dstSize);
                std::free(encoded);
            }
            out.close();

            std::printf("%d, %d Out\n", i, j);
        }
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
