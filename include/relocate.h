// Originally taken from https://github.com/hasherezade/demos
// I modified it though.
// Credits go to hasherezade!


#pragma once
#include <Windows.h>
#include "pe_hdrs_helper.h"

#define RELOC_32BIT_FIELD 3
#define RELOC_64BIT_FIELD 10

#define false 0
#define true 1
typedef int bool;

// Use with double parentheses
#ifdef PRINT_DEBUG
	#define DEBUG_PRINT(x) printf x
#else
	#define DEBUG_PRINT(x) do {} while (0)
#endif


typedef struct _BASE_RELOCATION_ENTRY {
    WORD Offset : 12;
    WORD Type: 4;
} BASE_RELOCATION_ENTRY;


bool has_relocations(BYTE *pe_buffer)
{
    IMAGE_DATA_DIRECTORY* relocDir = get_pe_directory(pe_buffer, IMAGE_DIRECTORY_ENTRY_BASERELOC);
    if (relocDir == NULL) {
        return false;
    }
    return true;
}


bool apply_reloc_block(BASE_RELOCATION_ENTRY *block, SIZE_T entriesNum, DWORD page, ULONGLONG oldBase, ULONGLONG newBase, PVOID modulePtr)
{
	DWORD *relocateAddr32;
	ULONGLONG *relocateAddr64;
	BASE_RELOCATION_ENTRY* entry = block;
	SIZE_T i = 0;
	for (i = 0; i < entriesNum; i++) {
		DWORD offset = entry->Offset;
		DWORD type = entry->Type;
		
		if (entry == NULL || type == 0) {
			break;
		}
				
		switch(type) {
			case RELOC_32BIT_FIELD:
				relocateAddr32 = (DWORD*) ((ULONG_PTR) modulePtr + page + offset);
				(*relocateAddr32) = (DWORD) (*relocateAddr32) - oldBase + newBase;
				entry = (BASE_RELOCATION_ENTRY*)((ULONG_PTR) entry + sizeof(WORD));	
				break;
			#ifdef X64
				case RELOC_64BIT_FIELD:
					relocateAddr64 = (ULONGLONG*) ((ULONG_PTR) modulePtr + page + offset);
					(*relocateAddr64) = ((ULONGLONG) (*relocateAddr64)) - oldBase + newBase;
					entry = (BASE_RELOCATION_ENTRY*)((ULONG_PTR) entry + sizeof(WORD));	
					break;
			#endif	
			default:
				DEBUG_PRINT(("Not supported relocations format at %d: %d\n", (int) i, type));
				return false;
		}								
				
	}
	//printf("[+] Applied %d relocations\n", (int) i);
	return true;		
}

				
bool apply_relocations(ULONGLONG newBase, ULONGLONG oldBase, PVOID modulePtr)
{
    IMAGE_DATA_DIRECTORY* relocDir = get_pe_directory(modulePtr, IMAGE_DIRECTORY_ENTRY_BASERELOC);
    if (relocDir == NULL) {
        DEBUG_PRINT(("Cannot relocate - application have no relocation table!\n"));
        return false;
    }
    DWORD maxSize = relocDir->Size;
    DWORD relocAddr = relocDir->VirtualAddress;

    IMAGE_BASE_RELOCATION* reloc = NULL;

    DWORD parsedSize = 0;
    while (parsedSize < maxSize) {
        reloc = (IMAGE_BASE_RELOCATION*)(relocAddr + parsedSize + (ULONG_PTR) modulePtr);
        parsedSize += reloc->SizeOfBlock;
		
		#ifdef X64
			if ((((ULONGLONG*) ((ULONGLONG) reloc->VirtualAddress)) == NULL) || (reloc->SizeOfBlock == 0)) {
				continue;
			}
		#else
			if ((((DWORD*) reloc->VirtualAddress) == NULL) || (reloc->SizeOfBlock == 0)) {
				continue;
			}
		#endif
				
        //printf("RelocBlock: %x %x\n", reloc->VirtualAddress, reloc->SizeOfBlock);
        
        size_t entriesNum = (reloc->SizeOfBlock - 2 * sizeof(DWORD))  / sizeof(WORD);
        DWORD page = reloc->VirtualAddress;

        BASE_RELOCATION_ENTRY* block = (BASE_RELOCATION_ENTRY*)((ULONG_PTR) reloc + sizeof(DWORD) + sizeof(DWORD));
        if (apply_reloc_block(block, entriesNum, page, oldBase, newBase, modulePtr) == false) {
            return false;
        }
    }
    return true;
}