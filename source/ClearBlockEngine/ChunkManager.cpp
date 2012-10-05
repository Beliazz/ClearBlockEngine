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

bool ChunkManager::Init(int widht, int height, int depth, int chunkSize)
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

	m_ppChunks = new Chunk*[widht*height*depth];

	m_width = widht;
	m_height = height;
	m_depth = depth;

	for (int i = 0; i < m_width * m_height * m_depth; i++)
		m_ppChunks[i] = NULL;

	m_chunkSize = chunkSize;

	m_pBlockTypes = cgl::CD3D11EffectVariableFromSemantic::Create(m_pEffect, "BLOCKTYPES");
	if (!CGL_RESTORE(m_pBlockTypes))
		return false;

	m_pTextureAtlas = cgl::CD3D11EffectVariableFromSemantic::Create(m_pEffect, "TEXTUREATLAS");
	if (!CGL_RESTORE(m_pTextureAtlas))
		return false;

	m_pNormals = cgl::CD3D11EffectVariableFromSemantic::Create(m_pEffect, "BLOCKNORMALS");
	if (!CGL_RESTORE(m_pNormals))
		return false;

	m_pMatWorld = cgl::CD3D11EffectVariableFromSemantic::Create(m_pEffect, "WORLD");
	if (!CGL_RESTORE(m_pMatWorld))
		return false;

	InitializeCriticalSection(&m_criticalSection);
	m_startEvent = CreateEvent(NULL, false, false, L"update asnyc start event");

	XMFLOAT4 normals[6];
	normals[VERT_NORMAL_FRONT_INDEX] = VERT_NORMAL_FRONT;
	normals[VERT_NORMAL_BACK_INDEX] = VERT_NORMAL_BACK;
	normals[VERT_NORMAL_LEFT_INDEX] = VERT_NORMAL_LEFT;
	normals[VERT_NORMAL_RIGHT_INDEX] = VERT_NORMAL_RIGHT;
	normals[VERT_NORMAL_DOWN_INDEX] = VERT_NORMAL_DOWN;
	normals[VERT_NORMAL_UP_INDEX] = VERT_NORMAL_UP;

	m_pNormals->get()->AsVector()->SetFloatVectorArray((float*)normals, 0, 6);
	m_upToDate = false;

	return true;
}
void ChunkManager::StartAsyncUpdating()
{
	m_thread = CreateThread(NULL, NULL, UpdateAsync, this, NULL, NULL);
}

void ChunkManager::Exit()
{
	TerminateThread(m_thread, 0);
	SetEvent(m_startEvent);

	EnterCriticalSection(&m_criticalSection);
	m_processing = false;

	if(m_ppChunks)
	{
		for (int i = 0; i < m_width * m_height * m_depth; i++)
			SAFE_DELETE(m_ppChunks[i]);
		
		SAFE_DELETE(m_ppChunks);
	}

	SAFE_DELETE(m_pTypeMgr);

	LeaveCriticalSection(&m_criticalSection);

	DeleteCriticalSection(&m_criticalSection);
}

void ChunkManager::_setBlockState( int x, int y, int z, BOOL state )
{
	EnterCriticalSection(&m_criticalSection);

	int chunkIndex[4];
	chunkIndex[3] = -1;

	int blockIndex[4];
	blockIndex[3] = -1;
	
	TransformCoords(x, y, z, chunkIndex, blockIndex);
	if (chunkIndex[3] == -1 || blockIndex[3] == -1)
	{
		LeaveCriticalSection(&m_criticalSection);
		return;
	}

	m_ppChunks[chunkIndex[3]]->SetBlockState(blockIndex[3], state);

	m_upToDate = false;
	AddChunk(chunkIndex[3]);
	//ChunkChanged(chunkIndex, blockIndex);

	LeaveCriticalSection(&m_criticalSection);
}
void ChunkManager::_setBlockType( int x, int y, int z, USHORT type )
{
	EnterCriticalSection(&m_criticalSection);

	int chunkIndex[4];
	chunkIndex[3] = -1;

	int blockIndex[4];
	blockIndex[3] = -1;

	TransformCoords(x, y, z, chunkIndex, blockIndex);
	if (chunkIndex[3] == -1 || blockIndex[3] == -1)
	{
		LeaveCriticalSection(&m_criticalSection);
		return;
	}

	m_ppChunks[chunkIndex[3]]->SetBlockType(blockIndex[3], type);

	m_upToDate = false;
	AddChunk(chunkIndex[3]);
	//ChunkChanged(chunkIndex, blockIndex);

	LeaveCriticalSection(&m_criticalSection);
}
void ChunkManager::_setBlockGroup( int x, int y, int z, BYTE group )
{
	EnterCriticalSection(&m_criticalSection);

	int chunkIndex[4];
	chunkIndex[3] = -1;

	int blockIndex[4];
	blockIndex[3] = -1;

	TransformCoords(x, y, z, chunkIndex, blockIndex);

	if (chunkIndex[3] == -1 || blockIndex[3] == -1)
	{
		LeaveCriticalSection(&m_criticalSection);
		return;
	}

	m_ppChunks[chunkIndex[3]]->SetBlockGroup(blockIndex[3], group);

	m_upToDate = false;
	AddChunk(chunkIndex[3]);
	//ChunkChanged(chunkIndex, blockIndex);

	LeaveCriticalSection(&m_criticalSection);
}

