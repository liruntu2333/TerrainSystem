#include "Camera.h"

#include <algorithm>

using namespace DirectX;
using namespace SimpleMath;

Matrix Camera::GetViewProjectionMatrix() const
{
    return XMMatrixLookToLH(m_Position, m_Forward, Vector3::Up) *
        XMMatrixPerspectiveFovLH(m_Fov, m_AspectRatio, m_NearPlane, m_FarPlane);
}

Matrix Camera::GetViewProjectionRelativeToPatch(const Vector3& pos) const
{
    return XMMatrixLookToLH(pos, m_Forward, Vector3::Up) *
        XMMatrixPerspectiveFovLH(m_Fov, m_AspectRatio, m_NearPlane, m_FarPlane);
}

void Camera::SetViewPort(ID3D11DeviceContext* context) const
{
    context->RSSetViewports(1, &m_Viewport);
}

void Camera::Update(const ImGuiIO& io)
{
    m_AspectRatio = io.DisplaySize.x / io.DisplaySize.y;
    m_Viewport =
    {
        0.0f, 0.0f,
        io.DisplaySize.x, io.DisplaySize.y,
        0.0f, 1.0f
    };

    const float dt = io.DeltaTime;
    const Matrix rot = Matrix::CreateFromYawPitchRoll(m_Rotation);
    const auto forward = -rot.Forward();
    forward.Normalize(m_Forward);
    const auto right = rot.Right();
    right.Normalize(m_Right);

    constexpr float spd = 300.0f;
    Vector3 dir;
    if (io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_W)])
        dir = forward;
    if (io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_S)])
        dir = -forward;
    if (io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_A)])
        dir = -right;
    if (io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_D)])
        dir = right;
    m_Position += dir * dt * spd;

    m_Fov = io.KeyShift ? XM_PIDIV4 / 4.0f : XM_PIDIV4;

    if (io.MouseDown[ImGuiMouseButton_Right])
    {
        const auto dx = io.MouseDelta.x;
        const auto dy = io.MouseDelta.y;
        m_Rotation.x += dy * 0.001f;
        m_Rotation.y += dx * 0.001f;
        m_Rotation.x = std::clamp(m_Rotation.x, -XM_PIDIV2 + 0.1f, XM_PIDIV2 - 0.1f);
    }
}