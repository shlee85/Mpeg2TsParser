#include "CNodes.h"

CNodes::CNodes()
{
	head = 0;
	next_start = 0;
	m_totalLen = 0;
	des = 0;
}

CNodes::~CNodes()
{
	reset();
	if (des) delete des;
}

unsigned int CNodes::totalLen()
{
	return m_totalLen;
}

unsigned int CNodes::len()
{
	return next_start;
}

bool CNodes::add(unsigned int start, unsigned char* data, unsigned int len, unsigned int totalLen)
{
	if (start == 0)
	{
		reset();
	}
	if (start != next_start)
	{
		reset();
		return false;
	}
	CNode* node = new CNode(start, data, len);
	if (totalLen > 0) m_totalLen = totalLen;
	if (head == 0)
	{
		head = node;
		next_start = len;
		return (next_start == m_totalLen) ? true : false;
	}
	CNode* curr = head;
	while (curr->next != 0) curr = curr->next;
	curr->next = node;
	next_start += len;
	return (next_start == m_totalLen) ? true : false;
}

unsigned char* CNodes::get(unsigned int nStartDes, unsigned int des_len)
{
	if (nStartDes >= next_start) return 0;
	if (nStartDes + des_len > next_start) return 0;
	if (des) delete des;
	des = new unsigned char[des_len + 1];
	des[des_len] = 0; // null zero for string
	unsigned int nTotalLen = 0;
	unsigned int nEndDes = nStartDes + des_len;
	for (CNode* src = head; src != 0; src = src->next)
	{
		unsigned int nStartSrc = src->m_start;
		unsigned int nEndSrc = nStartSrc + src->m_len;
		if (nEndDes < nStartSrc || nEndSrc < nStartDes) continue;
		int nStartSrcCopy = (nStartSrc < nStartDes) ? nStartDes - nStartSrc : 0;
		int nStartDesCopy = (nStartSrc > nStartDes) ? nStartSrc - nStartDes : 0;
		int nCopyEnd = (nEndSrc < nEndDes) ? nEndSrc : nEndDes;
		int nCopyStart = (nStartSrc > nStartDes) ? nStartSrc : nStartDes;
		int nCopyLen = nCopyEnd - nCopyStart;
		memcpy(&des[nStartDesCopy], &src->m_data[nStartSrcCopy], nCopyLen);
		nTotalLen += nCopyLen;
	}
	return des;
}

void CNodes::reset()
{
	CNode* curr = head;
	while (curr != 0)
	{
		CNode* prev = curr;
		curr = curr->next;
		delete prev;
	}
	head = 0;
	next_start = 0;
	m_totalLen = 0;
}
