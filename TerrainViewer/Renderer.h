#pragma once

#include <directxtk/CommonStates.h>
#include <directxtk/SimpleMath.h>

struct PassConstants
{
	DirectX::SimpleMath::Matrix ViewProjectionLocal;
	DirectX::SimpleMath::Matrix ViewProjection;
	DirectX::SimpleMath::Vector3 LightDir;
	float LightIntensity;
	DirectX::XMINT2 CameraXy;
	int Padding[2];
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
