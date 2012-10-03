#include "ChunkManager.h"

using namespace cbe;

ChunkManager::ChunkManager(cgl::PD3D11Effect pEffect)
	: m_ppChunks(NULL), m_pEffect(pEffect), m_width(0), m_height(0), m_depth(0)
{
}
ChunkManager::~ChunkManager(void)
{
	Exit();
}

bool ChunkManager::Init(int mapSize, int chunkSize)
{
	UINT techniqueCount = m_pEffect->Techniques();
	for (UINT technique = 0; technique < techniqueCount; technique++)
	{
		m_techniques.push_back(RENDER_TECHNIQUE());
		m_techniques[technique].pTechnique = cgl::CD3D11EffectTechniqueFromIndex::Create(m_pEffect, technique);
		if (!CGL_RESTORE(m_techniques[technique].pTechnique))
			return false;

		UINT passCount = m_techniques[technique].pTechnique->Passes();
		for (UINT pass = 0; pass < passCount; pass++)
		{
			m_techniques[technique].passes.push_back(cgl::CD3D11EffectPassFromIndex::Create(m_techniques[technique].pTechnique, pass));
			if (!CGL_RESTORE(m_techniques[technique].passes[pass]) )
			{
				return false;
			}
		}
	}

	m_pInputLayout = cgl::CD3D11InputLayout::Create(m_techniques[0].passes[0]);
	if (!CGL_RESTORE(m_pInputLayout))
		return false;

	m_pTypeMgr = new BlockTypeManager(256);
	if(!m_pTypeMgr->Init())
		return false;

	m_ppChunks = new Chunk*[mapSize*mapSize*mapSize];

	m_width = mapSize;
	m_height = mapSize;
	m_depth = mapSize;

	for (int i = 0; i < m_width * m_height * m_depth; i++)
		m_ppChunks[i] = NULL;

	m_chunkSize = chunkSize;

	m_pBlockTypes = cgl::CD3D11EffectVariableFromSemantic::Create(m_pEffect, "BLOCKTYPES");
	if (!CGL_RESTORE(m_pBlockTypes))
		return false;

	m_pTextureAtlas = cgl::CD3D11EffectVariableFromSemantic::Create(m_pEffect, "TEXTUREATLAS");
	if (!CGL_RESTORE(m_pTextureAtlas))
		return false;

	m_pWorld = cgl::CD3D11EffectVariableFromSemantic::Create(m_pEffect, "WORLD");
	if (!CGL_RESTORE(m_pWorld))
		return false;

	return true;
}
void ChunkManager::Exit()
{
	if(m_ppChunks)
	{
		for (int i = 0; i < m_width * m_height * m_depth; i++)
			SAFE_DELETE(m_ppChunks[i]);
		
		SAFE_DELETE(m_ppChunks);
	}

	SAFE_DELETE(m_pTypeMgr);
}

void ChunkManager::SetBlockState( int x, int y, int z, bool state )
{
	unsigned int chunkIndex = 0;
	unsigned int blockIndex = 0;
	TransformCoords(x, y, z, &chunkIndex, &blockIndex);
	m_ppChunks[chunkIndex]->SetBlockState(blockIndex, state);
}
void ChunkManager::SetBlockType( int x, int y, int z, BlockType& type )
{
	unsigned int chunkIndex = 0;
	unsigned int blockIndex = 0;
	TransformCoords(x, y, z, &chunkIndex, &blockIndex);
	m_ppChunks[chunkIndex]->SetBlockType(blockIndex, type.Id());
}
void ChunkManager::SetBlockGroup( int x, int y, int z, BYTE group )
{
	unsigned int chunkIndex = 0;
	unsigned int blockIndex = 0;
	TransformCoords(x, y, z, &chunkIndex, &blockIndex);
	m_ppChunks[chunkIndex]->SetBlockGroup(blockIndex, group);
}

