#include "Renderer.h"

std::unique_ptr<DirectX::CommonStates> Renderer::s_CommonStates = nullptr;

Renderer::Renderer(ID3D11Device* device) : m_Device(device)
{
    if (s_CommonStates == nullptr)
        s_CommonStates = std::make_unique<DirectX::CommonStates>(device);
}
