#pragma once

#include <directxtk/CommonStates.h>
#include <directxtk/SimpleMath.h>

struct PassConstants
{
	DirectX::SimpleMath::Matrix ViewProjectionLocal;	// within local patch coordinate
	DirectX::SimpleMath::Matrix ViewProjection;			// within world
	DirectX::SimpleMath::Vector3 LightDirection;
	float LightIntensity;
	DirectX::XMINT2 ViewPatch;
	DirectX::SimpleMath::Vector2 ViewPosition;
	DirectX::SimpleMath::Vector2 AlphaOffset;
	float OneOverWidth;									// 1 / w(h)
	float HeightScale;
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
