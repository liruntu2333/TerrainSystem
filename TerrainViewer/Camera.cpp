#include "Camera.h"

#include <algorithm>
#include <fstream>

#include "Patch.h"
#include "D3DHelper.h"

using namespace DirectX;
using namespace SimpleMath;

template <typename T>
void SaveBin(const std::filesystem::path& path, const std::vector<T>& data)
{
    std::ofstream ofs(path, std::ios::binary | std::ios::out);
    if (!ofs)
        throw std::runtime_error("failed to open " + path.u8string());
    for (const auto& v : data)
        ofs.write(reinterpret_cast<const char*>(&v), sizeof(T));
    ofs.close();
    std::printf("%s generated\n", path.u8string().c_str());
}

Matrix Camera::GetViewProjection() const
{
    return GetView() * GetProjection();
}

Matrix Camera::GetView() const
{
    return XMMatrixLookToRH(m_Position, m_Forward, m_Up);
}

// Matrix Camera::GetViewLocal() const
// {
//     return XMMatrixLookToLH(m_LocalPosition, m_Forward, Vector3::Up);
// }

Matrix Camera::GetProjection() const
{
    auto proj = Matrix::CreatePerspectiveFieldOfView(m_Fov, m_AspectRatio, m_NearPlane, m_FarPlane);
    // reverse depth [0, 1] -> [1, 0]
    const auto zReverse = Matrix(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, -1, 0,
        0, 0, 1, 1);
    return proj * zReverse;
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
    BoundingFrustum f2(Matrix::CreatePerspectiveFieldOfView(m_Fov, m_AspectRatio, m_NearPlane, m_FarPlane), true);
    float topSlope = -std::tanf(m_Fov * 0.5f);
    float btmSlope = -topSlope;
    float rhtSlope = topSlope * m_AspectRatio;
    float lftSlope = -rhtSlope;
    float nearZ    = -m_FarPlane;
    float farZ     = -m_NearPlane;
    BoundingFrustum f(Vector3::Zero, Quaternion::Identity, rhtSlope, lftSlope, topSlope, btmSlope, nearZ, farZ);
    f.Transform(f, Matrix::CreateFromQuaternion(m_Orientation) * Matrix::CreateTranslation(m_Position));
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
    m_Viewport    =
    {
        0.0f, 0.0f,
        io.DisplaySize.x, io.DisplaySize.y,
        0.0f, 1.0f
    };

    const float dt = io.DeltaTime;
    m_Orientation.Normalize();
    auto forward = Vector3::Transform(Vector3::Forward, m_Orientation);
    forward.Normalize();
    m_Forward  = forward;
    auto right = Vector3::Transform(Vector3::Right, m_Orientation);
    right.Normalize();
    m_Right = right;
    auto up = Vector3::Transform(Vector3::Up, m_Orientation);
    up.Normalize();
    m_Up = up;

    if (m_IsPlaying)
    {
        if (m_RcdIndex < m_RcdPositions.size())
        {
            m_Position = m_RcdPositions[m_RcdIndex];
            m_Rotation = m_RcdRotations[m_RcdIndex];
            m_RcdIndex++;
        }
        else
        {
            m_IsPlaying = false;
            m_RcdIndex  = 0;
        }
    }
    else
    {
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

        m_Fov += io.MouseWheel * -0.1f;
        m_Fov = std::clamp(m_Fov, XM_PIDIV4 / 4.0f, XM_PIDIV2);

        if (io.MouseDown[ImGuiMouseButton_Right])
        {
            const auto dx = io.MouseDelta.x;
            const auto dy = io.MouseDelta.y;

            m_Orientation *= Quaternion::CreateFromAxisAngle(m_Up, dx * -0.001f);
            m_Orientation *= Quaternion::CreateFromAxisAngle(m_Right, dy * -0.001f);
            //m_Rotation.x = std::clamp(m_Rotation.x, -XM_PIDIV2 + 0.0001f, XM_PIDIV2 - 0.0001f);
        }
        m_Orientation *= Quaternion::CreateFromAxisAngle(m_Forward, dt *
            (io.KeysDown[ImGuiKey_E] ? 1.0f : io.KeysDown[ImGuiKey_Q] ? -1.0f : 0.0f));
    }

    if (m_IsRecording)
    {
        m_RcdPositions.push_back(m_Position);
        m_RcdRotations.push_back(m_Rotation);
    }

    // m_Position = m_LocalPosition + Vector3(m_PatchX * PATCH_SIZE, 0, m_PatchY * PATCH_SIZE);
}

void Camera::StartRecord()
{
    m_IsRecording = true;
}

void Camera::StopRecord()
{
    m_IsRecording = false;
    m_IsPlaying   = false;
    if (!m_RcdRotations.empty())
    {
        SaveBin("CameraPositions.bin", m_RcdPositions);
        SaveBin("CameraRotations.bin", m_RcdRotations);
    }
    m_RcdPositions.clear();
    m_RcdRotations.clear();
}

void Camera::PlayRecord()
{
    if (m_IsPlaying)
    {
        return;
    }
    m_IsPlaying    = true;
    m_RcdPositions = LoadBinary<Vector3>("CameraPositions.bin");
    m_RcdRotations = LoadBinary<Vector3>("CameraRotations.bin");
}

Ray Camera::GetRay(const Vector2& mousePos) const
{
    const auto x           = (2.0f * mousePos.x) / m_Viewport.Width - 1.0f;
    const auto y           = 1.0f - (2.0f * mousePos.y) / m_Viewport.Height;
    const auto rayClip     = Vector4(x, y, 0.0f, 1.0f);
    const auto invViewProj = GetViewProjection().Invert();
    const auto rayEye      = Vector4::Transform(rayClip, invViewProj);
    auto rayEye3           = Vector3(rayEye.x, rayEye.y, rayEye.z);
    rayEye3.Normalize();
    const auto rayDir    = rayEye3;
    const auto rayOrigin = m_Position;
    return { rayOrigin, rayDir };
}
