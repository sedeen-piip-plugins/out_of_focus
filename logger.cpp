#include "logger.h"


logger::logger(void)
{
	debugMode=1;
}


logger::~logger(void)
{

}

void logger::close(void)
{
	myfile.close();
}

void logger::setDebugMode(int Val)
{
	debugMode=Val;
}


void logger::setFilename(std::string filename)
{

	if (debugMode) {
		myfile.open (filename + "_log.txt");
	}

}


void logger::print(std::string text,int reqVal)
{
	if (debugMode> reqVal) {
					myfile << text;
					myfile.flush();
	}
}
