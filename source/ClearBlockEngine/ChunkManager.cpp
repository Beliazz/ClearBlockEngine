#include "cbe.h"

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
	m_absoluteChunkSize = 50.0f;

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
	m_endEvent = CreateEvent(NULL, false, false, L"update asnyc end event");

	XMFLOAT4 normals[6];
	normals[VERT_NORMAL_FRONT_INDEX] = VERT_NORMAL_FRONT;
	normals[VERT_NORMAL_BACK_INDEX] = VERT_NORMAL_BACK;
	normals[VERT_NORMAL_LEFT_INDEX] = VERT_NORMAL_LEFT;
	normals[VERT_NORMAL_RIGHT_INDEX] = VERT_NORMAL_RIGHT;
	normals[VERT_NORMAL_DOWN_INDEX] = VERT_NORMAL_DOWN;
	normals[VERT_NORMAL_UP_INDEX] = VERT_NORMAL_UP;

	m_pNormals->get()->AsVector()->SetFloatVectorArray((float*)normals, 0, 6);
	m_upToDate = false;

	m_tsChunksToChangeIndices.set(new std::list<int>());
	m_tsChunksToUpdateIndices.set(new std::vector<int>());
	m_tsUpdateJobs.set(new std::vector<std::vector<UpdateJob>>());
	m_tsUpdateJobs->resize(m_width*m_height*m_width);

	return true;
}
void ChunkManager::StartAsyncUpdating()
{
	m_thread = CreateThread(NULL, NULL, UpdateAsync, this, NULL, NULL);
}

void ChunkManager::Exit()
{
	SetAsyncProccessing(false);
	SetEvent(m_startEvent);
	WaitForSingleObject(GetEndEvent(), INFINITE);

	m_processing = false;
	if(m_ppChunks)
	{
		for (int i = 0; i < m_width * m_height * m_depth; i++)
			SAFE_DELETE(m_ppChunks[i]);
		
		SAFE_DELETE(m_ppChunks);
	}

	SAFE_DELETE(m_pTypeMgr);

	DeleteCriticalSection(&m_criticalSection);
}

