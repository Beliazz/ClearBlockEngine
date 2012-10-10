#include "cbe.h"

using namespace cbe;

//////////////////////////////////////////////////////////////////////////
// chunk triangle merged
// 
Chunk::Chunk(ChunkManager* pManager, int ix, int iy, int iz, XMFLOAT3 pos, unsigned __int8 chunkSize, float blockSize, ChunkManager* pMgr )
	: m_pManager(pManager), m_ix(ix), m_iy(iy), m_iz(iz), m_vecPos(pos), m_size(chunkSize), m_blockSize(blockSize), m_numTris(0), m_numVertices(0),
		m_numActiveBlocks(0), m_numIndices(0), m_numBlocksVisible(0)
{
	m_pBlocks = new Block[chunkSize * chunkSize * chunkSize];
	m_upToDate = false;
	m_building = false;
	
	InitializeCriticalSection(&m_criticalSection);
}
Chunk::~Chunk( void )
{
	EnterCriticalSection(&m_criticalSection);

	SAFE_DELETE_ARRAY(m_pBlocks);

	m_pVertexBuffer->ResetData();
	m_pIndexBuffer->ResetData();

	LeaveCriticalSection(&m_criticalSection);
	DeleteCriticalSection(&m_criticalSection);
}

void Chunk::SetBlockState( int x, int y, int z, BOOL state )
{
	EnterCriticalSection(&m_criticalSection);

	m_pBlocks[_3dto1d(x, y, z)].SetActive(state);
	m_upToDate = false;

	LeaveCriticalSection(&m_criticalSection);
}
void Chunk::SetBlockState( int index, BOOL state )
{	
	EnterCriticalSection(&m_criticalSection);

	m_pBlocks[index].SetActive(state);
	m_upToDate = false;

	LeaveCriticalSection(&m_criticalSection);
}
bool Chunk::GetBlockState( int x, int y, int z )
{
	EnterCriticalSection(&m_criticalSection);

	if ( x < 0 || x >= m_size ||
		 y < 0 || y >= m_size ||
		 z < 0 || z >= m_size)
	{
		return false;
	}

	bool active =  m_pBlocks[_3dto1d(x, y, z)].Active();
	LeaveCriticalSection(&m_criticalSection);
	return active;
}
bool Chunk::GetBlockState( int index )
{
	EnterCriticalSection(&m_criticalSection);
	bool active =  m_pBlocks[index].Active();
	LeaveCriticalSection(&m_criticalSection);
	return active;
}

void Chunk::SetBlockType( int x, int y, int z, unsigned __int16 type )
{
	EnterCriticalSection(&m_criticalSection);

	m_pBlocks[_3dto1d(x, y, z)].SetType(type);
	m_upToDate = false;

	LeaveCriticalSection(&m_criticalSection);
}
void Chunk::SetBlockType( int index, unsigned __int16 type )
{
	EnterCriticalSection(&m_criticalSection);

	m_pBlocks[index].SetType(type);
	m_upToDate = false;

	LeaveCriticalSection(&m_criticalSection);
}
unsigned __int16 Chunk::GetBlockType( int x, int y, int z )
{
	EnterCriticalSection(&m_criticalSection);
	unsigned __int16 type = m_pBlocks[z + y * m_size + x * m_size * m_size].Type();
	LeaveCriticalSection(&m_criticalSection);

	return type;
}

void Chunk::SetBlockGroup( int index, BYTE group )
{
	EnterCriticalSection(&m_criticalSection);

	m_pBlocks[index].SetGroup(group);
	m_upToDate = false;

	LeaveCriticalSection(&m_criticalSection);
}