void ChunkManager::Render()
{
	EnterCriticalSection(&m_criticalSection);

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

	LeaveCriticalSection(&m_criticalSection);
}
void ChunkManager::RenderBatched()
{
	EnterCriticalSection(&m_criticalSection);

	// these are always the same
	UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	ZeroMemory(strides, sizeof(UINT) * D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
	ZeroMemory(offsets, sizeof(UINT) * D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);

	UINT currPos = 0;
	ID3D11Buffer*** pppVertexBuffers = new ID3D11Buffer**[10];
	pppVertexBuffers[0] = new ID3D11Buffer*[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	
	UINT currBatch = 0;
	for (int i = 0; i < m_width * m_height * m_depth; i++)
	{
		Chunk* pChunk = m_ppChunks[i];
		if (pChunk)
		{
			pppVertexBuffers[currBatch][currPos] = pChunk->GetVertexBuffer()->get();
			currPos++;
			
			if (currPos >= D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
			{
				currPos = 0;
				currBatch++;
				pppVertexBuffers[currBatch] = new ID3D11Buffer*[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			}
		}
	}

	cgl::CGLManagerConnector conn;

	UINT lastBatch = currBatch;
	UINT lastVertexBufferCount = currPos;
	if (currPos == 0)
	{
		lastBatch--;
		lastVertexBufferCount = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
	}

	UINT vertexOffset = 0;
	currPos = 0;
	currBatch = 0;

	m_pInputLayout->Bind();
	
	for (UINT pass = 0; pass < m_techniques[0].passes.size(); pass++)
	{
		m_techniques[0].passes[pass]->Apply();

		if (lastBatch == 0)
		{
			conn.Context()->IASetVertexBuffers(0, lastVertexBufferCount, pppVertexBuffers[0], strides, offsets);
		}
		else
		{
			conn.Context()->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, pppVertexBuffers[0], strides, offsets);	
		}
		
		for (int i = 0; i < m_width * m_height * m_depth; i++)
		{
			if (currPos >= D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
			{
				if (currBatch == lastBatch)
				{
					conn.Context()->IASetVertexBuffers(0, lastVertexBufferCount, pppVertexBuffers[currBatch], strides, offsets);
				}
				else
				{
					conn.Context()->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, pppVertexBuffers[currBatch], strides, offsets);
				}
				
				currBatch++;
				currPos = 0;
			}

			Chunk* pChunk = m_ppChunks[i];
			if (pChunk)
			{
				pChunk->RenderBatched(&vertexOffset);
				currPos++;
			}
		}
	}

	for (UINT i = 0; i < lastBatch + 1; i++)
		SAFE_DELETE_ARRAY(pppVertexBuffers[i]);

	SAFE_DELETE_ARRAY(pppVertexBuffers);

	LeaveCriticalSection(&m_criticalSection);
}

bool ChunkManager::GetBlockState( int x, int y, int z )
{
	EnterCriticalSection(&m_criticalSection);

	int chunkIndex[4];
	chunkIndex[3] = -1;

	int blockIndex[4];
	blockIndex[3] = -1;

	TransformCoords(x, y, z, chunkIndex, blockIndex, false);

	if (chunkIndex[3] == -1 || blockIndex[3] == -1)
	{
		LeaveCriticalSection(&m_criticalSection);
		return false;
	}

	bool state = false;
	if(m_ppChunks[chunkIndex[3]])
	{
		state = m_ppChunks[chunkIndex[3]]->GetBlockState(blockIndex[3]);
	}

	LeaveCriticalSection(&m_criticalSection);

	return state;
}
void ChunkManager::TransformCoords( int x, int y, int z, int* pChunkIndex, int* pBlockIndex, bool build)
{
	if (x < 0 || y < 0 || z < 0 ||
		x >= m_chunkSize * m_width || 
		y >= m_chunkSize * m_height ||
		z >= m_chunkSize * m_depth)
	{
		return;
	}

	double tmp = (double)x / m_chunkSize;
	pChunkIndex[0] = (int)(tmp + 0.0000000000005);
	pBlockIndex[0] = x - pChunkIndex[0] * m_chunkSize;

	tmp  = (double)y / m_chunkSize;
	pChunkIndex[1] = (int)(tmp + 0.0000000000005);
	pBlockIndex[1] = y - pChunkIndex[1] * m_chunkSize;

	tmp  = (double)z / m_chunkSize;
	pChunkIndex[2] = (int)(tmp + 0.0000000000005);
	pBlockIndex[2] = z - pChunkIndex[2] * m_chunkSize; 

	pChunkIndex[3] = _3dto1d(pChunkIndex[0], pChunkIndex[1], pChunkIndex[2], m_width, m_height);
	pBlockIndex[3] = _3dto1d(pBlockIndex[0], pBlockIndex[1], pBlockIndex[2], m_chunkSize, m_chunkSize);

	// create/init chunk if it doesn't exist
	// 
	// not the optimal place to do this but
	// much simpler than returning the chunk coordinates
	// ix, iy, iz
	float absoluteChunkSize = 20.0f;
	if(!m_ppChunks[pChunkIndex[3]] && build)
	{
		m_ppChunks[pChunkIndex[3]] = new Chunk(this, pChunkIndex[0], pChunkIndex[1], pChunkIndex[2], XMFLOAT3((float)(pChunkIndex[0] * (absoluteChunkSize)),
																											  (float)(pChunkIndex[1] * (absoluteChunkSize)), 
																											  (float)(pChunkIndex[2] * (absoluteChunkSize))), m_chunkSize, absoluteChunkSize / m_chunkSize, this);
		m_ppChunks[pChunkIndex[3]]->Init();
	}
}

void ChunkManager::Update()
{

	SetEvent(m_startEvent);

	if (m_pTypeMgr->Update())
	{
		m_pTextureAtlas->get()->AsShaderResource()->SetResourceArray(m_pTypeMgr->GetAtlases()->get(), 0, m_pTypeMgr->GetAtlases()->GetCollectionSize());
		m_pBlockTypes->get()->SetRawValue(m_pTypeMgr->GetPackedTypes().data(), 0, sizeof(PackedBlockType) * m_pTypeMgr->GetPackedTypes().size());
	}

	m_pMatWorld->get()->AsMatrix()->SetMatrix((float*)&m_matWorld);


	Chunk* pChunk = GetNextChunkToProcess(false);
	if (pChunk)
	{
		GetNextChunkToProcess(pChunk->Update());
	}
}

int ChunkManager::GetActiveBlockCount()
{
	EnterCriticalSection(&m_criticalSection);

	int blockCount = 0;
	for (int i = 0; i < m_width * m_height * m_depth; i++)
	{
		Chunk* pChunk = m_ppChunks[i];
		if (pChunk)
			blockCount += pChunk->ActiveBlocks();
	}

	LeaveCriticalSection(&m_criticalSection);

	return blockCount;
}
int ChunkManager::GetVertexCount()
{
	EnterCriticalSection(&m_criticalSection);

	int vertexCount = 0;
	for (int i = 0; i < m_width * m_height * m_depth; i++)
	{
		Chunk* pChunk = m_ppChunks[i];
		if (pChunk)
			vertexCount += pChunk->VertexCount();
	}

	LeaveCriticalSection(&m_criticalSection);

	return vertexCount;
}
int ChunkManager::GetActiveChunkCount()
{
	EnterCriticalSection(&m_criticalSection);

	int chunkCount = 0;
	for (int i = 0; i < m_width * m_height * m_depth; i++)
	{
		if (m_ppChunks[i])
			chunkCount++;
	}

	LeaveCriticalSection(&m_criticalSection);

	return chunkCount;
}

bool ChunkManager::IsAsyncProccessing()
{
	bool processing; 
	
	EnterCriticalSection(&m_criticalSection); 
	processing = m_processing; 
	LeaveCriticalSection(&m_criticalSection); 
	
	return processing;
}

void ChunkManager::Serialize( std::string fileName )
{
	FILE* pFile = fopen(fileName.c_str(), "wb");
	if (!pFile)
		return;

	// write map info
	MapInfo mapInfo;
	mapInfo.depth = m_depth;
	mapInfo.width = m_width;
	mapInfo.height = m_height;
	mapInfo.chunkSize = m_chunkSize;

	fwrite(&mapInfo, sizeof(MapInfo), 1, pFile);

	for (int x = 0; x < m_width; x++)
	{
		for (int y = 0; y < m_height; y++)
		{
			for (int z = 0; z < m_depth; z++)
			{
				bool writing;
				if (m_ppChunks[_3dto1d(x, y, z, m_height, m_width)])
				{
					writing = true;
					fwrite(&writing, 1, 1, pFile);
					m_ppChunks[_3dto1d(x, y, z, m_height, m_width)]->Serialize(pFile);
				}
				else
				{
					writing = false;
					fwrite(&writing, 1, 1, pFile);
				}
			}
		}
	}

	fclose(pFile);
}
bool ChunkManager::Deserialize( std::string fileName )
{
	FILE* pFile = fopen(fileName.c_str(), "rb");
	if (!pFile)
		return false;

	MapInfo mapInfo;
	fread(&mapInfo, sizeof(MapInfo), 1, pFile);

	if(!Init(mapInfo.width, mapInfo.height, mapInfo.depth, mapInfo.chunkSize))
		return false;

	for (int x = 0; x < m_width; x++)
	{
		for (int y = 0; y < m_height; y++)
		{
			for (int z = 0; z < m_depth; z++)
			{
				bool exists = false;
				fread(&exists, 1, 1, pFile);

				if (exists)
				{
					EnterCriticalSection(&m_criticalSection);
					CreateChunk(x, y, z);
					m_ppChunks[_3dto1d(x, y, z, m_height, m_width)]->Deserialize(pFile);
					m_chunksToChangeIndices.push_back(_3dto1d(x, y, z, m_width, m_height));
					LeaveCriticalSection(&m_criticalSection);
				}

				
			}
		}
	}

	fclose(pFile);

	return true;
}

void ChunkManager::CreateChunk( int ix, int iy, int iz )
{
	m_ppChunks[_3dto1d(ix, iy, iz, m_width, m_height)] = new Chunk(this, ix, iy, iz, XMFLOAT3((float)(ix * (m_absoluteChunkSize)),
																							  (float)(iy * (m_absoluteChunkSize)), 
																							  (float)(iz * (m_absoluteChunkSize))), m_chunkSize, m_absoluteChunkSize / m_chunkSize, this);
	m_ppChunks[_3dto1d(ix, iy, iz, m_width, m_height)]->Init();
}
Chunk* ChunkManager::GetNextChunkToProcess( bool del )
{
	EnterCriticalSection(&m_criticalSection);

	Chunk* pChunk = NULL;
	if (!m_chunksToChangeIndices.empty())
	{
		pChunk = GetChunk(*m_chunksToChangeIndices.begin());
		if (del || pChunk == NULL)
		{
			m_chunksToChangeIndices.remove(*m_chunksToChangeIndices.begin());
		}
	}

	LeaveCriticalSection(&m_criticalSection);

	return pChunk;
}

DWORD WINAPI ChunkManager::UpdateAsync( LPVOID data )
{
	ChunkManager* pManager = (ChunkManager*)data;
	while (pManager->IsAsyncProccessing())
	{
		pManager->ProcessPendingJobs();

		Chunk* pChunk = pManager->GetNextChunkToProcess(false);
		if (!pChunk)
		{
			WaitForSingleObject(pManager->GetStartEvent(), INFINITE);
		}
		else
		{
			pChunk->Build(pManager);
		}
	}

	return 0;
}

HANDLE ChunkManager::GetStartEvent()
{
	HANDLE startEvent;
	EnterCriticalSection(&m_criticalSection);
	startEvent = m_startEvent;
	LeaveCriticalSection(&m_criticalSection);

	return startEvent;
}

BlockTypeManager* ChunkManager::TypeManager()
{
	EnterCriticalSection(&m_criticalSection);
	BlockTypeManager* pManager = m_pTypeMgr;
	LeaveCriticalSection(&m_criticalSection);

	return pManager;
}

void ChunkManager::AddChunk( int index , bool highPriority )
{
	EnterCriticalSection(&m_criticalSection);

	bool found = false;
	for (auto it = m_chunksToChangeIndices.begin(); it != m_chunksToChangeIndices.end(); it++)
	{
		if (*it == index)
		{
			found = true;
			break;
		}
	}

	if (!found)
	{
		if (highPriority)
		{
			m_chunksToChangeIndices.push_front(index);
		}
		else
		{
			m_chunksToChangeIndices.push_back(index);
		}
		
	}

	LeaveCriticalSection(&m_criticalSection);
}

void ChunkManager::SetWorldMatrix( float* pMat )
{
	SetWorldMatrix(XMFLOAT4X4(pMat));
}
void ChunkManager::SetWorldMatrix( CXMMATRIX mat )
{
	XMFLOAT4X4 tmp;
	XMStoreFloat4x4(&tmp, mat);
	SetWorldMatrix(tmp);
}
void ChunkManager::SetWorldMatrix( XMFLOAT4X4 mat )
{
	m_matWorld = mat;

	XMMATRIX tmp = XMLoadFloat4x4(&mat);
	XMVECTOR vec;
	XMMATRIX inverse = XMMatrixInverse(&vec, tmp);

	XMStoreFloat4x4(&m_matWorldInverse, inverse);
}

void ChunkManager::ChunkChanged( int* pChunkIndices, int* pBlockIndices )
{
	if (pBlockIndices[0] < 0)
	{
		if (pChunkIndices[0] - 1 >= 0)
		{
			AddChunk(_3dto1d(pChunkIndices[0] - 1, pChunkIndices[1], pChunkIndices[2], m_width, m_height), false);
		}
	}
	else if (pBlockIndices[0] >= m_chunkSize)
	{
		if (pChunkIndices[0] + 1 < m_chunkSize)
		{
			AddChunk(_3dto1d(pChunkIndices[0] + 1, pChunkIndices[1], pChunkIndices[2], m_width, m_height), false);
		}
	}

	if (pBlockIndices[1] < 0)
	{
		if (pChunkIndices[1] - 1 >= 0)
		{
			AddChunk(_3dto1d(pChunkIndices[0], pChunkIndices[1] - 1, pChunkIndices[2], m_width, m_height), false);
		}
	}
	else if (pBlockIndices[1] >= m_chunkSize)
	{
		if (pChunkIndices[1] + 1 < m_chunkSize)
		{
			AddChunk(_3dto1d(pChunkIndices[0], pChunkIndices[1] + 1, pChunkIndices[2], m_width, m_height), false);
		}
	}

	if (pBlockIndices[2] < 0)
	{
		if (pChunkIndices[2] + 1 >= 0)
		{
			AddChunk(_3dto1d(pChunkIndices[0], pChunkIndices[1], pChunkIndices[2] - 1, m_width, m_height), false);
		}
	}
	else if (pBlockIndices[2] >= m_chunkSize)
	{
		if (pChunkIndices[2] + 1 < m_chunkSize)
		{
			AddChunk(_3dto1d(pChunkIndices[0], pChunkIndices[1], pChunkIndices[2] + 1, m_width, m_height), false);
		}
	}
}

void ChunkManager::SetBlockState( int x, int y, int z, BOOL state )
{
	EnterCriticalSection(&m_criticalSection);
	m_updateJobs.push_back(UpdateJobs(x, y, z, JOB_TYPE_STATE, state));
	LeaveCriticalSection(&m_criticalSection);
}
void ChunkManager::SetBlockType( int x, int y, int z, BlockType& type )
{
	EnterCriticalSection(&m_criticalSection);
	m_updateJobs.push_back(UpdateJobs(x, y, z, JOB_TYPE_BLOCKTYPE, type.Id()));
	LeaveCriticalSection(&m_criticalSection);
}
void ChunkManager::SetBlockGroup( int x, int y, int z, BYTE group )
{
	EnterCriticalSection(&m_criticalSection);
	m_updateJobs.push_back(UpdateJobs(x, y, z, JOB_TYPE_GROUP, group));
	LeaveCriticalSection(&m_criticalSection);
}

void ChunkManager::ProcessPendingJobs()
{
	EnterCriticalSection(&m_criticalSection);
	int count = m_updateJobs.size();
	if (count == 0)
	{
		LeaveCriticalSection(&m_criticalSection);
		return;
	}

	UpdateJobs* pJobs = (UpdateJobs*)malloc(count * sizeof(UpdateJobs));
	memcpy(pJobs, m_updateJobs.data(), count * sizeof(UpdateJobs));
	m_updateJobs.clear();
	LeaveCriticalSection(&m_criticalSection);

	for (int i = 0; i < count; i++)
	{
		switch(pJobs[i].type)
		{
		case JOB_TYPE_STATE:		_setBlockState(pJobs[i].indices[0], pJobs[i].indices[1], pJobs[i].indices[2], pJobs->val); break;
		case JOB_TYPE_GROUP:		_setBlockGroup(pJobs[i].indices[0], pJobs[i].indices[1], pJobs[i].indices[2], pJobs->val); break;
		case JOB_TYPE_BLOCKTYPE:	_setBlockType(pJobs[i].indices[0], pJobs[i].indices[1], pJobs[i].indices[2], pJobs->val); break;
		}
	}

	SAFE_DELETE(pJobs);
}


