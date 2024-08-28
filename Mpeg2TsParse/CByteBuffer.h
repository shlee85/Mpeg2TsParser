#ifndef CBYTEBUFFER_H
#define CBYTEBUFFER_H
#include <string>
class CByteBuffer
{
private:
	unsigned int nLen;
	unsigned char* data;
	unsigned int nPos;
public:
	CByteBuffer();
	CByteBuffer(const std::string& str);
	CByteBuffer(unsigned char* p, unsigned int len);
	void set(unsigned char* p, unsigned int len);
	unsigned char get();
	unsigned short get2(bool bLittleEndian = false);
	unsigned int get3(bool bLittleEndian = false);
	unsigned int get4(bool bLittleEndian = false);
	unsigned char* gets(unsigned int len);
	unsigned int remain();
};
#endif // CBYTEBUFFER_H