void ChunkManager::_setBlockState(int* chunkIndices, int* blockIndices, BOOL state )
{
	EnterCriticalSection(&m_criticalSection);

	CheckChunk(chunkIndices);
	m_ppChunks[chunkIndices[3]]->SetBlockState(blockIndices[3], state);
	ChunkChanged(chunkIndices, blockIndices);

	LeaveCriticalSection(&m_criticalSection);
}
void ChunkManager::_setBlockType( int* chunkIndices, int* blockIndices, USHORT type )
{
	EnterCriticalSection(&m_criticalSection);

	CheckChunk(chunkIndices);
	m_ppChunks[chunkIndices[3]]->SetBlockType(blockIndices[3], type);
	ChunkChanged(chunkIndices, blockIndices);

	LeaveCriticalSection(&m_criticalSection);
}
void ChunkManager::_setBlockGroup(int* chunkIndices, int* blockIndices, BYTE group )
{
	EnterCriticalSection(&m_criticalSection);

	CheckChunk(chunkIndices);
	m_ppChunks[chunkIndices[3]]->SetBlockGroup(blockIndices[3], group);
	ChunkChanged(chunkIndices, blockIndices);

	LeaveCriticalSection(&m_criticalSection);
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

	int chunkIndices[4];
	int blockIndices[4];
	bool state = false;

	if (TransformCoords(x, y, z, chunkIndices, blockIndices))
		state = m_ppChunks[chunkIndices[3]]->GetBlockState(blockIndices[3]);

	LeaveCriticalSection(&m_criticalSection);

	return state;
}
bool ChunkManager::TransformCoords( int x, int y, int z, int* pChunkIndex, int* pBlockIndex)
{
	EnterCriticalSection(&m_criticalSection);

	int width = m_width;
	int height = m_height;
	int depth = m_depth;
	int chunkSize = m_chunkSize;

	LeaveCriticalSection(&m_criticalSection);

	if (x < 0 || y < 0 || z < 0 ||
		x >= chunkSize * width || 
		y >= chunkSize * height ||
		z >= chunkSize * depth)
	{
		return false;
	}

	double tmp = (double)x / chunkSize;
	pChunkIndex[0] = (int)(tmp + 0.0000000000005);
	pBlockIndex[0] = x - pChunkIndex[0] * chunkSize;

	tmp  = (double)y / chunkSize;
	pChunkIndex[1] = (int)(tmp + 0.0000000000005);
	pBlockIndex[1] = y - pChunkIndex[1] * chunkSize;

	tmp  = (double)z / chunkSize;
	pChunkIndex[2] = (int)(tmp + 0.0000000000005);
	pBlockIndex[2] = z - pChunkIndex[2] * chunkSize; 

	pChunkIndex[3] = _3dto1d(pChunkIndex[0], pChunkIndex[1], pChunkIndex[2], width, height);
	pBlockIndex[3] = _3dto1d(pBlockIndex[0], pBlockIndex[1], pBlockIndex[2], chunkSize, chunkSize);

	// 	// create/init chunk if it doesn't exist
// 	// 
// 	// not the optimal place to do this but
// 	// much simpler than returning the chunk coordinates
// 	// ix, iy, iz
// 	float absoluteChunkSize = 20.0f;
// 	if(!m_ppChunks[pChunkIndex[3]] && build)
// 	{
// 		m_ppChunks[pChunkIndex[3]] = new Chunk(this, pChunkIndex[0], pChunkIndex[1], pChunkIndex[2], XMFLOAT3((float)(pChunkIndex[0] * (absoluteChunkSize)),
// 																											  (float)(pChunkIndex[1] * (absoluteChunkSize)), 
// 																											  (float)(pChunkIndex[2] * (absoluteChunkSize))), m_chunkSize, absoluteChunkSize / m_chunkSize, this);
// 		m_ppChunks[pChunkIndex[3]]->Init();
// 	}

	return true;
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

	UpdateNextChunk();
}
DWORD WINAPI ChunkManager::UpdateAsync( LPVOID data )
{
	ChunkManager* pManager = (ChunkManager*)data;
	while (pManager->IsAsyncProccessing())
	{
		pManager->ProcessPendingJobs();
		pManager->BuildNextChunk();
	}

	SetEvent(pManager->GetEndEvent());

	return 0;
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
void cbe::ChunkManager::SetAsyncProccessing(bool processing)
{
	EnterCriticalSection(&m_criticalSection); 
	m_processing = processing ; 
	LeaveCriticalSection(&m_criticalSection); 
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
					m_tsChunksToChangeIndices->push_back(_3dto1d(x, y, z, m_width, m_height));
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

HANDLE ChunkManager::GetStartEvent()
{
	HANDLE startEvent;
	EnterCriticalSection(&m_criticalSection);
	startEvent = m_startEvent;
	LeaveCriticalSection(&m_criticalSection);

	return startEvent;
}
HANDLE cbe::ChunkManager::GetEndEvent()
{
	HANDLE endEvent;
	EnterCriticalSection(&m_criticalSection);
	endEvent = m_endEvent;
	LeaveCriticalSection(&m_criticalSection);

	return endEvent;
}

BlockTypeManager* ChunkManager::TypeManager()
{
	EnterCriticalSection(&m_criticalSection);
	BlockTypeManager* pManager = m_pTypeMgr;
	LeaveCriticalSection(&m_criticalSection);

	return pManager;
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
	AddChangedChunk(pChunkIndices[3]);

	/*
	if (pBlockIndices[0] < 0)
	{
		if (pChunkIndices[0] - 1 >= 0)
		{
			AddChangedChunk(_3dto1d(pChunkIndices[0] - 1, pChunkIndices[1], pChunkIndices[2], m_width, m_height), false);
		}
	}
	else if (pBlockIndices[0] >= m_chunkSize)
	{
		if (pChunkIndices[0] + 1 < m_chunkSize)
		{
			AddChangedChunk(_3dto1d(pChunkIndices[0] + 1, pChunkIndices[1], pChunkIndices[2], m_width, m_height), false);
		}
	}

	if (pBlockIndices[1] < 0)
	{
		if (pChunkIndices[1] - 1 >= 0)
		{
			AddChangedChunk(_3dto1d(pChunkIndices[0], pChunkIndices[1] - 1, pChunkIndices[2], m_width, m_height), false);
		}
	}
	else if (pBlockIndices[1] >= m_chunkSize)
	{
		if (pChunkIndices[1] + 1 < m_chunkSize)
		{
			AddChangedChunk(_3dto1d(pChunkIndices[0], pChunkIndices[1] + 1, pChunkIndices[2], m_width, m_height), false);
		}
	}

	if (pBlockIndices[2] < 0)
	{
		if (pChunkIndices[2] - 1 >= 0)
		{
			AddChangedChunk(_3dto1d(pChunkIndices[0], pChunkIndices[1], pChunkIndices[2] - 1, m_width, m_height), false);
		}
	}
	else if (pBlockIndices[2] >= m_chunkSize)
	{
		if (pChunkIndices[2] + 1 < m_chunkSize)
		{
			AddChangedChunk(_3dto1d(pChunkIndices[0], pChunkIndices[1], pChunkIndices[2] + 1, m_width, m_height), false);
		}
	}
	*/

	m_upToDate = false;
}

void ChunkManager::SetBlockState( int x, int y, int z, BOOL state )
{
	UpdateJob job(JOB_TYPE_STATE, state);
	if (TransformCoords(x, y, z, job.chunkIndices, job.blockIndices))
		m_tsUpdateJobs->at(job.chunkIndices[3]).push_back(job);
}
void ChunkManager::SetBlockType( int x, int y, int z, BlockType& type )
{
	UpdateJob job(JOB_TYPE_BLOCKTYPE, type.Id());
	if (TransformCoords(x, y, z, job.chunkIndices, job.blockIndices))
		m_tsUpdateJobs->at(job.chunkIndices[3]).push_back(job);
}
void ChunkManager::SetBlockGroup( int x, int y, int z, BYTE group )
{
	UpdateJob job(JOB_TYPE_GROUP, group);
	if (TransformCoords(x, y, z, job.chunkIndices, job.blockIndices))
		m_tsUpdateJobs->at(job.chunkIndices[3]).push_back(job);
}

void cbe::ChunkManager::SetChunkChanged( int x, int y, int z, bool changed )
{
	double tmp = (double)x / m_chunkSize;
	int ix = (int)(tmp + 0.0000000000005);

	tmp  = (double)y / m_chunkSize;
	int iy = (int)(tmp + 0.0000000000005);

	tmp  = (double)z / m_chunkSize;
	int iz = (int)(tmp + 0.0000000000005);

	m_ppChunks[_3dto1d(ix, iy, iz, m_width, m_height)]->SetChunkChanged(true);

}

void cbe::ChunkManager::AddBuiltChunk( int index )
{
	auto sec = m_tsChunksToUpdateIndices.blockSecurity();

	bool found = false;
	for (auto it = m_tsChunksToUpdateIndices->begin(); it != m_tsChunksToUpdateIndices->end(); it++)
	{
		if (*it == index)
		{
			found = true;
			break;
		}
	}

	if (!found)
	{
		m_tsChunksToUpdateIndices->push_back(index);
	}
}
void cbe::ChunkManager::AddChangedChunk( int index , bool highPriority )
{
	auto sec = m_tsChunksToChangeIndices.blockSecurity();

	bool found = false;
	for (auto it = m_tsChunksToChangeIndices->begin(); it != m_tsChunksToChangeIndices->end(); it++)
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
			m_tsChunksToChangeIndices->push_front(index);
		}
		else
		{
			m_tsChunksToChangeIndices->push_back(index);
		}

	}
}

