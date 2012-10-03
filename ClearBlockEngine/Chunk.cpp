#include "Chunk.h"

using namespace cbe;

//////////////////////////////////////////////////////////////////////////
// chunk triangle merged
// 
Chunk::Chunk(XMFLOAT3 pos, unsigned __int8 chunkSize, float blockSize, ChunkManager* pMgr )
	: m_vecPos(pos), m_size(chunkSize), m_blockSize(blockSize)
{
	m_pBlocks = new Block[chunkSize * chunkSize * chunkSize];
}
Chunk::~Chunk( void )
{
	SAFE_DELETE_ARRAY(m_pBlocks);

	m_pVertexBuffer->ResetData();
	m_pIndexBuffer->ResetData();
}

void Chunk::SetBlockState( int x, int y, int z, bool state )
{
	m_pBlocks[_3dto1d(x, y, z)].SetActive(state);
}
void Chunk::SetBlockState( int index, bool state )
{
	m_pBlocks[index].SetActive(state);
}
bool Chunk::GetBlockState( int x, int y, int z )
{
	if ( x < 0 || x >= m_size ||
		 y < 0 || y >= m_size ||
		 z < 0 || z >= m_size)
	{
		return false;
	}

	return m_pBlocks[_3dto1d(x, y, z)].Active();
}

void Chunk::SetBlockType( int x, int y, int z, unsigned __int16 type )
{
	m_pBlocks[_3dto1d(x, y, z)].SetType(type);
}
void Chunk::SetBlockType( int index, unsigned __int16 type )
{
	m_pBlocks[index].SetType(type);
}
unsigned __int16 Chunk::GetBlockType( int x, int y, int z )
{
	return m_pBlocks[z + y * m_size + x * m_size * m_size].Type();
}

void Chunk::SetBlockGroup( int index, BYTE group )
{
	m_pBlocks[index].SetGroup(group);
}

