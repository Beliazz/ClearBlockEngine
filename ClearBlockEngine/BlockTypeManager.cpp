#include "BlockTypeManager.h"

using namespace cbe;

void safeDelete(BlockType* pType)
{
	SAFE_DELETE(pType);
}

BlockTypeManager::BlockTypeManager( UINT texSize )
	: m_upToDate(false)
{
	float root = log((float)texSize) / log(2.0f);
	if ( root - (int)root != 0 )
		throw ("texture size must be power of 2");

	m_texSize = texSize;

	m_pViewCollection = cgl::CGLRenderTargetViewCollection::Create();
}
BlockTypeManager::~BlockTypeManager()
{
	for_each(m_types.begin(), m_types.end(), safeDelete);
}

void BlockTypeManager::AddType( BlockType& type )
{
	type.SetId(m_types.size());
	m_types.push_back(new BlockType(type));
	m_upToDate = false;
}
void BlockTypeManager::SetType( UINT index, BlockType& type )
{
	if (index >= m_types.size())
		m_types.resize(index + 1);

	type.SetId(index);
	if (!m_types[index])
	{
		m_types[index] = new BlockType(type);
	}
	else
	{
		m_types[index] = new (&m_types[index]) BlockType(type);
	}

	m_upToDate = false;
}
void BlockTypeManager::ResetType( UINT index )
{
	SAFE_DELETE(m_types.at(index));
	m_upToDate = false;
}
void BlockTypeManager::RemoveLastType()
{
	SAFE_DELETE(m_types.at(m_types.size() - 1));
	m_types.erase(m_types.end() - 1);
	m_upToDate = false;
}