void ChunkManager::Render()
{
	m_pInputLayout->Bind();
	for (UINT pass = 0; pass < m_techniques[0].passes.size(); pass++)
	{
		m_techniques[0].passes[pass]->Apply();
		for (int i = 0; i < m_width * m_height * m_depth; i++)
		{
			Chunk* pChunk = m_ppChunks[i];
			if (pChunk)
				pChunk->Render();
		}
	}
}

void ChunkManager::TransformCoords( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int32* pChunkIndex, unsigned __int32* pBlockIndex )
{
	double tmp = (double)x / m_chunkSize;
	int ix = (int)(tmp + 0.0000000000005);
	int xx = x - ix * m_chunkSize;

	tmp  = round((double)y / m_chunkSize, 5);
	int iy = (int)(tmp + 0.0000000000005);
	//int yy = (int)((tmp - iy) * m_chunkSize);
	int yy = y - iy * m_chunkSize;

	tmp  = round((double)z / m_chunkSize, 5);
	int iz = (int)(tmp + 0.0000000000005);
	//int zz = (int)((tmp - iz) * m_chunkSize);
	int zz = z - iz * m_chunkSize; 

	(*pChunkIndex) = _3dto1d(ix, iy, iz, m_width, m_height);
	(*pBlockIndex) = _3dto1d(xx, yy, zz, m_chunkSize, m_chunkSize);

	// create/init chunk if it doesn't exist
	// 
	// not the optimal place to do this but
	// much simpler than returning the chunk coordinates
	// ix, iy, iz
	float absoluteChunkSize = 20.0f;
	if(!m_ppChunks[*pChunkIndex])
	{
		m_ppChunks[*pChunkIndex] = new Chunk(XMFLOAT3((float)(ix * (absoluteChunkSize)),
													  (float)(iy * (absoluteChunkSize)), 
													  (float)(iz * (absoluteChunkSize))), m_chunkSize, absoluteChunkSize / m_chunkSize, this);
		m_ppChunks[*pChunkIndex]->Init();
	}
}

void ChunkManager::Update()
{
	if (m_pTypeMgr->Update())
	{
		//m_pTextureAtlas->get()->AsShaderResource()->SetResource(m_pTypeMgr->GetAtlas()->GetShaderResource()->get());
		m_pTextureAtlas->get()->AsShaderResource()->SetResourceArray(m_pTypeMgr->GetAtlases()->get(), 0, m_pTypeMgr->GetAtlases()->GetCollectionSize());
		//m_pTextureAtlas->get()->AsShaderResource()->SetResource(m_pTypeMgr->GetAtlasArraySRV()->get());
		m_pBlockTypes->get()->SetRawValue(m_pTypeMgr->GetPackedTypes().data(), 0, sizeof(PackedBlockType) * m_pTypeMgr->GetPackedTypes().size());
	}

	for (int i = 0; i < m_width * m_height * m_depth; i++)
	{
		Chunk* pChunk = m_ppChunks[i];
		if (pChunk)
			pChunk->Build(this);
	}
}

int ChunkManager::GetActiveBlockCount()
{
	int blockCount = 0;
	for (int i = 0; i < m_width * m_height * m_depth; i++)
	{
		Chunk* pChunk = m_ppChunks[i];
		if (pChunk)
			blockCount += pChunk->ActiveBlocks();
	}

	return blockCount;
}

int ChunkManager::GetVertexCount()
{
	int vertexCount = 0;
	for (int i = 0; i < m_width * m_height * m_depth; i++)
	{
		Chunk* pChunk = m_ppChunks[i];
		if (pChunk)
			vertexCount += pChunk->VertexCount();
	}

	return vertexCount;
}

int ChunkManager::GetActiveChunkCount()
{
	int chunkCount = 0;
	for (int i = 0; i < m_width * m_height * m_depth; i++)
	{
		Chunk* pChunk = m_ppChunks[i];
		if (pChunk)
			chunkCount++;
	}

	return chunkCount;
}
