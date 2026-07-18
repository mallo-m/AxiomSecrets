#include "AxiomSecrets.h"

static libfsntfs_volume_t *NTFS_OpenDisk(wchar_t letter)
{
	int retval;
	libcerror_error_t *error = 0;
	libfsntfs_volume_t *volume;

	volume = (libfsntfs_volume_t *)malloc(sizeof(libfsntfs_volume_t));
	memset(&volume, 0, sizeof(libfsntfs_volume_t));
	libfsntfs_volume_initialize(&volume, &error);
	retval = libfsntfs_volume_open(
		volume,
		"\\\\.\\C:",
		LIBFSNTFS_OPEN_READ,
		&error
	);

	if (retval != 1)
	{
		printf("[!] Failed to open %c: volume\n", letter);
		exit(1);
	}
	printf("[+] Volume %c: opened\n", letter);
	return (volume);
}

static libfsntfs_file_entry_t *NTFS_ParseRoot(libfsntfs_volume_t *volume)
{
	int retval;
	libcerror_error_t *error = 0;
	libfsntfs_file_entry_t *root;

	root = (libfsntfs_file_entry_t *)malloc(sizeof(libfsntfs_file_entry_t));
	memset(&root, 0, sizeof(libfsntfs_file_entry_t));
	retval = libfsntfs_volume_get_root_directory(
		volume,
		&root,
		&error
	);

	if (retval != 1)
	{
		printf("[!] Failed to parse root directory of volume\n");
		exit(1);
	}
	printf("[+] Root volume parsed\n");
	return (root);
}

static libfsntfs_file_entry_t *NTFS_TraverseDirectories(libfsntfs_file_entry_t *root, const char **targets, size_t *filei)
{
	size_t i;
	int retval;
	libfsntfs_file_entry_t *result;
	libfsntfs_file_entry_t *current;
	libfsntfs_error_t *error;

	if (targets == NULL)
		return (0);

	i = 1;
	result = NULL;
	current = root;
	while (1)
	{
		if (targets[i] != NULL && targets[i + 1] == NULL)
			break;

		result = (libfsntfs_file_entry_t *)malloc(sizeof(libfsntfs_file_entry_t));
		memset(&result, 0, sizeof(libfsntfs_file_entry_t));
		retval = libfsntfs_file_entry_get_sub_file_entry_by_utf8_name(
			current,
			(const unsigned char *)targets[i],
			strlen(targets[i]),
			&result,
			&error
		);

		if (retval == 1)
		{
			printf("[+] Subentry %s found\n", targets[i]);
			retval = libfsntfs_file_entry_has_directory_entries_index(
				result,
				&error
			);
			if (retval == 0)
			{
				printf("[!] Error: the path specified a subdirectory of name %s, but it is not detected as one. Skipping this path\n", targets[i]);
				return (NULL);
			}
			current = result;
		}
		else {
			printf("[!] Subdirectory %s not found\n", targets[i]);
			return (NULL);
		}
		i++;
	}
	printf("[+] All subdirectories have been traversed\n");
	*filei = i;
	return (result);
}

static void NTFS_CopyFile(libfsntfs_file_entry_t *parent_directory, const char *filename, const char *savedir)
{
	HANDLE hFile;
	char filepath[MAX_PATH + 1];
	libfsntfs_file_entry_t *file;
	libfsntfs_error_t *error;
	BYTE filebuf[16 * 1024];
	size_t readbytes;

	file = (libfsntfs_file_entry_t *)malloc(sizeof(libfsntfs_file_entry_t));
	memset(&file, 0, sizeof(libfsntfs_file_entry_t));
	if (libfsntfs_file_entry_get_sub_file_entry_by_utf8_name(parent_directory, (const unsigned char *)filename, strlen(filename), &file, &error) == 1)
	{
		// Open a WRITE-TRUNCATE handle to destination file
		snprintf(filepath, sizeof(filepath), "%s\\%s.rdx", savedir, filename);
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

		// Loop through all data chunks, 16KB at a time
		while (1)
		{
			readbytes = libfsntfs_file_entry_read_buffer(
				file,
				filebuf,
				sizeof(filebuf),
				&error
			);
			for (size_t buf_i = 0; buf_i < 16 * 1024; buf_i++)
				filebuf[buf_i] ^= 0x42;
			WriteFile(
				hFile,
				filebuf,
				readbytes,
				NULL,
				NULL
			);
			if (readbytes <= 0) {
				break;
			}
		}

		CloseHandle(hFile);
	}
	else {
		printf("[!] Target file not found\n");
	}
}

int main(int argc, char **argv)
{
	int i;
	int argi;
	size_t filei;
	wchar_t volumeLetter;
	const char **targets;
	libfsntfs_volume_t *volume;
	libfsntfs_file_entry_t *root;
	libfsntfs_file_entry_t *tmp;
	libcerror_error_t *error = 0;
	
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
	while (argi < argc - 1)
	{
		// Validate path: path is absolute and first letter is a valid volume name
		if (PathIsRelativeA(argv[argi]))
		{
			printf("[!] Skipping file %s: only absolute paths are supported\n", argv[argi]);
			argi++;
			continue;
		}
		printf("[+] Processing file %s\n", argv[argi]);

		targets = UTILS_strsplit(argv[argi], '\\');
		mbtowc(&volumeLetter, targets[0], 1);
		volume = NTFS_OpenDisk(volumeLetter);
		root = NTFS_ParseRoot(volume);
		tmp = NTFS_TraverseDirectories(root, targets, &filei);
		if (tmp != NULL) {
			printf("[+] Copying file %s\n", targets[filei]);
			NTFS_CopyFile(tmp, targets[filei], argv[argc - 1]);
		}
		i = 0;
		while (targets && targets[i] != NULL)
		{
			free((void*)targets[i]);
			i++;
		}
		free(targets);
		libfsntfs_volume_close(volume, &error);
		argi++;
	}
	return (0);
}

