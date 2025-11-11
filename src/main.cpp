#include <fstream>
#include <shlwapi.h>
#include "AxiomSecrets.h"
#include "NTFSLib/NTFS.hpp"
#include "errhandlingapi.h"
#include "handleapi.h"

static CNTFSVolume NTFS_OpenDisk(wchar_t letter)
{
    CNTFSVolume volume(letter);

    if (!volume.IsVolumeOK())
    {
        printf("[!] Not a valid volume name\n");
        exit(1);
    }
    printf("[+] Volume opened\n");
    return (volume);
}

static CFileRecord NTFS_ParseRoot(CNTFSVolume *volume)
{
    CFileRecord fr(volume);
    fr.SetAttrMask(MASK_INDEX_ROOT | MASK_INDEX_ALLOCATION);
    if (!fr.ParseFileRecord(MFT_IDX_ROOT))
    {
        printf("Cannot read root directory of volume\n");
        exit(1);
    }
    printf("[+] Root directory parsed\n");

    if (!fr.ParseAttrs())
    {
        printf("[!] Cannot parse attributes\n");
        exit(1);
    }
    return (fr);
}

static int NTFS_TraverseDirectories(CFileRecord *fr, CIndexEntry *ie, const char **targets)
{
	size_t i;

	if (targets == NULL)
		return (0);

	i = 1;
	while (1)
	{
		if (targets[i] != NULL && targets[i + 1] == NULL)
			break;
		if (fr->FindSubEntry(targets[i], *ie))
		{
			printf("[+] Subdirectory %s found\n", targets[i]);
			if (!fr->ParseFileRecord(ie->GetFileReference()))
			{
				printf("[!] Cannot read root directory of volume\n");
				exit(1);
			}

			if (!fr->ParseAttrs())
			{
				if (fr->IsCompressed())
				    printf("[!] Compressed directory not supported yet\n");
				else if (fr->IsEncrypted())
				    printf("[!] Encrypted directory not supported yet\n");
				else
				    printf("[!] Cannot parse directory attributes\n");
				exit(1);
			}
		}
		else
		{
			printf("[!] Sub-directory %s not found\n", targets[i]);
			exit(1);
		}
		i++;
	}
	printf("[+] All subdirectories have been traversed\n");
	return (i);
}

static void NTFS_CopyFile(const char *filename, const char *savedir, CFileRecord *fr, CIndexEntry *ie)
{
	HANDLE hFile;
	char filepath[MAX_PATH + 1];

	if (fr->FindSubEntry(filename, *ie))
	{
		if (!fr->ParseFileRecord(ie->GetFileReference()))
		{
			printf("[!] Cannot read file: %s\n", filename);
			return;
		}
		printf("[+] File %s opened\n", filename);

		// Validate source file can be extracted
		fr->SetAttrMask(MASK_DATA);
		if (!fr->ParseAttrs())
		{
			if (fr->IsCompressed())
				printf("[!] Compressed file not supported yet\n");
			else if (fr->IsEncrypted())
				printf("[!] Encrypted file not supported yet\n");
			else if (fr->IsDirectory())
				printf("[!] Can not create backup of file %s: source is a directory\n", filename);
			else
				printf("[!] Cannot parse file attributes\n");
			return;
		}
		printf("[+] Attributes set for %s\n", filename);

		// Open a WRITE-TRUNCATE handle to destination file
		snprintf(filepath, sizeof(filepath), "%s\\%s.bak", savedir, filename);
		hFile = CreateFileA(
			filepath,
			FILE_GENERIC_READ | FILE_GENERIC_WRITE,
			FILE_SHARE_WRITE,
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			0
		);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			switch (errno)
			{
				case EACCES:
					printf("[!] Failed to open %s for writing: Permission denied\n", filepath);
					break;
				default:
					printf("[!] Unexpected error opening file for writing (%ld)\n", GetLastError());
					break;
			}
			return;
		}

		const CAttrBase* data = fr->FindStream();
		if (data)
		{
			std::ofstream _file;
			DWORD datalen;
			DWORD readLen;
			DWORD totalReadLen;
			size_t readSize;
			BYTE filebuf[16 * 1024];

			readLen = 0;
			totalReadLen = 0;
			datalen = (DWORD)data->GetDataSize();
			printf("[+] %s size: %ld\n", filename, datalen);
			while (totalReadLen < datalen)
			{
				readSize = datalen - totalReadLen;
				if (datalen - readLen > sizeof(filebuf))
					readSize = sizeof(filebuf);
				if (data->ReadData(totalReadLen, filebuf, readSize, &readLen))
				{
					WriteFile(
						hFile,
						filebuf,
						readLen,
						NULL,
						NULL
					);
					totalReadLen += readLen;
				}
				else
				{
					printf("[!] Read data error for: %s\n", filename);
					CloseHandle(hFile);
					return;
				}
			}
			CloseHandle(hFile);
			printf("[+] File %s backed up\n", filename);
		}
	}
	else {
		printf("[!] File %s not found\n", filename);
	}
}

int main(int argc, char **argv)
{
	int argi;
	int filei;
	size_t i = 0;
	wchar_t volumeLetter;
	const char **targets;
	const char *savedir;

	// Check that invocation matches usage
	if (argc < 3)
	{
		printf("[!] Usage: %s FILEPATH_1 [{FILEPATH_2}, {FILEPATH_3}, ...] SAVEDIR_PATH\n", argv[0]);
		return (1);
	}

	// Validate that the last argument is a valid path to a folder
	printf("[+] Validating destination folder %s\n", argv[argc - 1]);
	DWORD ftyp = GetFileAttributesA(argv[argc - 1]);
	if (ftyp == INVALID_FILE_ATTRIBUTES)
	{
		printf("[!] Can not write files to %s: ", argv[argc - 1]);
		switch (GetLastError())
		{
			case ENOENT:
				printf("no such file or directory\n");
				break;
			case ESRCH:
				printf("parent directory does not exist\n");
				break;
			case EACCES:
				printf("access denied\n");
				break;
			default:
				printf("unknown error (0x%lx)\n", GetLastError());
				break;
		}
		return (1);
	}
	if (!(ftyp & FILE_ATTRIBUTE_DIRECTORY))
	{
		printf("[!] Can not write files to %s: not a directory\n", argv[argc - 1]);
		return (1);
	}

	// Process each file passed as argument and save it into argv[argc - 1] folder
	argi = 1;
	savedir = argv[argc - 1];
	while (argi < argc - 1)
	{
		// Validate path: path is absolute and first letter is a valid volume name
		if (PathIsRelativeA(argv[argi]))
		{
			printf("[!] Skipping file %s: only absolute paths are supported\n", argv[argi]);
			argi++;
			continue;
		}

		targets = UTILS_strsplit(argv[argi], '\\');
		mbtowc(&volumeLetter, targets[0], 1);
		CIndexEntry ie;
		CNTFSVolume volume = NTFS_OpenDisk(volumeLetter);
		CFileRecord fr = NTFS_ParseRoot(&volume);
		filei = NTFS_TraverseDirectories(&fr, &ie, targets);
		NTFS_CopyFile(targets[filei], savedir, &fr, &ie);
		while (targets && targets[i] != NULL)
		{
			free((void*)targets[i]);
			i++;
		}
		free(targets);
		argi++;
	}
	return (0);
}

