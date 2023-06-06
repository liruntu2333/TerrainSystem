#pragma once

#include <directxtk/CommonStates.h>
#include <directxtk/SimpleMath.h>

struct PassConstants
{
	DirectX::SimpleMath::Matrix ViewProjection;
	DirectX::SimpleMath::Vector3 LightDir;
	float LightIntensity;
	DirectX::XMINT2 CameraXy;
	int Padding[2];
};

struct ObjectConstants
{
	DirectX::XMUINT2 PatchXy;
	uint32_t Color;
	int Padding;
};

class Renderer
{
public:
	Renderer(ID3D11Device* device);
	virtual ~Renderer() = default;

	static std::unique_ptr<DirectX::CommonStates> s_CommonStates;

protected:

	ID3D11Device* m_Device = nullptr;
};
