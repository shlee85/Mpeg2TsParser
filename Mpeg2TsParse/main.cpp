#include <stdio.h> 
#include <iostream>
#include <vector>

#include "TsParser.h"
#include "TsCommon.h"

int main(int agrc, char* argv[]) {
	std::cout << "Mpeg2-TS Parser" << std::endl;
	std::string sourcePath = "C:\\Work\\STREAM\\ts\\test2.ts";
	std::string outputPath = "ouputs/parser.txt";

	//TODO. TS���� ��� �Է�
	TsParser tsParser(sourcePath, outputPath);
	tsParser.Init();
	
	return 0;
}