#pragma once

namespace cbe
{

#pragma pack(push, 1)
class Block
{
private:
	//////////////////////////////////////////////////////////////////////////
	// description
	// 
	// |1|1 1 1 1 1|1 1 1 1 1 1 1 1 1 1| 
	// |A|<-Group->|<------Type------->|
	// 
	// A... active = 1 << 15
	// max block types = 2 ^ 10 - 1 = 1023 (the first is reserved for the invalid block type)
	// max groups = 2 ^ 5 = 31 (the first is reserved for indicating tiling)
	//
	// active read mask = |1|0 0 0 0 0|0 0 0 0 0 0 0 0 0 0| = 32768
	// group read mask  = |0|1 1 1 1 1|0 0 0 0 0 0 0 0 0 0| = 31744
	// type read mask   = |0|0 0 0 0 0|1 1 1 1 1 1 1 1 1 1| = 1023
	unsigned __int16 m_data;
	
	const __forceinline static unsigned __int16 GroupReadMask()  { return 31744; }
	const __forceinline static unsigned __int8  GroupBitOffset() { return 10; }

	const __forceinline static unsigned __int16 TypeReadMask()	 { return 1023;  }
	const __forceinline static unsigned __int8  TypeBitOffset()  { return 0; }

	const __forceinline static unsigned __int16 ActiveReadMask() { return 32768; }
	const __forceinline static unsigned __int8  ActiveBitOffset(){ return 15; }


public:	
	Block()
	{
		SetType(0);
		SetGroup(0);
		SetActive(0);
	}

	void SetActive(int active) 
	{ 
		m_data =  m_data & ~ActiveReadMask();
		m_data |= active << ActiveBitOffset(); 
	}
	void SetType(unsigned __int16 type)
	{ 
		m_data = m_data & ~TypeReadMask();
		m_data |= type << TypeBitOffset();
	}
	void SetGroup(unsigned __int16 group)
	{
		m_data = m_data & ~GroupReadMask();
		m_data |= group << GroupBitOffset();
	}

	inline unsigned __int16 Type()	const { return (m_data & TypeReadMask()); }
	inline unsigned __int16 Group() const { return (m_data & GroupReadMask()) >> GroupBitOffset(); }
	inline bool Active()			const { return (m_data & ActiveReadMask()) == ActiveReadMask();}

	const __forceinline static unsigned __int16 MaxType()  { return 1023;}
	const __forceinline static unsigned __int16 MaxGroup() { return 31;  }
};
#pragma pack(pop)

}