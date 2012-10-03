#pragma once

#include "BlockTypeManager.h"
#include "BlockType.h"
#include "Chunk.h"
#include "stdafx.h"
#include <cmath>

namespace cbe
{

class Chunk;
class CBE_API ChunkManager
{
private:
	BlockTypeManager* m_pTypeMgr;
	Chunk** m_ppChunks;
	int m_width;
	int m_height;
	int m_depth;
	int m_chunkSize;
	
	// d3d
	struct RENDER_TECHNIQUE
	{
		cgl::PD3D11EffectTechnique pTechnique;
		std::vector<cgl::PD3D11EffectPass> passes;
	};
	int m_currTechnique;
	cgl::PD3D11Effect m_pEffect;
	cgl::PD3D11InputLayout m_pInputLayout;
	std::vector<RENDER_TECHNIQUE> m_techniques;

	cgl::PD3D11EffectVariable	m_pBlockTypes;
	cgl::PD3D11EffectVariable	m_pTextureAtlas;
	cgl::PD3D11EffectVariable	m_pWorld;

	inline UINT _3dto1d(unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int8 width, unsigned __int8 height) { return z + y * width + x * height * width; }

	void TransformCoords(unsigned __int8 x, unsigned __int8 y, unsigned __int8 z,
						 unsigned __int32* pChunkIndex, unsigned __int32* pBlockIndex);

	bool CreateChunk(int chunkIndex, int x, int y, int z );

	inline double round( double x, int places )
	{
		const double sd = pow(10.0, places);
		return int(x*sd + 0.5) / sd;
	}
public:
	ChunkManager(cgl::PD3D11Effect pEffect);
	~ChunkManager(void);

	bool Init(int mapSize, int chunkSize);
	void Render();
	void Update();
	void Exit();

	void SetBlockState(int x, int y, int z, bool state);
	void SetBlockType(int x, int y, int z, BlockType& type);
	void SetBlockGroup(int x, int y, int z, BYTE group);

	int GetActiveChunkCount();
	int GetActiveBlockCount();
	int GetVertexCount();

	inline BlockTypeManager* TypeManager() const { return m_pTypeMgr; }
};

}
