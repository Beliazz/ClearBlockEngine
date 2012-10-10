#include "cbe.h"

namespace cbe
{

template <class T>
class ThreadSafe
{
private:
	CRITICAL_SECTION m_cs;
	std::shared_ptr<T> m_obj;

	void enterSecureMode()
	{
		::EnterCriticalSection(&m_cs);
	}
	void leaveSecureMode()
	{
		::LeaveCriticalSection(&m_cs);
	}

public:
	class BlockSecurity
	{
		ThreadSafe& m_pParent;
		int m_lockCount;

	public:
		BlockSecurity(ThreadSafe& pParent) : m_pParent(pParent), m_lockCount(0)
		{
			pParent.enterSecureMode();
		}
		~BlockSecurity()
		{
			m_pParent.leaveSecureMode();
		}

		T* operator ->()
		{
			return m_pParent.m_obj.get();
		}
		T* operator *()
		{
			return m_pParent.m_obj.get();
		}
	};

	ThreadSafe(T* obj = NULL) : m_obj(obj)
	{
		::InitializeCriticalSection(&m_cs);
	}
	ThreadSafe(ThreadSafe& rhs)
	{
		::InitializeCriticalSection(&m_cs);

		rhs.enterSecureMode();
		m_obj = rhs.m_obj;
		rhs.leaveSecureMode();
	}
	~ThreadSafe()
	{
		::DeleteCriticalSection(&m_cs);
	}
	ThreadSafe& operator = (ThreadSafe& rhs)
	{
		if (&rhs == this)
			return *this;

		this->m_obj = rhs.m_obj;

		return *this;
	}

	inline BlockSecurity blockSecurity() { return *this; }

	void set(T* obj)
	{
		BlockSecurity __blockSec(*this);
		m_obj = std::tr1::shared_ptr<T>(obj);
	}
	void reset()
	{
		BlockSecurity __blockSec(this);
		m_obj = NULL;
	}

	inline T* get() { return m_obj; }
	
	operator bool()
	{
		BlockSecurity __blockSec(*this);
		return (bool)m_obj;
	}
	bool operator !()
	{
		BlockSecurity __blockSec(*this);
		return !m_obj;
	}

	BlockSecurity operator ->()
	{
		return *this;
	}
	BlockSecurity operator *()
	{
		return *this;
	}
};

}
