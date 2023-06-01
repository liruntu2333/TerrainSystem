#include "TerrainVertex.h"

const D3D11_INPUT_ELEMENT_DESC TerrainVertex::InputElements[InputElementCount] =
{
	{
		"SV_Position",
		0,
		DXGI_FORMAT_R8G8_UNORM,
		0,
		0,
		D3D11_INPUT_PER_VERTEX_DATA,
		0
	}
};