bool Chunk::Init()
{
	m_pIndexBuffer = cgl::CD3D11IndexBuffer::Create(sizeof(DWORD), D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	m_pVertexBuffer = cgl::CD3D11VertexBuffer::Create(sizeof(VertexNormalTextureColor), D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	return true;
}
void Chunk::Render()
{
	m_pVertexBuffer->Bind();
	m_pIndexBuffer->Bind();
	m_pIndexBuffer->Draw();
}
bool Chunk::Build(ChunkManager* pMgr)
{
	m_numActiveBlocks = 0;
	m_numBlocksVisible = 0;
	m_numindices = 0;
	m_numTris = 0;
	m_numVertices = 0;

	m_pVertexBuffer->ResetData();
	m_pIndexBuffer->ResetData();

	BLOCK_INFO* pBlockInfo = new BLOCK_INFO[m_size * m_size * m_size];
	ZeroMemory(pBlockInfo, m_size * m_size * m_size * sizeof(BLOCK_INFO));

	BlockTypeManager* pTypeMgr = pMgr->TypeManager();

	for (int z = 0; z < m_size; z++)
	{
		for (int y = 0; y < m_size; y++)
		{
			for (int x = 0; x < m_size; x++)
			{
				GetBlockInfo(x, y, z, pBlockInfo);
				if(pBlockInfo[_3dto1d(x, y, z)].info & BLOCK_VISIBLE)
				{
					m_numBlocksVisible++;

					unsigned __int16 blockTypeIndex = m_pBlocks[_3dto1d(x, y, z)].Type();
					unsigned __int16 blockTypeGroup = m_pBlocks[_3dto1d(x, y, z)].Group();

					BlockType* type = pTypeMgr->GetType(blockTypeIndex);
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

					VertexNormalTextureColor* pVerts = new VertexNormalTextureColor[numRects * 4];
					DWORD* pIndices = new DWORD[numRects * 6];
					DWORD baseIndices[6] = { 0, 1, 2, 0, 2, 3};
					for (UINT rect = 0; rect < numRects; rect++)
					{
						// Vertices
						for (int i = 0; i < 4; i++)
						{
							pVerts[rect * 4 + i].typeIndex = type->Id();
							pVerts[rect * 4 + i].pos = rects[rect].corners[i];
							pVerts[rect * 4 + i].normal = rects[rect].normal;
							pVerts[rect * 4 + i].color = type->Color();
							pVerts[rect * 4 + i].texCoord = rects[rect].texCoords[i];
							pVerts[rect * 4 + i].baseTexCoord = type->TexCoords();
							pVerts[rect * 4 + i].relTexSize = XMFLOAT2( type->RelTexSize(),
																		type->RelTexSize());
						}

						// indices
						for (int i = 0; i < 6; i++)
							pIndices[rect * 6 + i] = m_numVertices + rect * 4 + baseIndices[i];
					}

					m_pIndexBuffer->AddData((char*)pIndices, numRects * 6);
					m_pVertexBuffer->AddData((char*)pVerts, numRects * 4);

					SAFE_DELETE_ARRAY(pVerts);
					SAFE_DELETE_ARRAY(pIndices);

					if (invalidType)
						SAFE_DELETE(type);

					m_numVertices += numRects * 4;
					m_numindices += numRects * 6;
					m_numTris += numRects * 2;
				}
			}
		}
	}

	SAFE_DELETE_ARRAY(pBlockInfo);

	
 	if (m_numTris > 0)
 	{
		if (!m_pVertexBuffer->Update() || 
			!m_pIndexBuffer->Update() )
			return false;
	}

	return true;
}

void Chunk::GetBlockInfo( __in unsigned __int8 x, __in unsigned __int8 y, __in unsigned __int8 z, __inout BLOCK_INFO* pBlockInfo )
{
	if (x < 0 || x >= m_size ||
		y < 0 || y >= m_size ||
		z < 0 || z >= m_size ||
		pBlockInfo[_3dto1d(x, y, z)].info & BLOCK_CHECKED)
	{
		return;
	}

	if (!GetBlockState(x, y, z))
	{
		pBlockInfo[_3dto1d(x, y, z)].info = 0;
		pBlockInfo[_3dto1d(x, y, z)].info |= BLOCK_CHECKED;

		return;
	}

	pBlockInfo[_3dto1d(x, y, z)].info |= BLOCK_ACTIVE;
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

		if (GetBlockState(ix, iy, iz) == false)
		{
			pBlockInfo[_3dto1d(x, y, z)].info |= face;
			numFacesVisible++;
		}
	}

	if (numFacesVisible > 0)
	{
		pBlockInfo[_3dto1d(x, y, z)].info |= BLOCK_VISIBLE;
	}

	pBlockInfo[_3dto1d(x, y, z)].info |= BLOCK_CHECKED;
}

unsigned __int8 Chunk::Merge( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int16 group, BlockType& type, RECTANGLE* pRect, BLOCK_INFO* pBlockInfo )
{
	unsigned __int8 numRects = 0;
	for ( unsigned __int16 face = 1; face <= BLOCK_FACE_COUNT; face *= 4 )
	{
 		if (pBlockInfo[_3dto1d(x, y, z)].info & face  && 
 		   (pBlockInfo[_3dto1d(x, y, z)].info & (2 * face)) != 2 * face)
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

			pBlockInfo[_3dto1d(ix, y, z)].info |= (2 * face);
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

			pBlockInfo[_3dto1d(ix, y, z)].info |= (2 * face);
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

			for (unsigned __int8 ix = left; ix <= right; ix++)
				pBlockInfo[_3dto1d(ix, iy, z)].info |= (2 * face);
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

			for (unsigned __int8 ix = left; ix <= right; ix++)
				pBlockInfo[_3dto1d(ix, iy, z)].info |= (2 * face);
		}
	}
	
	// build corner points
	if (face == BLOCK_FACE_VISIBLE_FRONT)
	{
		pRect->corners[0] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f),  (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[1] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f),  (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[2] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f),  (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[3] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f),  (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->normal = VEC_NORMAL_FRONT;
	}
	else
	{
		pRect->corners[1] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[0] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[3] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[2] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + z * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->normal = VEC_NORMAL_BACK;
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

			pBlockInfo[_3dto1d(x, y, iz)].info |= (2 * face);
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

			pBlockInfo[_3dto1d(x, y, iz)].info |= (2 * face);
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

			for (unsigned __int8 iz = left; iz <= right; iz++)
				pBlockInfo[_3dto1d(x, iy, iz)].info |= (2 * face);
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

			for (unsigned __int8 iz = left; iz <= right; iz++)
				pBlockInfo[_3dto1d(x, iy, iz)].info |= (2 * face);
		}
	}

	// build corner points
	if (face == BLOCK_FACE_VISIBLE_LEFT)
	{
		pRect->corners[1] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + right * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[0] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + left  * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[3] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + left  * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[2] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + right * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->normal = VEC_NORMAL_LEFT;
	}
	else
	{
		pRect->corners[0] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + right * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[1] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + bottom * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + left  * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[2] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + left  * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[3] = XMFLOAT4((float)(m_vecPos.x + x * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + top    * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + right * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->normal = VEC_NORMAL_RIGHT;
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

			pBlockInfo[_3dto1d(ix, y, z)].info |= (2 * face);
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

			pBlockInfo[_3dto1d(ix, y, z)].info |= (2 * face);
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

			for (unsigned __int8 ix = left; ix <= right; ix++)
				pBlockInfo[_3dto1d(ix, y, iz)].info |= (2 * face);
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

			for (unsigned __int8 ix = left; ix <= right; ix++)
				pBlockInfo[_3dto1d(ix, y, iz)].info |= (2 * face);
		}
	}

	// build corner points
	if (face == BLOCK_FACE_VISIBLE_BOTTOM)
	{
		pRect->corners[3] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + bottom * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[2] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + bottom * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[1] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + top    * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[0] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.z + top    * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->normal = VEC_NORMAL_DOWN;
	}
	else
	{
		pRect->corners[0] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + bottom * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[1] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + bottom * m_blockSize - m_blockSize / 2.0f), 1.0f);
		pRect->corners[2] = XMFLOAT4((float)(m_vecPos.x + left  * m_blockSize - m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + top    * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->corners[3] = XMFLOAT4((float)(m_vecPos.x + right * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.y + y * m_blockSize + m_blockSize / 2.0f), (float)(m_vecPos.z + top    * m_blockSize + m_blockSize / 2.0f), 1.0f);
		pRect->normal = VEC_NORMAL_UP;
	}

	// build tex coords
	SetTextureCoordinates(x, y, z, right - left + 1, top - bottom + 1, type, pRect);
}
void Chunk::SetTextureCoordinates( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int8 width, unsigned __int8 height, BlockType& type, RECTANGLE* pRect )
{
	// if group is 0 -> tiling
	XMFLOAT4 texCoords;
	if (m_pBlocks[_3dto1d(x, y, z)].Group() == 0)
	{
		texCoords = type.GetRectangleTexCoords(width, height);
	}
	else
	{
		texCoords = type.GetRectangleTexCoords(1, 1);
	}

	pRect->texCoords[0] = XMFLOAT2(texCoords.z, texCoords.w);
	pRect->texCoords[1] = XMFLOAT2(texCoords.x, texCoords.w);
	pRect->texCoords[2] = XMFLOAT2(texCoords.x, texCoords.y);
	pRect->texCoords[3] = XMFLOAT2(texCoords.z, texCoords.y);
}