bool Chunk::Init()
{
	m_pIndexBuffer = cgl::CD3D11IndexBuffer::Create(sizeof(DWORD), D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	m_pVertexBuffer = cgl::CD3D11VertexBuffer::Create(sizeof(BlockVertex), D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

	return true;
}
bool Chunk::Update()
{
	EnterCriticalSection(&m_criticalSection);
	if (!m_building && m_upToDate)
	{
		if (m_numTris != 0)
		{
			m_pVertexBuffer->Update();
			m_pIndexBuffer->Update();
		}

		LeaveCriticalSection(&m_criticalSection);
		return true;
	}
	LeaveCriticalSection(&m_criticalSection);
	return false;
}
void Chunk::Render()
{
	EnterCriticalSection(&m_criticalSection);
	if (m_numTris != 0)
	{
		m_pVertexBuffer->Bind();
		m_pIndexBuffer->Bind();
		m_pIndexBuffer->Draw();
	}
	LeaveCriticalSection(&m_criticalSection);
}
void Chunk::RenderBatched( UINT* pOffset )
{
	if (m_numTris == 0)
		return;

	m_pIndexBuffer->Bind();
	m_pIndexBuffer->Draw(0, m_numIndices, *pOffset);
	(*pOffset) += m_numVertices;
}

bool Chunk::Build(ChunkManager* pMgr)
{
	bool leave = false;
	EnterCriticalSection(&m_criticalSection);
	leave = m_building || m_upToDate;
	LeaveCriticalSection(&m_criticalSection);

	if (leave)
		return false;

	EnterCriticalSection(&m_criticalSection);
	m_building = true;
	m_pVertexBuffer->ResetData();
	m_pIndexBuffer->ResetData();
	m_numActiveBlocks = 0;
	m_numBlocksVisible = 0;
	m_numIndices = 0;
	m_numTris = 0;
	m_numVertices = 0;
	LeaveCriticalSection(&m_criticalSection);

	BLOCK_INFO* pBlockInfo = new BLOCK_INFO[m_size * m_size * m_size];
	ZeroMemory(pBlockInfo, m_size * m_size * m_size * sizeof(BLOCK_INFO));

	for (int z = 0; z < m_size; z++)
	{
		for (int y = 0; y < m_size; y++)
		{
			for (int x = 0; x < m_size; x++)
			{
				GetBlockInfo(x, y, z, pBlockInfo);
				if(pBlockInfo[_3dto1d(x, y, z)].info & BLOCK_VISIBLE)
				{
					EnterCriticalSection(&m_criticalSection);
					m_numBlocksVisible++;
					unsigned __int16 blockTypeIndex = m_pBlocks[_3dto1d(x, y, z)].Type();
					unsigned __int16 blockTypeGroup = m_pBlocks[_3dto1d(x, y, z)].Group();
					LeaveCriticalSection(&m_criticalSection);

					BlockType* type = pMgr->TypeManager()->GetType(blockTypeIndex);
					bool invalidType = false;
					if (!type)
					{
						type = new BlockType();
						invalidType = true;
					}

					// merge
					RECTANGLE rects[6];
					UINT numRects = Merge(x, y, z, blockTypeGroup, *type, rects, pBlockInfo);
					if (numRects == 0)
						continue;

					BlockVertex* pVerts = new BlockVertex[numRects * 4];
					DWORD* pIndices = new DWORD[numRects * 6];
					DWORD baseIndices[6] = { 0, 1, 2, 0, 2, 3};
					for (UINT rect = 0; rect < numRects; rect++)
					{
						// Vertices
						for (int i = 0; i < 4; i++)
						{
							pVerts[rect * 4 + i].indices[VERT_INDEX_TYPE] = type->Id();
							pVerts[rect * 4 + i].indices[VERT_INDEX_NORMAL] = rects[rect].normalIndex;
							pVerts[rect * 4 + i].pos = rects[rect].corners[i];
							pVerts[rect * 4 + i].texCoord = rects[rect].texCoords[i];
						}

						// indices
						EnterCriticalSection(&m_criticalSection);
						for (int i = 0; i < 6; i++)
							pIndices[rect * 6 + i] = m_numVertices + rect * 4 + baseIndices[i];
						LeaveCriticalSection(&m_criticalSection);
					}

					EnterCriticalSection(&m_criticalSection);

					m_numVertices += numRects * 4;
					m_numIndices += numRects * 6;
					m_numTris += numRects * 2;

					m_pIndexBuffer->AddData((char*)pIndices, numRects * 6);
					m_pVertexBuffer->AddData((char*)pVerts, numRects * 4);

					LeaveCriticalSection(&m_criticalSection);

					SAFE_DELETE_ARRAY(pVerts);
					SAFE_DELETE_ARRAY(pIndices);

					if (invalidType)
						SAFE_DELETE(type);
				}
			}
		}
	}

	SAFE_DELETE_ARRAY(pBlockInfo);

 	if (m_numTris > 0)
 	{
		EnterCriticalSection(&m_criticalSection);

		LeaveCriticalSection(&m_criticalSection);
	}

	EnterCriticalSection(&m_criticalSection);
	m_upToDate = true;
	m_building = false;
	LeaveCriticalSection(&m_criticalSection);

	return true;
}
bool Chunk::BuildIt( ChunkManager* pMgr )
{
	EnterCriticalSection(&m_criticalSection);
	if (m_building || m_upToDate)
		return false;
	LeaveCriticalSection(&m_criticalSection);

	m_building = true;

	return true;
}

void Chunk::GetBlockInfo( __in unsigned __int8 x, __in unsigned __int8 y, __in unsigned __int8 z, __inout BLOCK_INFO* pBlockInfo )
{
	EnterCriticalSection(&m_criticalSection);
	unsigned short info = pBlockInfo[_3dto1d(x, y, z)].info;
	LeaveCriticalSection(&m_criticalSection);

	if (x < 0 || x >= m_size ||
		y < 0 || y >= m_size ||
		z < 0 || z >= m_size ||
		info & BLOCK_CHECKED)
	{
		return;
	}

	if (!GetBlockState(x, y, z))
	{
		info = 0;
		info |= BLOCK_CHECKED;

		EnterCriticalSection(&m_criticalSection);
		pBlockInfo[_3dto1d(x, y, z)].info = info;
		LeaveCriticalSection(&m_criticalSection);

		return;
	}

	info |= BLOCK_ACTIVE;
	m_numActiveBlocks++;

	unsigned __int8 numFacesVisible = 0;
	for ( unsigned __int16 face = 1; face <= BLOCK_FACE_COUNT; face *= 4 )
	{
		int ix = x;
		int iy = y;
		int iz = z;

		switch(face)
		{
		case BLOCK_FACE_VISIBLE_FRONT:  { iz--; } break;
		case BLOCK_FACE_VISIBLE_BACK:	{ iz++; } break;
		case BLOCK_FACE_VISIBLE_LEFT:	{ ix--; } break; 
		case BLOCK_FACE_VISIBLE_RIGHT:  { ix++; } break; 
		case BLOCK_FACE_VISIBLE_BOTTOM: { iy--; } break; 
		case BLOCK_FACE_VISIBLE_TOP:	{ iy++; } break; 
		}

		if (ix < 0 || ix >= m_size ||
			iy < 0 || iy >= m_size ||
			iz < 0 || iz >= m_size)
		{
// 			if (!m_pManager->GetBlockState(m_ix * m_size + ix, m_iy * m_size + iy, m_iz * m_size + iz))
// 			{
// 				info |= face;
// 				numFacesVisible++;
// 			}

			info |= face;
			numFacesVisible++;
		}
		else if (GetBlockState(ix, iy, iz) == false)
		{
			info |= face;
			numFacesVisible++;
		}
	}

	if (numFacesVisible > 0)
	{
		info |= BLOCK_VISIBLE;
	}

	info |= BLOCK_CHECKED;

	EnterCriticalSection(&m_criticalSection);
	pBlockInfo[_3dto1d(x, y, z)].info = info;
	LeaveCriticalSection(&m_criticalSection);
}

unsigned __int8 Chunk::Merge( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int16 group, BlockType& type, RECTANGLE* pRect, BLOCK_INFO* pBlockInfo )
{
	EnterCriticalSection(&m_criticalSection);
	unsigned short info = pBlockInfo[_3dto1d(x, y, z)].info;
	LeaveCriticalSection(&m_criticalSection);

	unsigned __int8 numRects = 0;
	for ( unsigned __int16 face = 1; face <= BLOCK_FACE_COUNT; face *= 4 )
	{
 		if (info & face  && (info & (2 * face)) != 2 * face)
		{
			switch(face)
			{
			case BLOCK_FACE_VISIBLE_FRONT:
			case BLOCK_FACE_VISIBLE_BACK: 
				{ 	  
					MergeXY(x, y, z, face, group, type, &pRect[numRects], pBlockInfo); 
				} break;

			case BLOCK_FACE_VISIBLE_LEFT:
			case BLOCK_FACE_VISIBLE_RIGHT: 
				{ 
					MergeYZ(x, y, z, face, group, type, &pRect[numRects], pBlockInfo);
				} break; 

			case BLOCK_FACE_VISIBLE_BOTTOM:
			case BLOCK_FACE_VISIBLE_TOP:	
				{ 
					MergeXZ(x, y, z, face, group, type, &pRect[numRects], pBlockInfo);
				} break; 
			}

			pBlockInfo[_3dto1d(x, y, z)].info |= (2 * face);
			numRects++;
		}
	}

	return numRects;
}
void Chunk::MergeXY( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int16 face, unsigned __int16 group, BlockType& type, RECTANGLE* pRect, BLOCK_INFO* pBlockInfo )
{
	// left border
	unsigned __int8 left = 0;
	if (x != left)
	{
		for (unsigned __int8 ix = x - 1; ix >= 0; ix--)
		{
			GetBlockInfo(ix, y, z, pBlockInfo);
			if (!BlockMergeable(ix, y, z, face, group, type, pBlockInfo))
			{
				left = ix + 1;
				break;
			}

			EnterCriticalSection(&m_criticalSection);
			pBlockInfo[_3dto1d(ix, y, z)].info |= (2 * face);
			LeaveCriticalSection(&m_criticalSection);
		}
	}

	// right border
	unsigned __int8 right = m_size - 1;
	if (x != right)
	{
		for (unsigned __int8 ix = x + 1; ix < m_size; ix++)
		{
			GetBlockInfo(ix, y, z, pBlockInfo);
			if (!BlockMergeable(ix, y, z, face, group, type, pBlockInfo))
			{
				right = ix - 1;
				break;
			}

			EnterCriticalSection(&m_criticalSection);
			pBlockInfo[_3dto1d(ix, y, z)].info |= (2 * face);
			LeaveCriticalSection(&m_criticalSection);
		}
	}

	// bottom border
	unsigned __int8 bottom = 0;
	if ( y != bottom)
	{
		for (unsigned __int8 iy = y - 1; iy >= 0; iy--)
		{		
			bool bottomLine = false;
			for (unsigned __int8 ix = left; ix <= right; ix++)
			{
				GetBlockInfo(ix, iy, z, pBlockInfo);
				if (!BlockMergeable(ix, iy, z, face, group, type, pBlockInfo))
				{
					bottomLine = true;
					break;
				}
			}
		
			if (bottomLine)
			{
				bottom = iy + 1;
				break;
			}

			EnterCriticalSection(&m_criticalSection);

			for (unsigned __int8 ix = left; ix <= right; ix++)
				pBlockInfo[_3dto1d(ix, iy, z)].info |= (2 * face);

			LeaveCriticalSection(&m_criticalSection);
		}
	}

	// top border
	unsigned __int8 top = m_size - 1;
	if (y != top)
	{
		for (unsigned __int8 iy = y + 1; iy < m_size; iy++)
		{
			bool topLine = false;
			for (unsigned __int8 ix = left; ix <= right; ix++)
			{
				GetBlockInfo(ix, iy, z, pBlockInfo);
				if (!BlockMergeable(ix, iy, z, face, group, type, pBlockInfo))
				{
					topLine = true;
					break;
				}
			}

			if (topLine)
			{
				top = iy - 1;
				break;
			}

			EnterCriticalSection(&m_criticalSection);

			for (unsigned __int8 ix = left; ix <= right; ix++)
				pBlockInfo[_3dto1d(ix, iy, z)].info |= (2 * face);

			LeaveCriticalSection(&m_criticalSection);
		}
	}
	
	// build corner points
	if (face == BLOCK_FACE_VISIBLE_FRONT)
	{
		pRect->corners[0] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f),  (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[1] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f),  (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[2] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f),  (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[3] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f),  (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->normalIndex = VERT_NORMAL_FRONT_INDEX;
	}
	else
	{
		pRect->corners[1] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[0] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[3] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[2] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->normalIndex = VERT_NORMAL_BACK_INDEX;
	}
	
	// build tex coords
	SetTextureCoordinates(x, y, z, right - left + 1, top - bottom + 1, type, pRect);
}
void Chunk::MergeYZ( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int16 face, unsigned __int16 group, BlockType& type, RECTANGLE* pRect, BLOCK_INFO* pBlockInfo )
{
	// left border
	unsigned __int8 left = 0;
	if (z != left)
	{
		for (unsigned __int8 iz = z - 1; iz >= 0; iz--)
		{
			GetBlockInfo(x, y, iz, pBlockInfo);
			if (!BlockMergeable(x, y, iz, face, group, type, pBlockInfo))
			{
				left = iz + 1;
				break;
			}

			EnterCriticalSection(&m_criticalSection);
			pBlockInfo[_3dto1d(x, y, iz)].info |= (2 * face);
			LeaveCriticalSection(&m_criticalSection);
		}
	}

	// right border
	unsigned __int8 right = m_size - 1;
	if (z != right)
	{
		for (unsigned __int8 iz = z + 1; iz < m_size; iz++)
		{
			GetBlockInfo(x, y, iz, pBlockInfo);
			if (!BlockMergeable(x, y, iz, face, group, type, pBlockInfo))
			{
				right = iz - 1;
				break;
			}

			EnterCriticalSection(&m_criticalSection);
			pBlockInfo[_3dto1d(x, y, iz)].info |= (2 * face);
			LeaveCriticalSection(&m_criticalSection);
		}
	}

	// bottom border
	unsigned __int8 bottom = 0;
	if (y != bottom)
	{
		for (unsigned __int8 iy = y - 1; iy >= 0; iy--)
		{
			bool bottomLine = false;
			for (unsigned __int8 iz = left; iz <= right; iz++)
			{
				GetBlockInfo(x, iy, iz, pBlockInfo);

				if (!BlockMergeable(x, iy, iz, face, group, type, pBlockInfo))
				{
					bottomLine = true;
					break;
				}
			}

			if (bottomLine)
			{
				bottom = iy + 1;
				break;
			}

			EnterCriticalSection(&m_criticalSection);
			for (unsigned __int8 iz = left; iz <= right; iz++)
				pBlockInfo[_3dto1d(x, iy, iz)].info |= (2 * face);
			LeaveCriticalSection(&m_criticalSection);
		}
	}
	
	// top border
	unsigned __int8 top = m_size - 1 ;
	if (y != top)
	{
		for (unsigned __int8 iy = y + 1; iy < m_size; iy++)
		{
			bool topLine = false;
			for (unsigned __int8 iz = left; iz <= right; iz++)
			{
				GetBlockInfo(x, iy, iz, pBlockInfo);
				if (!BlockMergeable(x, iy, iz, face, group, type, pBlockInfo) )
				{
					topLine = true;
					break;
				}
			}

			if (topLine)
			{
				top = iy - 1;
				break;
			}

			EnterCriticalSection(&m_criticalSection);
			for (unsigned __int8 iz = left; iz <= right; iz++)
				pBlockInfo[_3dto1d(x, iy, iz)].info |= (2 * face);
			LeaveCriticalSection(&m_criticalSection);
		}
	}

	// build corner points
	if (face == BLOCK_FACE_VISIBLE_LEFT)
	{
		pRect->corners[1] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + right * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[0] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + left  * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[3] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + left  * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[2] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + right * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->normalIndex = VERT_NORMAL_LEFT_INDEX;
	}
	else
	{
		pRect->corners[0] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + right * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[1] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + left  * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[2] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + left  * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[3] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + right * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->normalIndex = VERT_NORMAL_RIGHT_INDEX;
	}

	// build tex coords
	SetTextureCoordinates(x, y, z, right - left + 1, top - bottom + 1, type, pRect);
}
void Chunk::MergeXZ( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int16 face, unsigned __int16 group, BlockType& type, RECTANGLE* pRect, BLOCK_INFO* pBlockInfo )
{
	// left border
	unsigned __int8 left = 0;
	if (x != left)
	{
		for (unsigned __int8 ix = x - 1; ix >= 0; ix--)
		{
			GetBlockInfo(ix, y, z, pBlockInfo);
			if (!BlockMergeable(ix, y, z, face, group, type, pBlockInfo))
			{
				left = ix + 1;
				break;
			}

			EnterCriticalSection(&m_criticalSection);
			pBlockInfo[_3dto1d(ix, y, z)].info |= (2 * face);
			LeaveCriticalSection(&m_criticalSection);
		}
	}

	// right border
	unsigned __int8 right = m_size - 1;
	if (x != right)
	{
		for (unsigned __int8 ix = x + 1; ix < m_size; ix++)
		{
			GetBlockInfo(ix, y, z, pBlockInfo);
			if (!BlockMergeable(ix, y, z, face, group, type, pBlockInfo))
			{
				right = ix - 1;
				break;
			}

			EnterCriticalSection(&m_criticalSection);
			pBlockInfo[_3dto1d(ix, y, z)].info |= (2 * face);
			LeaveCriticalSection(&m_criticalSection);
		}
	}

	// bottom border
	unsigned __int8 bottom = 0;
	if (z != bottom)
	{
		for (unsigned __int8 iz = z - 1; iz >= 0; iz--)
		{
			bool bottomLine = false;
			for (unsigned __int8 ix = left; ix <= right; ix++)
			{
				GetBlockInfo(ix, y, iz, pBlockInfo);
				if (!BlockMergeable(ix, y, iz, face, group, type, pBlockInfo))
				{
					bottomLine = true;
					break;
				}
			}

			if (bottomLine)
			{
				bottom = iz + 1;
				break;
			}

			EnterCriticalSection(&m_criticalSection);
			for (unsigned __int8 ix = left; ix <= right; ix++)
				pBlockInfo[_3dto1d(ix, y, iz)].info |= (2 * face);
			LeaveCriticalSection(&m_criticalSection);
		}
	}

	// top border
	unsigned __int8 top = m_size - 1;
	if (z != top)
	{
		for (unsigned __int8 iz = z + 1; iz < m_size; iz++)
		{
			bool topLine = false;
			for (unsigned __int8 ix = left; ix <= right; ix++)
			{
				GetBlockInfo(ix, y, iz, pBlockInfo);
				if (!BlockMergeable(ix, y, iz, face, group, type, pBlockInfo))
				{
					topLine = true;
					break;
				}
			}

			if (topLine)
			{
				top = iz - 1;
				break;
			}

			EnterCriticalSection(&m_criticalSection);
			for (unsigned __int8 ix = left; ix <= right; ix++)
				pBlockInfo[_3dto1d(ix, y, iz)].info |= (2 * face);
			LeaveCriticalSection(&m_criticalSection);
		}
	}

	// build corner points
	if (face == BLOCK_FACE_VISIBLE_BOTTOM)
	{
		pRect->corners[3] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + bottom * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[2] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + bottom * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[1] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + top    * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[0] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + top    * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->normalIndex = VERT_NORMAL_DOWN_INDEX;
	}
	else
	{
		pRect->corners[0] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + bottom * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[1] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + bottom * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[2] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + top    * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[3] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + top    * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->normalIndex = VERT_NORMAL_UP_INDEX;
	}

	// build tex coords
	SetTextureCoordinates(x, y, z, right - left + 1, top - bottom + 1, type, pRect);
}
void Chunk::SetTextureCoordinates( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int8 width, unsigned __int8 height, BlockType& type, RECTANGLE* pRect )
{
	// if group is 0 -> tiling
	XMFLOAT4 texCoords;
	
	EnterCriticalSection(&m_criticalSection);
	if (m_pBlocks[_3dto1d(x, y, z)].Group() == 0)
	{
		texCoords = type.GetRectangleTexCoords(width, height);
	}
	else
	{
		texCoords = type.GetRectangleTexCoords(1, 1);
	}
	LeaveCriticalSection(&m_criticalSection);

	pRect->texCoords[0] = XMFLOAT2(texCoords.z, texCoords.w);
	pRect->texCoords[1] = XMFLOAT2(texCoords.x, texCoords.w);
	pRect->texCoords[2] = XMFLOAT2(texCoords.x, texCoords.y);
	pRect->texCoords[3] = XMFLOAT2(texCoords.z, texCoords.y);
}

void Chunk::Serialize( FILE* pFile )
{
	EnterCriticalSection(&m_criticalSection);
	fwrite(m_pBlocks, sizeof(Block), m_size * m_size * m_size, pFile);
	LeaveCriticalSection(&m_criticalSection);
}
bool Chunk::Deserialize( FILE* pFile )
{
	EnterCriticalSection(&m_criticalSection);
	fread(m_pBlocks, sizeof(Block), m_size * m_size * m_size, pFile);
	LeaveCriticalSection(&m_criticalSection);

	return true;
}

void cbe::Chunk::SetChunkChanged( bool changed )
{
	m_upToDate = false;
}
