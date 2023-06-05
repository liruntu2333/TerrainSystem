#pragma once

#include <directxtk/SimpleMath.h>
#include <d3d11.h>
#include "imgui.h"

class Camera
{
public:

    [[nodiscard]] DirectX::SimpleMath::Matrix GetViewProjectionMatrix() const;
    [[nodiscard]] DirectX::SimpleMath::Matrix GetViewMatrix() const;
    [[nodiscard]] DirectX::SimpleMath::Matrix GetProjectionMatrix() const;
    [[nodiscard]] DirectX::SimpleMath::Matrix GetViewProjectionRelativeToPatch(
        const DirectX::SimpleMath::Vector3& pos) const;
    [[nodiscard]] DirectX::SimpleMath::Vector3 GetPosition() const { return m_Position; }
    [[nodiscard]] DirectX::BoundingFrustum GetFrustum() const;
    void SetViewPort(ID3D11DeviceContext* context) const;
    void Update(const ImGuiIO& io);

private:
    DirectX::SimpleMath::Vector3 m_Position { 0, 1250.0f, 0 };
    DirectX::SimpleMath::Vector3 m_Rotation { 0.0f, 0, 0.0f }; // row pitch yaw
    DirectX::SimpleMath::Vector3 m_Forward;
    DirectX::SimpleMath::Vector3 m_Right;
    D3D11_VIEWPORT m_Viewport {};
    float m_Fov = DirectX::XM_PIDIV4;
    float m_AspectRatio = 0;
    float m_NearPlane = 1.0f;
    float m_FarPlane = 20000.0f;

    //int m_PatchX = 0;
    //int m_PatchZ = 0;
    //DirectX::SimpleMath::Vector3 m_LocalPosition { 0, 1150.0f, 0 };
};