bool BlockTypeManager::Build()
{
	float texturedTypes = 0;
	for (UINT i = 0; i < m_types.size(); i++)
	{
		if(m_types[i])
		{
			if (m_types[i]->HasTexture())
			{
				texturedTypes++;
			}
			else
			{
				 m_types[i]->SetTexCoords(XMFLOAT2(0.0f, 0.0f));
			}
		}
	}

	// compute new atlas size
	UINT newAtlasSize = ( (int)sqrt((float)texturedTypes) + 1) * m_texSize;

	// if size changed rebuild texture
	if (newAtlasSize != m_atlasSize)
	{
		m_atlasSize = newAtlasSize;
		m_relTexSize = (float)m_texSize / m_atlasSize;

		CD3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_atlasSize, m_atlasSize, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
		cgl::PD3D11Resource pTex = cgl::CD3D11Texture2DBlank::Create(desc);
		m_atlas->SetTexture(pTex);

		m_pViewCollection->Add(m_atlas->GetRenderTarget(), 0);
		CGL_RESTORE(m_pViewCollection);

		m_atlas->SetWidth((FLOAT)m_atlasSize);
		m_atlas->SetHeight((FLOAT)m_atlasSize);
	}

	// set up sprites
	m_spriteBatch->ResetData();
	m_currTexCoord = XMFLOAT2(0.0f, 0.0f);
	std::ifstream myfile;
	int size = 0;
	std::tr1::shared_ptr<char> pData = NULL;
	for (UINT i = 0; i < m_types.size(); i++)
	{		
		if (!m_types[i] || !m_types[i]->HasTexture())
			continue;

		// update tex coord
		//
		// note: the first place is empty
		//		 for types with only color
		m_currTexCoord.x += m_relTexSize;
		if (m_currTexCoord.x == 1.0f)
		{
			m_currTexCoord.x = 0.0f;
			m_currTexCoord.y += m_relTexSize;
		}

		m_types[i]->SetTexCoords(m_currTexCoord);
		m_types[i]->SetRelTexSize(m_relTexSize);

		// create sprite
		cgl::drawing::PCGLSprite sprite = cgl::drawing::CGLSprite::Create();
		sprite->SetWidth((FLOAT)m_texSize);
		sprite->SetHeight((FLOAT)m_texSize);

		// open texture
		myfile.open(m_types[i]->TextureName().c_str(), std::ios::in|std::ios::binary|std::ios::ate);
		if (!myfile.is_open())
			continue;
	
		size = (int)myfile.tellg();
		pData = std::tr1::shared_ptr<char>(new char[size]);
		myfile.seekg(0, std::ios_base::beg);
		myfile.read(pData.get(), size);
		myfile.close();

		D3DX11_IMAGE_LOAD_INFO loadInfo;
		ZeroMemory(&loadInfo, sizeof(loadInfo));
		loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		loadInfo.Usage = D3D11_USAGE_DEFAULT;
		loadInfo.Depth = 1;
		loadInfo.MipLevels = 1;
		loadInfo.Width = m_texSize;
		loadInfo.Height = m_texSize;

		cgl::PD3D11Resource pTex = cgl::CD3D11TextureFromMemory::Create(pData.get(), size, loadInfo);
		sprite->SetTexture(pTex);	
		sprite->SetX(m_currTexCoord.x * m_atlasSize);
		sprite->SetY(m_currTexCoord.y * m_atlasSize);
	
		m_spriteBatch->AddSprite(sprite);
	}

	m_atlas->GetRenderTarget()->Clear(1.0f, 1.0f, 1.0f, 1.0f);
	m_pViewCollection->Save();
	m_pViewCollection->Bind();
	m_spriteBatch->Render();
	m_pViewCollection->Load();

	m_spriteBatch->ResetData();
	m_spriteBatch->AddSprite(m_atlas);

	m_upToDate = true;

	return true;
}
bool BlockTypeManager::BuildWithVaryingTextureSizes()
{
	// get all types with textures
	std::list<BlockType*> tmp;
	for (UINT i = 0; i < m_types.size(); i++)
	{
		if(m_types[i])
		{
			if (m_types[i]->HasTexture())
			{
				tmp.push_back(m_types[i]);
			}
			else
			{
				m_types[i]->SetTexCoords(XMFLOAT2(0.0f, 0.0f));
			}
		}
	}

	// sort type according to their texture size
	tmp.sort(BlockType::sort);

	// get required full size block count
	float maxSize = (float)(*tmp.begin())->TexSize();
	float maxSizeBlocksNeeded = 0;
	for (auto it = tmp.begin(); it != tmp.end(); it++)
	{
		maxSizeBlocksNeeded += (*it)->TexSize() / maxSize;
	}

	// wipe out fractions
	maxSizeBlocksNeeded = (float)((int)maxSizeBlocksNeeded);

	// compute new atlas size
	UINT newAtlasSize = (UINT)(((int)sqrt((float)maxSizeBlocksNeeded) + 1) * maxSize); // +1 because of the empty block at the beginning
	for (int i = 1; i < 20; i++)
	{
		if ( pow(2.0f, i) >= newAtlasSize)
		{
			newAtlasSize = (UINT)pow(2.0f, i);
			break;
		}
	}

	// if size changed rebuild texture
	if (newAtlasSize != m_atlasSize)
	{
		m_atlasSize = newAtlasSize;

		CD3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, m_atlasSize, m_atlasSize, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
		cgl::PD3D11Resource pTex = cgl::CD3D11Texture2DBlank::Create(desc);
		if (!CGL_RESTORE(pTex))
			return false;

		if (!m_atlas->SetTexture(pTex))
			return false;

		m_pViewCollection->Add(m_atlas->GetRenderTarget(), 0);
		CGL_RESTORE(m_pViewCollection);

		m_atlas->SetWidth((FLOAT)m_atlasSize);
		m_atlas->SetHeight((FLOAT)m_atlasSize);
	}

	// set up sprites
	// 
	// note: the first place is empty
	//		 for types with only color
	//		 
	//		 these coordinates are relative
	//		 to the current max sized block
	XMFLOAT2 currTexCoords = XMFLOAT2(0.0f, 0.0f);
	XMFLOAT2 currBlock = XMFLOAT2(1, 0);
	std::tr1::shared_ptr<char> pData = NULL;
	std::ifstream myfile;
	int size = 0;
	m_spriteBatch->ResetData();
	std::vector<XMFLOAT2> insertPoints;
	float relativeBlockSize = maxSize / m_atlasSize;
	float currentTexSize = maxSize;
	for (auto it = tmp.begin(); it != tmp.end(); it++)
	{
		(*it)->SetTexCoords(XMFLOAT2((currBlock.x + currTexCoords.x) * relativeBlockSize,
									 (currBlock.y + currTexCoords.y) * relativeBlockSize));

		(*it)->SetRelTexSize((*it)->TexSize() / (float)m_atlasSize);

		// create sprite
		cgl::drawing::PCGLSprite sprite = cgl::drawing::CGLSprite::Create();
		sprite->SetWidth((FLOAT)(*it)->TexSize());
		sprite->SetHeight((FLOAT)(*it)->TexSize());
		sprite->SetX((currBlock.x + currTexCoords.x) * maxSize);
		sprite->SetY((currBlock.y + currTexCoords.y) * maxSize);
		sprite->SetColor((*it)->Color());

		m_spriteBatch->AddSprite(sprite);

		// open texture
		myfile.open((*it)->TextureName().c_str(), std::ios::in|std::ios::binary|std::ios::ate);
		if (!myfile.is_open())
			continue;

		size = (int)myfile.tellg();
		pData = std::tr1::shared_ptr<char>(new char[size]);
		myfile.seekg(0, std::ios_base::beg);
		myfile.read(pData.get(), size);
		myfile.close();

		D3DX11_IMAGE_LOAD_INFO loadInfo;
		ZeroMemory(&loadInfo, sizeof(loadInfo));
		loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		loadInfo.Usage = D3D11_USAGE_DEFAULT;
		loadInfo.Depth = 1;
		loadInfo.MipLevels = 1;
		loadInfo.Width = (*it)->TexSize();
		loadInfo.Height = (*it)->TexSize();

		cgl::PD3D11Resource pTex = cgl::CD3D11TextureFromMemory::Create(pData.get(), size, loadInfo);
		if (!sprite->SetTexture(pTex))
		{
			// couldn't create texture
			// don't break here just set the sprite color to 
			// something noticeable (debug)
			sprite->SetColor(XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f));
			continue;
		}
			
		// update tex coord
		//
		// reached the end of the current block
		if (currTexCoords.x + (*it)->TexSize() / maxSize >= 1.0f)
		{

			if(insertPoints.empty())
			{	
				if (currTexCoords.y + (*it)->TexSize() / maxSize < 1.0f)
				{
					currTexCoords.x = 0.0f;
					currTexCoords.y += (*it)->TexSize() / maxSize;
				}
				else
				{
					// if no more space left in this block
					// move to the next block
					currBlock = NextBlock(currBlock, m_atlasSize / (UINT)maxSize);
					currTexCoords.x = 0.0f;
					currTexCoords.y = 0.0f;
				}
			}
			else
			{
				currTexCoords = insertPoints.back();
				insertPoints.pop_back();
			}
		}
		else
		{			
			// tex got smaller
			if ((*it)->TexSize() != currentTexSize)
			{
				insertPoints.push_back(XMFLOAT2(currTexCoords.x, currTexCoords.y + (*it)->TexSize() / maxSize));
				currentTexSize = (float)((*it)->TexSize());
			}

			currTexCoords.x += (*it)->TexSize() / maxSize;
		}
	}

	m_viewPort.TopLeftX = 0;
	m_viewPort.TopLeftY = 0;
	m_viewPort.Width = (float)m_atlasSize;
	m_viewPort.Height = (float)m_atlasSize;
	m_viewPort.MinDepth = 0.0f;
	m_viewPort.MaxDepth = 1.0f;

	UINT num = 1;
	m_pViewCollection->getDevice()->GetContext()->RSGetViewports(&num, &m_savedViewPort);

	m_pViewCollection->getDevice()->GetContext()->RSSetViewports(1, &m_viewPort);

	m_atlas->GetRenderTarget()->Clear(0.9f, 0.9f, 0.9f, 1.0f);
	m_pViewCollection->Save();
	m_pViewCollection->Bind();
	m_spriteBatch->Render();
	m_pViewCollection->Load();

	m_pViewCollection->getDevice()->GetContext()->RSSetViewports(1, &m_savedViewPort);

	m_spriteBatch->ResetData();
	m_spriteBatch->AddSprite(m_atlas);

	SetAtlasRenderSize(768);
	
	m_upToDate = true;

	return true;
}
bool BlockTypeManager::BuildWithVaryingTextureSizesAndMultipleAtlases()
{
	// get all types with textures
	std::list<BlockType*> tmp;
	for (UINT i = 0; i < m_types.size(); i++)
	{
		if(m_types[i])
		{
			if (m_types[i]->HasTexture())
			{
				tmp.push_back(m_types[i]);
			}
			else
			{
				m_types[i]->SetTexCoords(XMFLOAT2(0.0f, 0.0f));
			}
		}
	}

	// sort type according to their texture size
	tmp.sort(BlockType::sort);

	// get required full size block count
	float maxSize = (float)(*tmp.begin())->TexSize();
	float maxSizeBlocksNeeded = 0;
	for (auto it = tmp.begin(); it != tmp.end(); it++)
	{
		maxSizeBlocksNeeded += (*it)->TexSize() / maxSize;
	}

	// wipe out fractions
	maxSizeBlocksNeeded = (float)((int)maxSizeBlocksNeeded);

	// compute new atlas size
	UINT newAtlasSize = (UINT)(((int)sqrt((float)maxSizeBlocksNeeded) + 1) * maxSize); // +1 because of the empty block at the beginning
	for (int i = 1; i < 20; i++)
	{
		if ( pow(2.0f, i) >= newAtlasSize)
		{
			newAtlasSize = (UINT)pow(2.0f, i);
			break;
		}
	}

	const float maxAtlasSize = 8192.0f;
	float atlasCount = newAtlasSize / maxAtlasSize;
	if (atlasCount - (int)atlasCount != 0)
		atlasCount = atlasCount + 1.0f;

	m_atlases.clear();
	m_atlases.resize((UINT)atlasCount);
	m_resourceCollection = cgl::CGLShaderResourceViewCollection::Create();
	for (int i = 0; i < atlasCount; i++ )
	{
		m_atlases[i].pSprite = cgl::drawing::CGLSprite::Create();
		m_atlases[i].pSpriteBatch = cgl::drawing::CGLSpriteBatch::Create(m_pEffect);
		m_atlases[i].pCollection = cgl::CGLRenderTargetViewCollection::Create();

		if (!m_atlases[i].pSpriteBatch->Init())
			return false;

		if (newAtlasSize > maxAtlasSize)
		{
			m_atlases[i].pSprite->SetWidth((FLOAT)maxAtlasSize);
			m_atlases[i].pSprite->SetHeight((FLOAT)maxAtlasSize);
			newAtlasSize -= (UINT)maxAtlasSize;
		}
		else
		{
			m_atlases[i].pSprite->SetWidth((FLOAT)newAtlasSize);
			m_atlases[i].pSprite->SetHeight((FLOAT)newAtlasSize);
		}

		CD3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, (UINT)m_atlases[i].pSprite->GetWidth(), (UINT)m_atlases[i].pSprite->GetHeight(), 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
		cgl::PD3D11Resource pTex = cgl::CD3D11Texture2DBlank::Create(desc);
		if (!CGL_RESTORE(pTex))
			return false;

		if (!m_atlases[i].pSprite->SetTexture(pTex))
			return false;

		m_atlases[i].pCollection->Add(m_atlases[i].pSprite->GetRenderTarget(), 0);
		if(!CGL_RESTORE(m_atlases[i].pCollection))
			return false;

		m_resourceCollection->Add(m_atlases[i].pSprite->GetShaderResource(), i);
	}

	if(!CGL_RESTORE(m_resourceCollection))
		return false;

	// set up sprites
	// 
	// note: the first place is empty
	//		 for types with only color
	//		 
	//		 these coordinates are relative
	//		 to the current max sized block
	XMFLOAT2 currTexCoords = XMFLOAT2(0.0f, 0.0f);
	XMFLOAT2 currBlock = XMFLOAT2(1, 0);
	std::tr1::shared_ptr<char> pData = NULL;
	std::ifstream myfile;
	int size = 0;
	m_spriteBatch->ResetData();
	std::vector<XMFLOAT2> insertPoints;
	float relativeBlockSize = maxSize / m_atlases[0].pSprite->GetWidth();
	float currentTexSize = maxSize;
	int currAtlas = 0;
	for (auto it = tmp.begin(); it != tmp.end(); it++)
	{	
		(*it)->SetTexCoords(XMFLOAT2((currBlock.x + currTexCoords.x) * relativeBlockSize,
									 (currBlock.y + currTexCoords.y) * relativeBlockSize));

		(*it)->SetRelTexSize((*it)->TexSize() / (float)m_atlases[currAtlas].pSprite->GetWidth());
		(*it)->SetAtlasIndex(currAtlas);

		// create sprite
		cgl::drawing::PCGLSprite sprite = cgl::drawing::CGLSprite::Create();
		sprite->SetWidth((FLOAT)(*it)->TexSize());
		sprite->SetHeight((FLOAT)(*it)->TexSize());
		sprite->SetX((currBlock.x + currTexCoords.x) * maxSize);
		sprite->SetY((currBlock.y + currTexCoords.y) * maxSize);
		sprite->SetColor((*it)->Color());

		m_atlases[currAtlas].pSpriteBatch->AddSprite(sprite);

		// open texture
		myfile.open((*it)->TextureName().c_str(), std::ios::in|std::ios::binary|std::ios::ate);
		if (!myfile.is_open())
			continue;

		size = (int)myfile.tellg();
		pData = std::tr1::shared_ptr<char>(new char[size]);
		myfile.seekg(0, std::ios_base::beg);
		myfile.read(pData.get(), size);
		myfile.close();

		D3DX11_IMAGE_LOAD_INFO loadInfo;
		ZeroMemory(&loadInfo, sizeof(loadInfo));
		loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		loadInfo.Usage = D3D11_USAGE_DEFAULT;
		loadInfo.Depth = 1;
		loadInfo.MipLevels = 1;
		loadInfo.Width = (*it)->TexSize();
		loadInfo.Height = (*it)->TexSize();

		cgl::PD3D11Resource pTex = cgl::CD3D11TextureFromMemory::Create(pData.get(), size, loadInfo);
		if (!sprite->SetTexture(pTex))
		{
			// couldn't create texture
			// don't break here just set the sprite color to 
			// something noticeable (debug)
			sprite->SetColor(XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f));
			continue;
		}

		// update tex coords
		//
		// reached the end of the current block
		if (currTexCoords.x + (*it)->TexSize() / maxSize >= 1.0f)
		{
			// if no more space left in this block
			// move to the next block
			if(insertPoints.empty())
			{
				currBlock = NextBlock(currBlock, (UINT)(m_atlases[currAtlas].pSprite->GetWidth() / maxSize));
				currTexCoords.x = 0.0f;
				currTexCoords.y = 0.0f;

				if (currBlock.y == m_atlases[currAtlas].pSprite->GetHeight() / maxSize - 1 &&
					currBlock.x == m_atlases[currAtlas].pSprite->GetWidth()  / maxSize - 1 )
				{
					// currAtlas++;
					currBlock.x = 0.0f;
					currBlock.y = 0.0f;

					relativeBlockSize = maxSize / m_atlases[currAtlas].pSprite->GetWidth();
				}
			}
			else
			{
				currTexCoords = insertPoints.back();
				insertPoints.pop_back();
			}
		}
		else
		{			
			// tex got smaller
			if ((*it)->TexSize() != currentTexSize)
			{
				insertPoints.push_back(XMFLOAT2(currTexCoords.x, currTexCoords.y + (*it)->TexSize() / maxSize));
				currentTexSize = (float)((*it)->TexSize());
			}

			currTexCoords.x += (*it)->TexSize() / maxSize;
		}
	}

	// render
	// 
	// reset debug sprite batch 
	m_spriteBatch->ResetData();
	//
	// save current view port
	UINT num = 1;
	mgr()->GetDevice()->GetContext()->RSGetViewports(&num, &m_savedViewPort);
	m_atlases[0].pCollection->Save();
	for (int i = 0; i < atlasCount; i++)
	{
		//////////////////////////////////////////////////////////////////////////
		// set required view port
		// 
		m_viewPort.TopLeftX = 0;
		m_viewPort.TopLeftY = 0;
		m_viewPort.Width = (float)m_atlases[i].pSprite->GetWidth();
		m_viewPort.Height = (float)m_atlases[i].pSprite->GetHeight();
		m_viewPort.MinDepth = 0.0f;
		m_viewPort.MaxDepth = 1.0f;
		
		mgr()->GetDevice()->GetContext()->RSSetViewports(1, &m_viewPort);
		
		m_atlases[i].pSprite->GetRenderTarget()->Clear(0.9f, 0.9f, 0.9f, 1.0f);
		m_atlases[i].pCollection->Bind();
		m_atlases[i].pSpriteBatch->Render();

		//
		// set up debug batch for visualizing
		// the tex atlases
		m_atlases[i].pSprite->SetX(i * 512.0f);
		m_atlases[i].pSprite->SetY(0.0f);
		m_atlases[i].pSprite->SetWidth(512.0f);
		m_atlases[i].pSprite->SetHeight(512.0f);
		m_spriteBatch->AddSprite(m_atlases[i].pSprite);
	}

	m_atlases[0].pCollection->Load();

	mgr()->GetDevice()->GetContext()->RSSetViewports(1, &m_savedViewPort);

	m_upToDate = true;

	return true;
}
bool BlockTypeManager::BuildWithVaryingTextureSizesAndMultipleAtlasesWithVaryingSize()
{
	// get all types with textures
	std::list<BlockType*> tmp;
	for (UINT i = 0; i < m_types.size(); i++)
	{
		if(m_types[i])
		{
			if (m_types[i]->HasTexture())
			{
				tmp.push_back(m_types[i]);
			}
			else
			{
				m_types[i]->SetTexCoords(XMFLOAT2(0.0f, 0.0f));
			}
		}
	}

	// sort type according to their texture size
	tmp.sort(BlockType::sort);

	// get required full size block count
	float maxSize = (float)(*tmp.begin())->TexSize();

	float maxAtlasSize = 8192.0f;

	float maxSizeBlocksNeeded = 0;
// 	for (auto it = tmp.begin(); it != tmp.end(); it++)
// 		maxSizeBlocksNeeded += ((*it)->TexSize() / maxSize) * ((*it)->TexSize() / maxSize);

	int	maxSizeBlocksPerAtlas = (int)((maxAtlasSize * maxAtlasSize) / (maxSize * maxSize));
		
	std::vector<BlockType*> currTypes;
	for (auto it = tmp.begin(); it != tmp.end(); it++)
	{
		maxSizeBlocksNeeded += ((*it)->TexSize() / maxSize) * ((*it)->TexSize() / maxSize);
		if (maxSizeBlocksNeeded > maxSizeBlocksPerAtlas)
		{
			Atlas atlas;
			atlas.pSprite = cgl::drawing::CGLSprite::Create();
			atlas.pSpriteBatch = cgl::drawing::CGLSpriteBatch::Create(m_pEffect);
			if (!atlas.pSpriteBatch->Init())
				return false;

			if (!BuildWithVaryingTextureSizes(currTypes, (UINT)maxAtlasSize, maxSize, atlas.pSprite, atlas.pSpriteBatch))
				return false;

			m_atlases.push_back(atlas);

			currTypes.clear();
			maxSizeBlocksNeeded -= maxSizeBlocksPerAtlas;

			// uncomment this to enable varying atlas sizes
			maxSize = (float)(*it)->TexSize();
			maxSizeBlocksPerAtlas = (int)((maxAtlasSize * maxAtlasSize) / (maxSize * maxSize));
		}

		currTypes.push_back((*it));
	}

	// wipe out fractions
	maxSizeBlocksNeeded = (float)((int)maxSizeBlocksNeeded);

	// compute new atlas size
	UINT atlasSize = (UINT)(((int)sqrt((float)maxSizeBlocksNeeded) + 1) * maxSize); // + 1 because of the empty block at the beginning

	Atlas atlas;
	atlas.pSprite = cgl::drawing::CGLSprite::Create();
	atlas.pSpriteBatch = cgl::drawing::CGLSpriteBatch::Create(m_pEffect);
	if (!atlas.pSpriteBatch->Init())
		return false;

	if (!BuildWithVaryingTextureSizes(currTypes, atlasSize, maxSize, atlas.pSprite, atlas.pSpriteBatch))
		return false;

	m_atlases.push_back(atlas);

	//  
	// prepare rendering,
	m_spriteBatch->ResetData();

	D3D11_VIEWPORT viewPort;
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;
	viewPort.MinDepth = 0.0f;
	viewPort.MaxDepth = 1.0f;

	// save current view ports
	UINT num = 1;
	mgr()->GetDevice()->GetContext()->RSGetViewports(&num, &m_savedViewPort);

	m_resourceCollection = cgl::CGLShaderResourceViewCollection::Create();

// 	D3D11_TEXTURE2D_DESC desc;
// 	ZeroMemory(&desc, sizeof(desc));
// 	desc.ArraySize = m_atlases.size();
// 	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
// 	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
// 	desc.Usage = D3D11_USAGE_DEFAULT;
// 	desc.Width = atlasSize;
// 	desc.Height = atlasSize;
// 	desc.MipLevels = 1;
// 	desc.SampleDesc.Count = 1;
// 	desc.SampleDesc.Quality = 0;
// 	desc.MipLevels = 1;
// 	m_pAtlasTexture2DArr = cgl::CD3D11Texture2DBlank::Create(desc);
// 	if (!m_pAtlasTexture2DArr->restore())
// 	{
// 		return false;
// 	}

	UINT currAtlas = 0;
	for (auto it = m_atlases.begin(); it != m_atlases.end(); it++)
	{
		// view port
		viewPort.Width = (float)it->pSprite->GetWidth();
		viewPort.Height = (float)it->pSprite->GetHeight();
		mgr()->GetDevice()->GetContext()->RSSetViewports(1, &viewPort);
		
		// render target
		it->pSprite->GetRenderTarget()->Clear(0.9f, 0.9f, 0.9f, 1.0f);
		cgl::PCGLRenderTargetViewCollection collection = cgl::CGLRenderTargetViewCollection::Create();
		collection->Add(it->pSprite->GetRenderTarget(), 0);
		if (!CGL_RESTORE(collection))
			return false;

		collection->Save();
		collection->Bind();

		// render
		it->pSpriteBatch->Render();

		// load previous render targets
		collection->Load();

		// save texture
		m_resourceCollection->Add(it->pSprite->GetShaderResource(), currAtlas);

		// copy tex into the atlas texture 2d array
		// mgr()->GetDevice()->GetContext()->CopySubresourceRegion(m_pAtlasTexture2DArr->get(), currAtlas, 0, 0, 0, it->pSprite->GetTexture()->get(), 0, NULL);

		it->pSprite->SetHeight(256.0f);
		it->pSprite->SetWidth(256.0f);
		it->pSprite->SetX(currAtlas * 256.0f);

		m_spriteBatch->AddSprite(it->pSprite);
	
		currAtlas++;
	}

	// load previous view ports
	mgr()->GetDevice()->GetContext()->RSSetViewports(1, &m_savedViewPort);

	// init resource view collection
	if(!CGL_RESTORE(m_resourceCollection))
		return false;

	// create atlas texture 2d arr resource view
// 	m_pAtlasTexture2DArrSRV = cgl::CD3D11ShaderResourceView::Create(m_pAtlasTexture2DArr);
// 	if (!m_pAtlasTexture2DArrSRV->restore())
// 	{
// 		return false;
// 	}

	// pack types
	m_packedTypes.clear();
	for (auto it = m_types.begin(); it != m_types.end(); it++)
		m_packedTypes.push_back((*it)->GetPacked());

	m_upToDate = true;

	return true;
}
bool BlockTypeManager::BuildWithVaryingTextureSizes(std::vector<BlockType*> types, UINT atlasSize, float maxBlockSize, cgl::drawing::PCGLSprite& atlas, cgl::drawing::PCGLSpriteBatch& batch)
{
	CD3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8G8B8A8_UNORM, atlasSize, atlasSize, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
	cgl::PD3D11Resource pTex = cgl::CD3D11Texture2DBlank::Create(desc);
	if (!CGL_RESTORE(pTex))
		return false;

	if (!atlas->SetTexture(pTex))
		return false;

	atlas->SetWidth((FLOAT)atlasSize);
	atlas->SetHeight((FLOAT)atlasSize);

	// set up sprites
	// 
	// note: the first place is empty
	//		 for types with only color
	//		 
	//		 these coordinates are relative
	//		 to the current max sized block	
	//		
	// tex loading
	std::tr1::shared_ptr<char> pData = NULL;
	std::ifstream myfile;
	int size = 0;
	//
	// tex coords
	XMFLOAT2 currTexCoords = XMFLOAT2(0.0f, 0.0f);
	XMFLOAT2 currBlock = XMFLOAT2(0, 0);
	std::vector<XMFLOAT2> insertPoints;
	
	float relativeBlockSize = maxBlockSize / atlasSize;
	float currentTexSize = maxBlockSize;

	batch->ResetData();
	for (auto it = types.begin(); it != types.end(); it++)
	{
		(*it)->SetTexCoords(XMFLOAT2((currBlock.x + currTexCoords.x) * relativeBlockSize,
							         (currBlock.y + currTexCoords.y) * relativeBlockSize));

		(*it)->SetRelTexSize((*it)->TexSize() / (float)atlasSize);
		(*it)->SetAtlasIndex(m_atlases.size());

		// create sprite
		cgl::drawing::PCGLSprite sprite = cgl::drawing::CGLSprite::Create();
		sprite->SetWidth((FLOAT)(*it)->TexSize());
		sprite->SetHeight((FLOAT)(*it)->TexSize());
		sprite->SetX((currBlock.x + currTexCoords.x) * maxBlockSize);
		sprite->SetY((currBlock.y + currTexCoords.y) * maxBlockSize);
		sprite->SetColor((*it)->Color());

		batch->AddSprite(sprite);

		// open texture
		myfile.open((*it)->TextureName().c_str(), std::ios::in|std::ios::binary|std::ios::ate);
		if (!myfile.is_open())
			continue;

		size = (int)myfile.tellg();
		pData = std::tr1::shared_ptr<char>(new char[size]);
		myfile.seekg(0, std::ios_base::beg);
		myfile.read(pData.get(), size);
		myfile.close();

		D3DX11_IMAGE_LOAD_INFO loadInfo;
		ZeroMemory(&loadInfo, sizeof(loadInfo));
		loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		loadInfo.Usage = D3D11_USAGE_DEFAULT;
		loadInfo.Depth = 1;
		loadInfo.MipLevels = 1;
		loadInfo.Width = (*it)->TexSize();
		loadInfo.Height = (*it)->TexSize();

		cgl::PD3D11Resource pTex = cgl::CD3D11TextureFromMemory::Create(pData.get(), size, loadInfo);
		if (!sprite->SetTexture(pTex))
		{
			// couldn't create texture
			// don't break here just set the sprite color to 
			// something noticeable (debug)
			sprite->SetColor(XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f));
			continue;
		}

		// update tex coord
		//
		// reached the end of the current block
		if (currTexCoords.x + (*it)->TexSize() / maxBlockSize >= 1.0f)
		{
			if(insertPoints.empty())
			{	
				if (currTexCoords.y + (*it)->TexSize() / maxBlockSize < 1.0f)
				{
					currTexCoords.x = 0.0f;
					currTexCoords.y += (*it)->TexSize() / maxBlockSize;
				}
				else
				{
					// if no more space left in this block
					// move to the next block
					currBlock = NextBlock(currBlock, atlasSize / (UINT)maxBlockSize);
					currTexCoords.x = 0.0f;
					currTexCoords.y = 0.0f;
				}
			}
			else
			{
				currTexCoords = insertPoints.back();
				insertPoints.pop_back();
			}
		}
		else
		{			
			// tex got smaller
			if ((*it)->TexSize() != currentTexSize)
			{
				insertPoints.push_back(XMFLOAT2(currTexCoords.x, currTexCoords.y + (*it)->TexSize() / (float)maxBlockSize));
				currentTexSize = (float)((*it)->TexSize());
			}

			currTexCoords.x += (*it)->TexSize() / maxBlockSize;
		}
	}

	return true;
}

