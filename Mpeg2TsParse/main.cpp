#include <stdio.h> 
#include <iostream>
#include <vector>

#include "TsParser.h"
#include "TsCommon.h"
#include "player.h"

int main(int agrc, char* argv[]) {
	std::cout << "Mpeg2-TS Parser" << std::endl;
	std::string sourcePath = "C:\\Work\\STREAM\\ts\\test2.ts";
	std::string outputPath = "ouputs/parser.txt";

	//TODO. TS파일 경로 입력
#if 0
	TsParser tsParser(sourcePath, outputPath);
	tsParser.Init();
#else
	Player play;
	play.mainLoop();
	//play.start(sourcePath, outputPath);
#endif

	return 0;
}