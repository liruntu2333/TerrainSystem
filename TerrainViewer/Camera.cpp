#include "Camera.h"

#include <algorithm>
#include "Patch.h"

using namespace DirectX;
using namespace SimpleMath;

Matrix Camera::GetViewProjection() const
{
    return GetView() * GetProjection();
}

Matrix Camera::GetView() const
{
    return XMMatrixLookToLH(m_Position, m_Forward, Vector3::Up);
}

// Matrix Camera::GetViewLocal() const
// {
//     return XMMatrixLookToLH(m_LocalPosition, m_Forward, Vector3::Up);
// }

Matrix Camera::GetProjection() const
{
    return XMMatrixPerspectiveFovLH(m_Fov, m_AspectRatio, m_NearPlane, m_FarPlane);
}

// Matrix Camera::GetProjectionLocal() const
// {
//     return XMMatrixPerspectiveFovLH(m_Fov, m_AspectRatio, m_NearPlane, m_FarPlane);
// }

// Matrix Camera::GetViewProjectionLocal() const
// {
//     return XMMatrixLookToLH(m_LocalPosition, m_Forward, Vector3::Up) *
//         XMMatrixPerspectiveFovLH(m_Fov, m_AspectRatio, m_NearPlane, m_FarPlane);
// }

BoundingFrustum Camera::GetFrustum() const
{
    BoundingFrustum f(XMMatrixPerspectiveFovLH(m_Fov, m_AspectRatio, m_NearPlane, m_FarPlane));
    f.Transform(f, Matrix::CreateFromYawPitchRoll(m_Rotation) * Matrix::CreateTranslation(m_Position));
    return f;
}

// BoundingFrustum Camera::GetFrustumLocal() const
// {
//     BoundingFrustum f(XMMatrixPerspectiveFovLH(m_Fov, m_AspectRatio, m_NearPlane, m_FarPlane));
//     f.Transform(f, Matrix::CreateFromYawPitchRoll(m_Rotation) * Matrix::CreateTranslation(m_LocalPosition));
//     return f;
// }

void Camera::SetViewPort(ID3D11DeviceContext* context) const
{
    context->RSSetViewports(1, &m_Viewport);
}

void Camera::Update(const ImGuiIO& io, float spd)
{
    m_AspectRatio = io.DisplaySize.x / io.DisplaySize.y;
    m_Viewport =
    {
        0.0f, 0.0f,
        io.DisplaySize.x, io.DisplaySize.y,
        0.0f, 1.0f
    };

    const float dt = io.DeltaTime;
    m_Orientation = Quaternion::CreateFromYawPitchRoll(m_Rotation);
    m_Orientation.Normalize();
    auto forward = Vector3::Transform(-Vector3::Forward, m_Orientation);
    forward.Normalize();
    m_Forward = forward;
    auto right = Vector3::Transform(Vector3::Right, m_Orientation);
    right.Normalize();

    Vector3 dir;
    if (io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_W)])
        dir += forward;
    else if (io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_S)])
        dir -= forward;
    if (io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_A)])
        dir -= right;
    else if (io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_D)])
        dir += right;
    dir.Normalize();

    m_DeltaPosition = dir * dt * spd;
    m_Position += m_DeltaPosition;

    // Vector2 dxy;
    // m_LocalPosition.x = std::modf(m_LocalPosition.x / PATCH_SIZE, &dxy.x) * PATCH_SIZE;
    // m_LocalPosition.z = std::modf(m_LocalPosition.z / PATCH_SIZE, &dxy.y) * PATCH_SIZE;
    // m_PatchX += static_cast<int>(dxy.x);
    // m_PatchY += static_cast<int>(dxy.y);

    m_Fov = io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_Q)]
                ? XM_PIDIV4 / 4.0f
                : io.KeysDown[ImGui::GetKeyIndex(ImGuiKey_E)]
                      ? XM_PIDIV2
                      : XM_PIDIV4;

    if (io.MouseDown[ImGuiMouseButton_Right])
    {
        const auto dx = io.MouseDelta.x;
        const auto dy = io.MouseDelta.y;
        m_Rotation.x += dy * 0.001f;
        m_Rotation.y += dx * 0.001f;
        m_Rotation.x = std::clamp(m_Rotation.x, -XM_PIDIV2 + 0.0001f, XM_PIDIV2 - 0.0001f);
    }

    // m_Position = m_LocalPosition + Vector3(m_PatchX * PATCH_SIZE, 0, m_PatchY * PATCH_SIZE);
}
