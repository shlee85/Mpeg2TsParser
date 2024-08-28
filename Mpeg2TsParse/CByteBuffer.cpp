#include <stdio.h>
#include "CByteBuffer.h"

CByteBuffer::CByteBuffer()
{
	nLen = 0;
	data = 0;
	nPos = 0;
}
CByteBuffer::CByteBuffer(const std::string& str)
{
	set((unsigned char*)str.c_str(), (unsigned int)str.length());
}
CByteBuffer::CByteBuffer(unsigned char* p, unsigned int len)
{
	set(p, len);
}
void CByteBuffer::set(unsigned char* p, unsigned int len)
{
	nLen = len;
	data = p;
	nPos = 0;
}
unsigned char CByteBuffer::get()
{
	if (nLen == 0 || data == 0)
	{
		printf("%s|%d|Error\n", __func__, __LINE__);
		return 0;
	}
	if (nPos + 1 > nLen)
	{
		printf("%s|%d|Error\n", __func__, __LINE__);
		return 0;
	}
	return data[nPos++];
}
unsigned short CByteBuffer::get2(bool bLittleEndian)
{
	if (nLen == 0 || data == 0)
	{
		printf("%s|%d|Error\n", __func__, __LINE__);
		return 0;
	}
	if (nPos + 2 > nLen)
	{
		printf("%s|%d|Error\n", __func__, __LINE__);
		return 0;
	}
	if (bLittleEndian == false)
	{
		unsigned short ret = data[nPos] * 0x100 + data[nPos + 1];
		nPos += 2;
		return ret;
	}
	unsigned short ret = data[nPos + 1] * 0x100 + data[nPos];
	nPos += 2;
	return ret;
}
unsigned int CByteBuffer::get3(bool bLittleEndian)
{
	if (nLen == 0 || data == 0)
	{
		printf("%s|%d|Error\n", __func__, __LINE__);
		return 0;
	}
	if (nPos + 3 > nLen)
	{
		printf("%s|%d|Error\n", __func__, __LINE__);
		return 0;
	}
	if (bLittleEndian == false)
	{
		unsigned int ret = data[nPos] * 0x10000 + data[nPos + 1] * 0x100 + data[nPos + 2];
		nPos += 3;
		return ret;
	}
	unsigned int ret = data[nPos + 2] * 0x10000 + data[nPos + 1] * 0x100 + data[nPos];
	nPos += 3;
	return ret;
}
unsigned int CByteBuffer::get4(bool bLittleEndian)
{
	if (nLen == 0 || data == 0)
	{
		printf("%s|%d|Error\n", __func__, __LINE__);
		return 0;
	}
	if (nPos + 4 > nLen)
	{
		printf("%s|%d|Error\n", __func__, __LINE__);
		return 0;
	}
	if (bLittleEndian == false)
	{
		unsigned int ret = data[nPos] * 0x1000000 + data[nPos + 1] * 0x10000 + data[nPos + 2] * 0x100 + data[nPos + 3];
		nPos += 4;
		return ret;
	}
	unsigned int ret = data[nPos + 3] * 0x1000000 + data[nPos + 2] * 0x10000 + data[nPos + 1] * 0x100 + data[nPos];
	nPos += 4;
	return ret;
}
unsigned char* CByteBuffer::gets(unsigned int len)
{
	if (len == 0) return 0;
	//printf("len %d\n", len);
	//printf("nLen %d\n", nLen);
	//printf("mpos %d\n", nPos);
	if (nLen == 0 || data == 0)
	{
		printf("%s|%d|Error\n", __func__, __LINE__);
		return 0;
	}
	if (nPos + len > nLen)
	{
		printf("%s|%d|len=%d|Error\n", __func__, __LINE__, len);
		return 0;
	}
	nPos += len;
	return &data[nPos - len];
}
unsigned int CByteBuffer::remain()
{
	return nLen - nPos;
}
