#pragma once

#include <directxtk/CommonStates.h>
#include <directxtk/SimpleMath.h>

struct PassConstants
{
	DirectX::SimpleMath::Matrix ViewProjection;
	DirectX::XMINT2 PatchOffset;
	DirectX::XMUINT2 PatchXY;
	uint32_t Color;
	DirectX::SimpleMath::Vector3 LightDir;
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
