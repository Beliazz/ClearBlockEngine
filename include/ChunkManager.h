#pragma once

#include "BlockTypeManager.h"
#include "BlockType.h"
#include "Chunk.h"
#include "cbe.h"
#include <cmath>

namespace cbe {

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
	float m_absoluteChunkSize;

	XMFLOAT4X4 m_matWorld;
	XMFLOAT4X4 m_matWorldInverse;
	cgl::PD3D11EffectVariable	m_pMatWorld;

	bool m_upToDate;
	
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

	cgl::PD3D11EffectVariable	m_pNormals;
	cgl::PD3D11EffectVariable	m_pBlockTypes;
	cgl::PD3D11EffectVariable	m_pTextureAtlas;


	// threading
	enum JOB_TYPE
	{
		JOB_TYPE_STATE,
		JOB_TYPE_GROUP,
		JOB_TYPE_BLOCKTYPE
	};

	#pragma pack(push, 1)
	struct UpdateJob
	{
		int chunkIndices[4];
		int blockIndices[4];

		JOB_TYPE type;
		int val;

		UpdateJob(JOB_TYPE _type, int _val)
			: type(_type), val(_val)
		{		}
	};
	#pragma pack(pop)
	
	CRITICAL_SECTION m_criticalSection;
	HANDLE m_thread;
	HANDLE m_startEvent;
	ThreadSafe<std::list<int>>						m_tsChunksToChangeIndices;
	ThreadSafe<std::vector<int>>					m_tsChunksToUpdateIndices;
	ThreadSafe<std::vector<std::vector<UpdateJob>>>	m_tsUpdateJobs;
	bool m_processing;

	inline Chunk* GetChunk(int index) { return m_ppChunks[index]; }
	HANDLE GetStartEvent();

	void BuildNextChunk();
	bool UpdateNextChunk();
	static DWORD WINAPI UpdateAsync(LPVOID data);
	
	bool IsAsyncProccessing();

	inline int _3dto1d(unsigned __int16 x, unsigned __int16 y, unsigned __int16 z, unsigned __int16 width, unsigned __int16 height) { return z + y * width + x * height * width; }
	bool TransformCoords(int x, int y, int z, int* pChunkIndex,int* pBlockIndex);
	bool CreateChunk(int chunkIndex, int x, int y, int z );
	void CheckChunk(int* pIndices);

	#pragma pack (push, 1)
	struct MapInfo
	{
		__int32 width;
		__int32 height;
		__int32 depth;
		__int32 chunkSize;
	};
	#pragma pack (pop)

	void AddChangedChunk(int index, bool highPriority = false);
	void AddBuiltChunk(int index);
	void CreateChunk(int ix, int iy, int iz);
	void RenderBatched();
	void ChunkChanged(int* pChunkIndices, int* pBlockIndices);
	void ProcessPendingJobs();

	// synchronized access
	void _setBlockState(int* chunkIndices, int* blockIndices, BOOL state);
	void _setBlockType( int* chunkIndices, int* blockIndices, USHORT type);
	void _setBlockGroup(int* chunkIndices, int* blockIndices, BYTE group);

public:
	ChunkManager(cgl::PD3D11Effect pEffect);
	~ChunkManager(void);

	bool Init(int widht, int height, int depth, int chunkSize);
	void Render();
	void Update();
	void Exit();

	void Serialize(std::string fileName);
	bool Deserialize(std::string fileName);

	void SetBlockState(int x, int y, int z, BOOL state);
	void SetBlockType(int x, int y, int z, BlockType& type);
	void SetBlockGroup(int x, int y, int z, BYTE group);
	void SetChunkChanged(int x, int y, int z, bool changed);

	void SetWorldMatrix(float* pMat);
	void SetWorldMatrix(CXMMATRIX mat);
	void SetWorldMatrix(XMFLOAT4X4 mat);

	bool GetBlockState(int x, int y, int z);
	int GetActiveChunkCount();
	int GetActiveBlockCount();
	int GetVertexCount();

	void StartAsyncUpdating();

	inline float Width()	{ return m_absoluteChunkSize * m_width;	 }
	inline float Height()	{ return m_absoluteChunkSize * m_height; }
	inline float Depth()	{ return m_absoluteChunkSize * m_depth;  }

	BlockTypeManager* TypeManager();
};

}