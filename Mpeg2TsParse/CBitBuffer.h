#ifndef CBITBUFFER_H
#define CBITBUFFER_H
class CBitBuffer
{
private:
	unsigned char* m_buff;
	unsigned int buff_pos;
	unsigned int bit_pos;
public:
	CBitBuffer(unsigned char* buff);
	unsigned long get(unsigned char nCount);
	unsigned char get();
	unsigned int get_golomb();
};
#endif // CBITBUFFER_H