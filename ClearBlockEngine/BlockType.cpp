#include "BlockType.h"

using namespace cbe;

BlockType::BlockType()
{
	m_id = 0;
	m_name = "invalid";
	m_diffuseTexture = "";
	m_transparent = true;
	m_color = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_hasTexture = false;
	m_relTexSize = 0.0f;
	m_texCoords = XMFLOAT2(0.0f, 0.0f);
}

void BlockType::SetId(USHORT index)
{
	m_id = index;
}
void BlockType::SetTexCoords(XMFLOAT2 texCoords)
{
	m_texCoords = texCoords;
}
void BlockType::SetRelTexSize(float relTexSize)
{
	m_relTexSize = relTexSize;
}
void BlockType::SetTexture( std::string textureName, bool transparent /*= false*/ )
{
	m_diffuseTexture = textureName;
	m_transparent = transparent;
	m_hasTexture = true;
}
void BlockType::SetColor( XMFLOAT4 color )
{
	m_color = color;
	if (!m_transparent)
		m_transparent = (color.w < 1.0f);
}
void BlockType::SetName( std::string name )
{
	m_name = name;
}

XMFLOAT4 BlockType::GetRectangleTexCoords( UINT relWidth, UINT relHeight )
{
	return XMFLOAT4(m_texCoords.x, m_texCoords.y, m_texCoords.x + m_relTexSize * relWidth, m_texCoords.y + m_relTexSize * relHeight);
}

void BlockType::SetTexSize( int size )
{
	float root = log((float)size) / log(2.0f);
	if ( root - (int)root != 0 )
		throw ("texture size must be power of 2");

	m_texSize = size;
}

void BlockType::SetAtlasIndex( UINT index )
{
	m_atlasIndex = index;
}

PackedBlockType BlockType::GetPacked()
{
	PackedBlockType packed;
	packed.atlasIndex = m_atlasIndex;
	packed.color = m_color;
	packed.relTexSize = m_relTexSize;
	packed.texCoords = m_texCoords;

	return packed;
}