void cbe::ChunkManager::ProcessPendingJobs()
{
	std::vector<UpdateJob> jobs;
	int count = 0;
	{
		auto sec = m_tsUpdateJobs.blockSecurity();

		for (auto it = m_tsUpdateJobs->begin(); it != m_tsUpdateJobs->end(); it++)
		{
			if(!it->empty())
			{
				jobs = std::vector<UpdateJob>(it->begin(), it->end());
				it->clear();
				break;
			}
		}
	}

	for (auto it2 = jobs.begin(); it2 != jobs.end(); it2++)
	{
		UpdateJob currJob = *it2;
		switch(currJob.type)
		{
		case JOB_TYPE_STATE:		_setBlockState(currJob.chunkIndices, currJob.blockIndices, currJob.val); break;
		case JOB_TYPE_GROUP:		_setBlockGroup(currJob.chunkIndices, currJob.blockIndices, currJob.val); break;
		case JOB_TYPE_BLOCKTYPE:	_setBlockType( currJob.chunkIndices, currJob.blockIndices, currJob.val); break;
		}
	}
}
bool cbe::ChunkManager::UpdateNextChunk()
{
	auto sec = m_tsChunksToUpdateIndices.blockSecurity();

	bool update = false;
	if (!m_tsChunksToUpdateIndices->empty())
	{
		Chunk* pChunk = m_ppChunks[*m_tsChunksToUpdateIndices->begin()];

		if (pChunk)
		{
			update = pChunk->Update();
			if (!update)
			{
				m_tsChunksToUpdateIndices->push_back(*m_tsChunksToUpdateIndices->begin());
			}
		}
		
		m_tsChunksToUpdateIndices->erase(m_tsChunksToUpdateIndices->begin());
	}

	return update;
}
void cbe::ChunkManager::BuildNextChunk()
{
	m_tsChunksToChangeIndices.enterSecureMode();
	if (!m_tsChunksToChangeIndices->empty())
	{
		Chunk* pChunk = m_ppChunks[*m_tsChunksToChangeIndices->begin()];
		if (pChunk)
		{
			pChunk->Build(this);	
			AddBuiltChunk(*m_tsChunksToChangeIndices->begin());
		}
		
		m_tsChunksToChangeIndices->erase(m_tsChunksToChangeIndices->begin());
		m_tsChunksToChangeIndices.leaveSecureMode();
	}	
	else
	{
		m_tsChunksToChangeIndices.leaveSecureMode();
		WaitForSingleObject(GetStartEvent(), INFINITE);
	}
}

void cbe::ChunkManager::CheckChunk( int* pIndices )
{
	if (!m_ppChunks[pIndices[3]])
	{
		CreateChunk(pIndices[0], pIndices[1], pIndices[2]);
	}
}

