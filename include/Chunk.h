#pragma once

#include "cbe.h"
#include "ChunkManager.h"
#include "BlockTypeManager.h"
#include "Block.h"

namespace cbe {

#pragma pack(push, 1) 
struct BlockVertex
{
	XMFLOAT4 pos;
	XMFLOAT2 texCoord;
	int	indices[2];
};
#pragma pack(pop)

#define VERT_INDEX_TYPE 0
#define VERT_INDEX_NORMAL 1

#define VERT_NORMAL_FRONT	XMFLOAT4( 0.0f, 0.0f,-1.0f, 0.0f)
#define VERT_NORMAL_FRONT_INDEX 0

#define VERT_NORMAL_BACK		XMFLOAT4( 0.0f, 0.0f, 1.0f, 0.0f)
#define VERT_NORMAL_BACK_INDEX 1

#define VERT_NORMAL_RIGHT	XMFLOAT4( 1.0f, 0.0f, 0.0f, 0.0f)
#define VERT_NORMAL_RIGHT_INDEX 2

#define VERT_NORMAL_LEFT		XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f)
#define VERT_NORMAL_LEFT_INDEX 3

#define VERT_NORMAL_UP		XMFLOAT4( 0.0f, 1.0f, 0.0f, 0.0f)
#define VERT_NORMAL_UP_INDEX 4

#define VERT_NORMAL_DOWN		XMFLOAT4( 0.0f,-1.0f, 0.0f, 0.0f)
#define VERT_NORMAL_DOWN_INDEX 5

class ChunkManager;
class CBE_API Chunk
{
private:
	XMFLOAT3	m_vecPos;
	int m_ix;
	int m_iy;
	int m_iz;

	bool m_upToDate;

	ChunkManager* m_pManager;

	Block* m_pBlocks;
	unsigned __int8 m_size;
	float m_blockSize;

	// d3d buffer
	cgl::PD3D11VertexBuffer		m_pVertexBuffer;
	cgl::PD3D11IndexBuffer		m_pIndexBuffer;

	// data generation
	std::vector<BlockVertex> m_pendingVertices;
	std::vector<DWORD>		 m_pendingIndices;
	UINT m_numVertices;
	UINT m_numIndices;
	UINT m_numTris;
	UINT m_numActiveBlocks;
	UINT m_numBlocksVisible;

	// thread safety
	HANDLE m_thread;
	CRITICAL_SECTION m_criticalSection;
	bool m_building;
	
	struct BLOCK_INFO
	{
		unsigned __int16 info;	
	};

	#define BLOCK_FACE_VISIBLE_FRONT	1 << 0 // 1 
	#define BLOCK_FACE_USED_FRONT		1 << 1 // 2

	#define BLOCK_FACE_VISIBLE_BACK		1 << 2 // 4
	#define BLOCK_FACE_USED_BACK		1 << 3 // 8

	#define BLOCK_FACE_VISIBLE_LEFT		1 << 4 // 16
	#define BLOCK_FACE_USED_LEFT		1 << 5 // 32

	#define BLOCK_FACE_VISIBLE_RIGHT	1 << 6 // 64
	#define BLOCK_FACE_USED_RIGHT		1 << 7 // 128

	#define BLOCK_FACE_VISIBLE_BOTTOM	1 << 8 // 256
	#define BLOCK_FACE_USED_BOTTOM		1 << 9 // 512

	#define BLOCK_FACE_VISIBLE_TOP		1 << 10 // 1024
	#define BLOCK_FACE_USED_TOP			1 << 11 // 2048

	#define BLOCK_CHECKED				1 << 12 // 4096
	#define BLOCK_VISIBLE				1 << 13 // 8192
	#define BLOCK_ACTIVE			    1 << 14 // 16384

	#define BLOCK_FACE_COUNT			BLOCK_FACE_VISIBLE_TOP

	#pragma pack (push, 1)
	struct RECTANGLE
	{
		
// 		[0] lower right
// 		[1] lower left	
// 		[2] upper left
// 		[3] upper right
		XMFLOAT4 corners[4];
		XMFLOAT2 texCoords[4];

		int normalIndex;
		char unused[4];
	};
	#pragma pack(pop)
	
	inline UINT _3dto1d(unsigned __int8 x, unsigned __int8 y, unsigned __int8 z) { return z + y * m_size + x * m_size * m_size; }

	// check block info
	void GetBlockInfo( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, BLOCK_INFO* pBlockInfo);
	inline bool BlockMergeable( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int16 face, unsigned __int16 group, BlockType& type, BLOCK_INFO* pBlockInfo)
	{
		return ( pBlockInfo[_3dto1d(x, y, z)].info & face &&				// has this face visible
				(pBlockInfo[_3dto1d(x, y, z)].info & (2 * face)) == 0 &&	// this face hasn't been already used
				 m_pBlocks[_3dto1d(x, y, z)].Type() == type.Id() &&			// it has the same type
				 m_pBlocks[_3dto1d(x, y, z)].Group() == group);				// has the same group
	}

	// get rectangles
	unsigned __int8 Merge ( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int16 group, BlockType& type,  RECTANGLE* pRect, BLOCK_INFO* pBlockInfo );
	void MergeXY( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int16 face, unsigned __int16 group, BlockType& type, RECTANGLE* pRect, BLOCK_INFO* pBlockInfo );
	void MergeYZ( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int16 face, unsigned __int16 group, BlockType& type, RECTANGLE* pRect, BLOCK_INFO* pBlockInfo );
	void MergeXZ( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int16 face, unsigned __int16 group, BlockType& type, RECTANGLE* pRect, BLOCK_INFO* pBlockInfo );

	void SetTextureCoordinates( unsigned __int8 x, unsigned __int8 y, unsigned __int8 z, unsigned __int8 width, unsigned __int8 height, BlockType&, RECTANGLE* pRect );
		

public:
	Chunk(ChunkManager* pManager, int ix, int iy, int iz,XMFLOAT3 pos, unsigned __int8 chunkSize, float blockSize, ChunkManager* pMgr);
	~Chunk(void);

	bool Init();
	bool Update();
	void Render();
	void RenderBatched(UINT* pOffset);
	bool Build(ChunkManager* pMgr);
	bool BuildIt(ChunkManager* pMgr);
	
	bool GetBlockState(int x, int y, int z);
	bool GetBlockState(int index);

	void SetBlockState(int x, int y, int z, BOOL state);
	void SetBlockState(int index, BOOL state);

	unsigned __int16 GetBlockType(int x, int y, int z);
	void SetBlockType(int x, int y, int z,  unsigned __int16 type);
	void SetBlockType(int index, unsigned __int16 type);
	void SetBlockGroup(int index, BYTE group);

	inline UINT ActiveBlocks()	{ return m_numActiveBlocks; }
	inline UINT VisibleBlocks() { return m_numBlocksVisible; }
	inline UINT VertexCount()	{ return m_numVertices; }
	inline UINT IndexCount()	{ return m_numIndices; }
	inline UINT TriangleCount() { return m_numTris; }
	inline cgl::PD3D11VertexBuffer& GetVertexBuffer() { return m_pVertexBuffer; }

	void Serialize(FILE* pFile);
	bool Deserialize(FILE* pFile);
};

}