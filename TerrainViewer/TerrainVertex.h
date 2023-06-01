#pragma once
#include <cstdint>
#include <d3d11.h>

struct TerrainVertex
{
	uint8_t PositionX;
	uint8_t PositionY;
	//uint8_t MorphX;
	//uint8_t MorphZ;

	static constexpr unsigned int InputElementCount = 1;
	static const D3D11_INPUT_ELEMENT_DESC InputElements[InputElementCount];
};
