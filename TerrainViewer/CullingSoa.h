#pragma once
#include "AlignedVector.h"
#include <directxtk/SimpleMath.h>

template <uint32_t Capacity>
class CullingSoa
{
public:
	CullingSoa();
	~CullingSoa() = default;

	CullingSoa(const CullingSoa&) = delete;
	CullingSoa(CullingSoa&&) = delete;
	CullingSoa& operator=(const CullingSoa&) = delete;
	CullingSoa& operator=(CullingSoa&&) = delete;

	template <class Architecture>
	std::vector<uint32_t> TickCulling(const std::vector<DirectX::BoundingSphere>& data,
		const DirectX::BoundingFrustum& frustum);

	// scalar version
	std::vector<uint32_t> TickCulling(const std::vector<DirectX::BoundingSphere>& data,
		const DirectX::BoundingFrustum& frustum);

private:
	void Clear();
	void Push(const DirectX::BoundingSphere& sphere);
	void Push(const std::vector<DirectX::BoundingSphere>& spheres);

	[[nodiscard]] std::vector<uint32_t> ComputeScalar(const DirectX::BoundingFrustum& frustum) const;
	template <class Architecture>
	[[nodiscard]] std::vector<uint32_t> ComputeVector(const DirectX::BoundingFrustum& frustum) const;

	static constexpr uint32_t NFloatField = 4;
	static constexpr uint32_t NBoolField = 1;
	static constexpr uint32_t Alignment = xsimd::avx2::alignment(); // bigger alignment for new arch

	AlignedVector<float, Capacity, NFloatField, Alignment> m_FloatChunk;
	AlignedVector<bool, Capacity, NBoolField, Alignment> m_BoolChunk;
	uint32_t m_Size = 0;

	float* const m_PositionX;
	float* const m_PositionY;
	float* const m_PositionZ;
	float* const m_Radius;
	bool* const m_IsVisible;
};

template <uint32_t Capacity>
CullingSoa<Capacity>::CullingSoa() : m_FloatChunk(), m_BoolChunk(),
m_PositionX(m_FloatChunk[0]), m_PositionY(m_FloatChunk[1]), m_PositionZ(m_FloatChunk[2]), m_Radius(m_FloatChunk[3]), m_IsVisible(m_BoolChunk[0])
{
}

template <uint32_t Capacity>
void CullingSoa<Capacity>::Push(const DirectX::BoundingSphere& sphere)
{
	if (m_Size >= Capacity) return;

	m_PositionX[m_Size] = sphere.Center.x;
	m_PositionY[m_Size] = sphere.Center.y;
	m_PositionZ[m_Size] = sphere.Center.z;
	m_Radius[m_Size] = sphere.Radius;
	m_Size++;
}

template <uint32_t Capacity>
void CullingSoa<Capacity>::Push(const std::vector<DirectX::BoundingSphere>& spheres)
{
	for (const auto& sphere : spheres) Push(sphere);
}

template <uint32_t Capacity>
std::vector<uint32_t> CullingSoa<Capacity>::ComputeScalar(const DirectX::BoundingFrustum& frustum) const
{
	DirectX::XMVECTOR vs[6];
	frustum.GetPlanes(vs, vs + 1, vs + 2, vs + 3, vs + 4, vs + 5);
	DirectX::SimpleMath::Plane planes[6];
	for (uint32_t i = 0; i < 6; ++i)
	{
		planes[i] = DirectX::SimpleMath::Plane(vs[i]);
	}

	for (uint32_t i = 0; i < m_Size; ++i)
	{
		bool isVisible = true;
		for (const auto& plane : planes)
			isVisible = isVisible && plane.w + m_PositionX[i] * plane.x + m_PositionY[i] * plane.y + m_PositionZ[i] * plane.z < m_Radius[i];
		m_IsVisible[i] = isVisible;
	}

	std::vector<uint32_t> result;
	for (uint32_t i = 0; i < m_Size; ++i)
	{
		if (m_IsVisible[i]) result.push_back(i);
	}
	return result;
}

template <uint32_t Capacity>
template <class Architecture>
std::vector<uint32_t> CullingSoa<Capacity>::ComputeVector(const DirectX::BoundingFrustum& frustum) const
{
	DirectX::XMVECTOR vs[6];
	frustum.GetPlanes(vs, vs + 1, vs + 2, vs + 3, vs + 4, vs + 5);
	DirectX::SimpleMath::Plane planes[6];
	for (uint32_t i = 0; i < 6; ++i)
	{
		planes[i] = DirectX::SimpleMath::Plane(vs[i]);
	}

	using FloatBatch = xsimd::batch<float, Architecture>;
	using BoolBatch = typename FloatBatch::batch_bool_type;
	const uint32_t stride = FloatBatch::size;

	auto dispatch = [stride, &planes, this](uint32_t from, uint32_t length)
	{
		const uint32_t alignedSize = length - length % stride;

		for (uint32_t i = from; i < from + alignedSize; i += stride)
		{
			BoolBatch visible(true);
			for (const auto& plane : planes)
			{
				// 3 multiply 3 add 1 compare
				FloatBatch dist(plane.w);
				dist += FloatBatch::load_aligned(m_PositionX + i) * plane.x;
				dist += FloatBatch::load_aligned(m_PositionY + i) * plane.y;
				dist += FloatBatch::load_aligned(m_PositionZ + i) * plane.z;
				BoolBatch const result = dist < FloatBatch::load_aligned(m_Radius + i);
				visible = visible & result;
			}
			visible.store_aligned(m_IsVisible + i);
		}

		for (uint32_t i = from + alignedSize; i < from + length; ++i)
		{
			bool visible = true;
			for (const auto& plane : planes)
				visible &= plane.w + m_PositionX[i] * plane.x + m_PositionY[i] * plane.y + m_PositionZ[i] * plane.z < m_Radius[i];
			m_IsVisible[i] = visible;
		}
	};

	// multi-threading
	//ThreadPool* pool = g_Context.Pool.get();
	//const auto threadCount = g_Context.ThreadCount;
	//std::vector<std::future<void>> futures;
	//const int groupLen = m_Size / threadCount;

	//for (int i = 0; i < threadCount; ++i)
	//    futures.emplace_back(
	//        pool->enqueue(dispatch, i * groupLen, groupLen));

	//for (uint32_t i = 0; i < threadCount; ++i)
	//    futures[i].wait();

	dispatch(0, m_Size);

	std::vector<uint32_t> visible;
	for (uint32_t i = 0; i < m_Size; ++i)
		if (m_IsVisible[i])
			visible.push_back(i);

	return visible;
}

template <uint32_t Capacity>
std::vector<uint32_t> CullingSoa<Capacity>::TickCulling(const std::vector<DirectX::BoundingSphere>& data,
	const DirectX::BoundingFrustum& frustum)
{
	Clear();
	Push(data);

	return ComputeScalar(frustum);
}

template <uint32_t Capacity>
template <class Architecture>
std::vector<uint32_t> CullingSoa<Capacity>::TickCulling(const std::vector<DirectX::BoundingSphere>& data,
	const DirectX::BoundingFrustum& frustum)
{
	Clear();
	Push(data);

	return ComputeVector<Architecture>(frustum);
}

template <uint32_t Capacity>
void CullingSoa<Capacity>::Clear()
{
	m_Size = 0;
}
