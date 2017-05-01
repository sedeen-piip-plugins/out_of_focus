#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <fstream>
#include <fstream>
class logger
{
public:
	std::ofstream myfile;
	int debugMode;
	logger(void);
	~logger(void);
	void close();
	void setDebugMode(int Val);
	void setFilename(std::string filename);
	void print(std::string  text, int reqVal = 0);
};

