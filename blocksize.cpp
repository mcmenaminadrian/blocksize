#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <expat.h>
#include <pthread.h>

using namespace std;

#define BUFFSZ 512

static char outputfile[BUFFSZ];
static pthread_mutex_t countLock = PTHREAD_MUTEX_INITIALIZER;
static map<int, long> overallCount;
static map<int, long> overallMemory;
static map<int, long> overallCode;
static vector<long> startAddress(18, 0);
static vector<long> lastAddress(18, 0);
static vector<int> sizeSoFar(18, 0);
static vector<bool> wasModify(18, false);
static vector<bool> wasCode(18, true);
static vector<bool> inAction(18, false);

//use this class to pass data to threads and parser
class SetPointers
{
	public:
	map<int, long>* lCount;
	map<int, long>* lMemory;
	map<int, long>* lCode;
	char* threadPath;
	int threadID;
};

void usage()
{
	cout << "USAGE:\n";
	cout << "pagestats [control file] [output file]\n";
}

void writeBlockRecord(SetPointers* sets)
{
	int threadNo = sets->threadID;
	map<int, long>::iterator itCount;
	map<int, long>::iterator itType;
	int sizeToFind = sizeSoFar[threadNo];

	itCount = sets->lCount->find(sizeToFind);
	if (itCount == sets->lCount->end() {
		sets->lCount->insert(pair<int, long>(sizeToFind, 0));
		itCount = sets->lCount->find(sizeToFind);
	}
	itCount->second++;
	if (wasModify[threadNo] {
		itCount->second++;
	}

	if (wasCode[threadNo]) {
		itType = sets->lCode->find(sizetoFind);
		if (itType == sets->lCode->end()) {
			sets->lCode->insert(pair<int, long>(sizeToFind, 0));
			itType = sets->lCode->find(sizeToFind);
		}
		itType->second++;
	} else {
		itType = sets->lMemory->find(sizeToFind);
		if (itType == sets->lMemory->end()) {
			sets->lMemory->insert(pair<int, long>(sizeToFind, 0));
			itType = sets->lMemory->find(sizeToFind);
		}
		itType->second++;
		if (wasModify[threadNo]) {
			itType->second++;
			wasModify[threadNo] == false;
		}
	}
	inAction[threadNo] = false;
}

static void XMLCALL
hackHandler(void *data, const XML_Char *name, const XML_Char **attr)
{
	SetPointers* sets = static_cast<SetPointers*>(data);
	long address;
	int size;
	int threadNo = sets->threadID;
	if (strcmp(name, "instruction") == 0 || strcmp(name, "load") == 0 ||
		strcmp(name, "modify")||strcmp(name, "store") == 0) {
		bool modify = false;
		if (strcmp(name, "modify") == 0 && inAction[threadNo]) {
			if (!wasModify[threadNo]) {
				writeBlockRecord(sets);
			}
			wasModify[threadNo] = true;
		}
		for (int i = 0; attr[i]; i += 2) {
			if (strcmp(attr[i], "address") == 0) {
				address = strtol(attr[i + 1], NULL, 16);
			} else if (strcmp(attr[i], "size") == 0) {
				size = strtol(attr[i + 1], NULL, 16);
			}
		}
		if (inAction[threadNo]) {
			if (address != lastAddress[threadNo] + 1) {
				writeBlockRecord(sets);
			} else if (strcmp(name, "instruction") == 0 &&
				!wasCode[threadNo]) {
				writeBlockRecord(sets);
			} else {
				sizeSoFar[threadNo] += size;
				lastAddress[threadNo] += size;	
		} else {
			inAction[threadNo] = true;
			lastAddress[threadNo] = address + size;
			sizeSoFar[threadNo] = size;
			if (strcmp(name, "instruction" == 0)) {
				wasCode[threadNo] = true;
			} else {
				wasCode[threadNo] = false;
			}
		}
	}
}

static void* hackMemory(void* tSets)
{
	//parse the file
	size_t len = 0;
	bool done;
	char data[BUFFSZ];
	SetPointers* threadSets = (SetPointers*) tSets; 
	XML_Parser parser_Thread = XML_ParserCreate("UTF-8");
	if (!parser_Thread) {
		cerr << "Could not create thread parser\n";
		return NULL;
	}
	XML_SetUserData(parser_Thread, tSets);
	XML_SetStartElementHandler(parser_Thread, hackHandler);
	FILE* threadXML = fopen(threadSets->threadPath, "r");
	if (threadXML == NULL) {
		cerr << "Could not open " << threadSets->threadPath << "\n";
		XML_ParserFree(parser_Thread);
		return NULL;
	}

	do {
		len = fread(data, 1, sizeof(data), threadXML);
		done = len < sizeof(data);
		if (XML_Parse(parser_Thread, data, len, 0) == 0) {
			enum XML_Error errcde = XML_GetErrorCode(parser_Thread);
			printf("ERROR: %s\n", XML_ErrorString(errcde));
			printf("Error at column number %lu\n",
				XML_GetCurrentColumnNumber(parser_Thread));
			printf("Error at line number %lu\n",
				XML_GetCurrentLineNumber(parser_Thread));
			return NULL;
		}
	} while(!done);

	pthread_mutex_lock(&countLock);
	cout << "Thread handled \n";
	map<int, long>::iterator itLocal;
	map<int, long>::iterator itGlobal;

	for (itLocal = threadSets->lCount->begin();
		itLocal != threadSets->lCount->end(); itLocal++) {
		int segment = itLocal->first;
		itGlobal = overallCount.find(segment);
		if (itGlobal != overallCount.end()){
			itGlobal->second += itLocal->second;
		} else {
			overallCount.insert(pair<int, long>(
				itLocal->first, itLocal->second));
		}
	}
	
	for (itLocal = threadSets->lMemory->begin();
		itLocal != threadSets->lMemory->end(); itLocal++) {
		int segment = itLocal->first;
		itGlobal = overallMemory.find(segment);
		if (itGlobal != overallMemory.end()){
			itGlobal->second += itLocal->second;
		} else {
			overallMemory.insert(pair<int, long>(
				itLocal->first, itLocal->second));
		}
	}

	for (itLocal = threadSets->lCode->begin();
		itLocal != threadSets->lCode->end(); itLocal++) {
		int segment = itLocal->first;
		itGlobal = overallCode.find(segment);
		if (itGlobal != overallCode.end()){
			itGlobal->second += itLocal->second;
		} else {
			overallCode.insert(pair<int, long>(
				itLocal->first, itLocal->second));
		}
	}
	pthread_mutex_unlock(&countLock);
	delete threadSets->lCount;
	delete threadSets->lMemory;
	delete threadSets->lCode;
	return NULL;
}



pthread_t* 
countThread(int threadID, char* threadPath)
{
	cout << "Handling thread " << threadID << "\n";
	//parse each file in parallel
	SetPointers* threadSets = new SetPointers();
	threadSets->lCount = new map<int, long>();
	threadSets->lMemory = new map<int, long>();
	threadSets->lCode = new map<int, long>();
	threadSets->threadPath = threadPath;
	threadSets->threadID = threadID;
	
	pthread_t* aThread = new pthread_t();
	
	pthread_create(aThread, NULL, hackMemory, (void*)threadSets);
	return aThread;
	
}

void joinup(pthread_t* t)
{
	pthread_join(*t, NULL);
}

void killoff(pthread_t* t)
{
	delete t;
}

static void XMLCALL
fileHandler(void *data, const XML_Char *name, const XML_Char **attr)
{

	vector<pthread_t*>* pThreads = static_cast<vector<pthread_t*>*>(data);
	
	int i;
	int threadID = 0;
	char* threadPath = NULL; 
	if (strcmp(name, "file") == 0) {
		for (i = 0; attr[i]; i += 2) {
			if (strcmp(attr[i], "thread") == 0) {
				threadID = atoi(attr[i + 1]);
				break;
			}
		}
		for (i = 0; attr[i]; i += 2) {
			if (strcmp(attr[i], "path") == 0) {
				threadPath = new char[BUFFSZ];		
				strcpy(threadPath, attr[i + 1]);
				break;
			}
		}
		pThreads->push_back(countThread(threadID, threadPath));
	}
}

int main(int argc, char* argv[])
{
	FILE* inXML;
	char data[BUFFSZ]; 
	size_t len = 0;
	int done;
	vector<pthread_t*> threads;

	if (argc < 3) {
		usage();
		exit(-1);
	}

	strcpy(outputfile, argv[2]);

	XML_Parser p_ctrl = XML_ParserCreate("UTF-8");
	if (!p_ctrl) {
		fprintf(stderr, "Could not create parser\n");
		exit(-1);
	}
	

	XML_SetUserData(p_ctrl, &threads);
	XML_SetStartElementHandler(p_ctrl, fileHandler);
	inXML = fopen(argv[1], "r");
	if (inXML == NULL) {
		fprintf(stderr, "Could not open %s\n", argv[1]);
		XML_ParserFree(p_ctrl);
		exit(-1);
	}


	cout << "Pagestats: which bits of pages are being touched\n";
	cout << "Copyright (c), Adrian McMenamin, 2014 \n";
	cout << "See https://github.com/mcmenaminadrian for licence details.\n";
	do {
		len = fread(data, 1, sizeof(data), inXML);
		done = len < sizeof(data);

		if (XML_Parse(p_ctrl, data, len, 0) == 0) {
			enum XML_Error errcde = XML_GetErrorCode(p_ctrl);
			printf("ERROR: %s\n", XML_ErrorString(errcde));
			printf("Error at column number %lu\n",
				XML_GetCurrentColumnNumber(p_ctrl));
			printf("Error at line number %lu\n",
				XML_GetCurrentLineNumber(p_ctrl));
			exit(-1);
		}
	} while(!done);
	for_each(threads.begin(), threads.end(), joinup);
	for_each(threads.begin(), threads.end(), killoff);
	
	map<int, int>::iterator it;
	ofstream overallFile;
	ofstream memoryFile;
	ofstream codeFile;
	
	overallFile.open(arv[2]);
	for (it = overallCount.begin(); it != overallCount.end(); it++)
	{
		overallFile << it->first << "," << it->second << "\n";
	}
	overallFile.close();

	memoryFile.open("memallocs.txt");
	for (it = overallMemory.begin(); it != overallMemory.end(); it++)
	{
		memoryFile << it->first << "," << it->second << "\n";
	}
	memoryFile.close();

	codeFile.open("codeallocs.txt");
	for (it = overallCode.begin(); it != overallCode.end(); it++)
	{
		codeFile << it->first << "," << it->second << "\n";
	}
	codeFile.close();

	cout << "Program completed \n";
	

}

