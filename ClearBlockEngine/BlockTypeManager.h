#pragma once

#include "stdafx.h"
#include "BlockType.h"
#include <cmath>
#include <list>

namespace cbe
{

class CBE_API BlockTypeManager : private cgl::CGLManagerConnector
{
private:
	cgl::drawing::PCGLSprite m_atlas;
	cgl::drawing::PCGLSpriteBatch m_spriteBatch;
	std::vector<BlockType*> m_types;
	std::vector<PackedBlockType> m_packedTypes;
	cgl::PD3D11Effect m_pEffect;
	cgl::PCGLRenderTargetViewCollection m_pViewCollection;

	UINT m_texSize;
	float m_relTexSize;
	UINT m_atlasSize;
	XMFLOAT2 m_currTexCoord;
	
	bool m_upToDate;

	D3D11_VIEWPORT m_viewPort;
	D3D11_VIEWPORT m_savedViewPort;

protected:
	bool Build();

	//////////////////////////////////////////////////////////////////////////
	// research brand
	// 
	// varying texture sizes
	bool BuildWithVaryingTextureSizes();
	bool BuildWithVaryingTextureSizes(std::vector<BlockType*> types, UINT atlasSize, float maxBlockSize, cgl::drawing::PCGLSprite& atlas, cgl::drawing::PCGLSpriteBatch& batch);
	XMFLOAT2 NextBlock( const XMFLOAT2& currBlock, int blockPerRow);
	//
	// multiple atlantes
	struct Atlas
	{
		cgl::drawing::PCGLSprite pSprite;
		cgl::drawing::PCGLSpriteBatch pSpriteBatch;
		cgl::PCGLRenderTargetViewCollection pCollection;
	};
	std::vector<Atlas> m_atlases;
	cgl::PD3D11Texture2DBlank m_pAtlasTexture2DArr;
	cgl::PD3D11ShaderResourceView m_pAtlasTexture2DArrSRV;
	cgl::PCGLShaderResourceViewCollection m_resourceCollection;
	bool BuildWithVaryingTextureSizesAndMultipleAtlases();
	bool BuildWithVaryingTextureSizesAndMultipleAtlasesWithVaryingSize();
	//////////////////////////////////////////////////////////////////////////

public:
	BlockTypeManager(UINT texSize);
	~BlockTypeManager();

	bool Init();
	void AddType(BlockType& type);
	void SetType(UINT index, BlockType& type);
	void ResetType(UINT index);
	void RemoveLastType();
	bool Update();
	void Render();
	void SetAtlasRenderSize(int size);

	inline cgl::drawing::PCGLSprite& GetAtlas()					{ return m_atlas; }
	inline UINT GetAtlasSize()									{ return m_atlasSize; }
	inline float GetRelativeTextureSize()						{ return m_relTexSize; }
	inline UINT GetTextureSize()								{ return m_texSize; }
	inline UINT GetTypeCount()									{ return m_types.size(); }
	inline BlockType* GetType(UINT index)						{ return m_types.at(index); }
	inline cgl::PCGLShaderResourceViewCollection& GetAtlases()	{ return m_resourceCollection; }
	inline cgl::PD3D11ShaderResourceView& GetAtlasArraySRV()	{ return m_pAtlasTexture2DArrSRV; }
	inline std::vector<PackedBlockType>& GetPackedTypes()		{ return m_packedTypes; }

	virtual bool Serialize(std::string filename)		{ throw "not implemented"; }
	virtual bool Deserialize(std::string filename)		{ throw "not implemented"; }
};

}