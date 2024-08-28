#ifndef __C_NODES_H__
#define __C_NODES_H__

#include <stdio.h>
#include <memory.h>

class CNode
{
public:
	class CNode* next;
	unsigned int m_start;
	unsigned char* m_data;
	unsigned int m_len;
	CNode()
	{
		next = 0;
		m_start = 0;
		m_data = 0;
		m_len = 0;

	}
	CNode(unsigned int start, unsigned char* data, unsigned int len)
	{
		next = 0;
		m_start = start;
		m_data = new unsigned char[len];
		memcpy(m_data, data, len);
		m_len = len;
	}
	~CNode()
	{
		if (m_data) delete[] m_data;
	}
};

class CNodes
{
private:
	class CNode* head;
	unsigned int next_start;
	unsigned int m_totalLen;
	unsigned char* des;
public:
	CNodes();
	~CNodes();
	unsigned int totalLen();
	unsigned int len();
	
	bool add(unsigned int start, unsigned char* data, unsigned int len, unsigned int totalLen);
	unsigned char* get(unsigned int nStartDes, unsigned int des_len);
	void reset();
};

#endif