bool BlockTypeManager::Init()
{
	// open effect
	std::ifstream myfile;
	int size = 0;
	std::tr1::shared_ptr<char> pData = NULL;
	myfile.open ("console.fxc", std::ios::in|std::ios::binary|std::ios::ate);
	if (myfile.is_open())
	{
		size = (int)myfile.tellg();
		pData = std::tr1::shared_ptr<char>(new char[size]);
		myfile.seekg(0, std::ios_base::beg);
		myfile.read(pData.get(), size);
		myfile.close();
	}

	m_pEffect = cgl::CD3D11EffectFromMemory::Create(pData.get(), size);
	if (!CGL_RESTORE(m_pEffect))
		return false;

	m_spriteBatch = cgl::drawing::CGLSpriteBatch::Create(m_pEffect);
	if (!m_spriteBatch->Init())
		return false;

	m_atlas = cgl::drawing::CGLSprite::Create();

	return true;
}
bool BlockTypeManager::Update()
{

#define TEST

#ifdef TEST
	if (!m_upToDate)
		return BuildWithVaryingTextureSizesAndMultipleAtlasesWithVaryingSize();
#else
	if (!m_upToDate)
		return BuildWithVaryingTextureSizes();
#endif

	return false;
}
void BlockTypeManager::Render()
{
	m_spriteBatch->Render();
}

XMFLOAT2 BlockTypeManager::NextBlock( const XMFLOAT2& currBlock, int blocksPerRow)
{
	XMFLOAT2 out;
	if (currBlock.x == blocksPerRow - 1)
	{
		out.x = 0;
		out.y = currBlock.y + 1;
	}
	else
	{
		out.x = currBlock.x + 1;
		out.y = currBlock.y;
	}
	
	return out;
}
void BlockTypeManager::SetAtlasRenderSize( int size )
{
	m_atlas->SetWidth((float)size);
	m_atlas->SetHeight((float)size);
}



