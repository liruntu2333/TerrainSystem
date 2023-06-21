#pragma once

#include <directxtk/SimpleMath.h>
#include <d3d11.h>
#include "imgui.h"

class Camera
{
public:
    [[nodiscard]] DirectX::SimpleMath::Matrix GetViewProjection() const;
    [[nodiscard]] DirectX::SimpleMath::Matrix GetView() const;
    [[nodiscard]] DirectX::SimpleMath::Matrix GetViewLocal() const;
    [[nodiscard]] DirectX::SimpleMath::Matrix GetProjection() const;
    [[nodiscard]] DirectX::SimpleMath::Matrix GetProjectionLocal() const;
    [[nodiscard]] DirectX::SimpleMath::Matrix GetViewProjectionLocal() const;
    [[nodiscard]] DirectX::SimpleMath::Vector3 GetPosition() const { return m_Position; }
    [[nodiscard]] DirectX::SimpleMath::Vector3 GetPositionLocal() const { return m_LocalPosition; } 
    [[nodiscard]] DirectX::BoundingFrustum GetFrustum() const;
    [[nodiscard]] DirectX::BoundingFrustum GetFrustumLocal() const;
    [[nodiscard]] DirectX::XMINT2 GetPatch() const { return { m_PatchX, m_PatchY }; }
    void SetViewPort(ID3D11DeviceContext* context) const;
    void Update(const ImGuiIO& io, float spd);

private:
    DirectX::SimpleMath::Vector3 m_Position { 0, 400, 0 };
    DirectX::SimpleMath::Vector3 m_Rotation { DirectX::XM_PIDIV4, 0, 0.0f }; // row pitch yaw
    DirectX::SimpleMath::Vector3 m_Forward;
    DirectX::SimpleMath::Vector3 m_Right;
    D3D11_VIEWPORT m_Viewport {};
    float m_Fov = DirectX::XM_PIDIV4;
    float m_AspectRatio = 0;
    float m_NearPlane = 0.01f;
    float m_FarPlane = 5000.0f;

    int m_PatchX = 0;
    int m_PatchY = 0;
    DirectX::SimpleMath::Vector3 m_LocalPosition { 0, 400, 0 };
};
