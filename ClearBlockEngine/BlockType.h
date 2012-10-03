#pragma once

#include "stdafx.h"
#include <cmath>

namespace cbe
{

struct PackedBlockType
{
	XMFLOAT2	texCoords;
	float		relTexSize;
	int			atlasIndex;
	XMFLOAT4	color;
};

class CBE_API BlockType
{
private:
	friend class BlockTypeManager;

	USHORT m_id;

	bool m_transparent;
	bool m_hasTexture;

	XMFLOAT2 m_texCoords;
	float m_relTexSize;
	int m_texSize;
	UINT m_atlasIndex;

	XMFLOAT4 m_color;

	std::string m_name;
	std::string m_diffuseTexture;

	void SetId(USHORT index);
	void SetRelTexSize(float relTexSize);

public:
	BlockType();
	
	const inline USHORT			Id()			const { return m_id; }
	const inline bool			Transparent()	const { return m_transparent; }
	const inline bool			HasTexture()	const { return m_hasTexture; }
	const inline XMFLOAT4		Color()			const { return m_color; }
	const inline std::string	Name()			const { return m_name; }
	const inline std::string	TextureName()	const { return m_diffuseTexture; }
	const inline XMFLOAT2		TexCoords()		const { return m_texCoords; }
	const inline int			TexSize()		const { return m_texSize; }
	const inline float			RelTexSize()	const { return m_relTexSize; }
	const inline UINT			AtlasIndex()	const { return m_atlasIndex; }

	XMFLOAT4 GetRectangleTexCoords(UINT relWidth, UINT relHeight);

	void SetTexture(std::string textureName, bool transparent = false);
	void SetColor(XMFLOAT4 color);
	void SetName(std::string name);
	void SetTexCoords(XMFLOAT2 texCoords);
	void SetTexSize(int size);
	void SetAtlasIndex(UINT index);
	
	virtual bool Serialize()	{ throw "not implemented"; }
	virtual bool Deserialize()	{ throw "not implemented"; }

	PackedBlockType GetPacked();

	static bool sort (const BlockType* lhs, const BlockType* rhs)
	{
		return lhs->m_texSize > rhs->m_texSize;
	}
};

}