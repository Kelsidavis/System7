/* Minimal UEFI loader that checks for a "blessed" System folder and
 * verifies it contains both "System" and "Finder" entries.
 *
 * This implementation uses GNU-EFI (efi.h, efilib.h). It is intentionally
 * conservative: it tries common blessed markers (a file named "BLESSED") and
 * then falls back to checking common folder layouts:
 *  - root contains both "System" and "Finder"
 *  - "System Folder" contains "System" and "Finder"
 *
 * If the checks succeed, the loader prints a success message. The loader does
 * not yet load or jump to the kernel; that will be added in a follow-up.
 */

#include <efi.h>
#include <efilib.h>

// Try several candidate names for a blessed marker file
static CHAR16 *blessed_candidates[] = {
    L"BLESSED",
    L"blessed",
    L"blessed.txt",
    L"EFI\\BOOT\\blessed",
    NULL
};

static EFI_STATUS read_blessed_path(EFI_FILE_PROTOCOL *Root, CHAR16 *outPath, UINTN outPathLen) {
    EFI_STATUS Status;
    for (CHAR16 **cand = blessed_candidates; *cand != NULL; ++cand) {
        EFI_FILE_PROTOCOL *File;
        Status = Root->Open(Root, &File, *cand, EFI_FILE_MODE_READ, 0);
        if (EFI_ERROR(Status)) continue;

        // Read up to outPathLen-1 ASCII chars and NUL-terminate
        CHAR8 buffer[256];
        UINTN bufSize = sizeof(buffer);
        Status = File->Read(File, &bufSize, buffer);
        File->Close(File);
        if (EFI_ERROR(Status) || bufSize == 0) continue;

        // Ensure NUL termination
        if (bufSize >= sizeof(buffer)) bufSize = sizeof(buffer) - 1;
        buffer[bufSize] = '\0';

        // Convert ASCII to CHAR16 (naive conversion)
        UINTN i = 0;
        for (; i < outPathLen - 1 && i < bufSize; ++i) {
            outPath[i] = (CHAR16)buffer[i];
        }
        outPath[i] = L'\0';
        return EFI_SUCCESS;
    }
    return EFI_NOT_FOUND;
}

static EFI_STATUS check_dir_has_system_and_finder(EFI_FILE_PROTOCOL *Root, CHAR16 *dirPath) {
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *Dir = NULL;

    // Open the candidate directory relative to root
    Status = Root->Open(Root, &Dir, dirPath, EFI_FILE_MODE_READ, EFI_FILE_DIRECTORY);
    if (EFI_ERROR(Status)) {
        // Maybe dirPath is an absolute or contains backslashes; try walking the path
        // We'll support a single backslash-separated level: "Parent\\Sub"
        CHAR16 *sep = StrStr(dirPath, L"\\");
        if (!sep) return Status;
        // Split
        CHAR16 parent[256];
        CHAR16 child[256];
        UINTN idx = sep - dirPath;
        StrnCpy(parent, dirPath, idx);
        parent[idx] = L'\0';
        StrnCpy(child, sep + 1, 255);
        parent[255] = L'\0'; child[255] = L'\0';

        EFI_FILE_PROTOCOL *ParentDir = NULL;
        Status = Root->Open(Root, &ParentDir, parent, EFI_FILE_MODE_READ, EFI_FILE_DIRECTORY);
        if (EFI_ERROR(Status)) return Status;
        Status = ParentDir->Open(ParentDir, &Dir, child, EFI_FILE_MODE_READ, EFI_FILE_DIRECTORY);
        ParentDir->Close(ParentDir);
        if (EFI_ERROR(Status)) return Status;
    }

    // Check for "System"
    EFI_FILE_PROTOCOL *F = NULL;
    Status = Dir->Open(Dir, &F, L"System", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) { Dir->Close(Dir); return EFI_NOT_FOUND; }
    F->Close(F);

    // Check for "Finder"
    Status = Dir->Open(Dir, &F, L"Finder", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) { Dir->Close(Dir); return EFI_NOT_FOUND; }
    F->Close(F);

    Dir->Close(Dir);
    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    InitializeLib(ImageHandle, SystemTable);

    Print(L"EFI Loader: Checking blessed system folder...\n");

    // Locate Simple FileSystem protocol handles
    UINTN HandleCount = 0;
    EFI_HANDLE *Handles = NULL;
    Status = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol,
                               &gEfiSimpleFileSystemProtocolGuid, NULL,
                               &HandleCount, &Handles);
    if (EFI_ERROR(Status) || HandleCount == 0) {
        Print(L"No file systems found: %r\n", Status);
        return EFI_NOT_FOUND;
    }

    CHAR16 blessedPath[512];
    BOOLEAN found = FALSE;

    for (UINTN h = 0; h < HandleCount; ++h) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Sfs = NULL;
        Status = uefi_call_wrapper(BS->HandleProtocol, 3, Handles[h],
                                   &gEfiSimpleFileSystemProtocolGuid, (VOID**)&Sfs);
        if (EFI_ERROR(Status) || Sfs == NULL) continue;

        EFI_FILE_PROTOCOL *Root = NULL;
        Status = uefi_call_wrapper(Sfs->OpenVolume, 2, Sfs, &Root);
        if (EFI_ERROR(Status) || Root == NULL) continue;

        // 1) Try blessed marker file
        blessedPath[0] = L'\0';
        Status = read_blessed_path(Root, blessedPath, sizeof(blessedPath)/sizeof(CHAR16));
        if (!EFI_ERROR(Status)) {
            Print(L"Found blessed marker: %s\n", blessedPath);
            Status = check_dir_has_system_and_finder(Root, blessedPath);
            if (!EFI_ERROR(Status)) {
                Print(L"Blessed system folder OK: %s\n", blessedPath);
                found = TRUE;
                Root->Close(Root);
                break;
            } else {
                Print(L"Blessed path exists but missing System/Finder: %r\n", Status);
            }
        }

        // 2) Check root directly for System and Finder
        Status = check_dir_has_system_and_finder(Root, L"\\");
        if (!EFI_ERROR(Status)) {
            Print(L"Root contains System and Finder -> accepted as blessed\n");
            found = TRUE;
            Root->Close(Root);
            break;
        }

        // 3) Check "System Folder"
        Status = check_dir_has_system_and_finder(Root, L"System Folder");
        if (!EFI_ERROR(Status)) {
            Print(L"System Folder contains System and Finder -> accepted as blessed\n");
            found = TRUE;
            Root->Close(Root);
            break;
        }

        Root->Close(Root);
    }

    FreePool(Handles);

    if (!found) {
        Print(L"ERROR: No blessed system folder found containing both 'System' and 'Finder'.\n");
        // Wait for user before exiting (so message is readable)
        Print(L"Press any key to continue...\n");
        EFI_INPUT_KEY Key;
        UINTN EventIndex;
        uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
        uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &EventIndex);
        uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
        return EFI_NOT_FOUND;
    }

    Print(L"EFI Loader: blessed check passed. (Next: load kernel)\n");

    // TODO: Load kernel image and jump to kernel entry point

    // Stall for a short while so the user sees the message
    uefi_call_wrapper(BS->Stall, 1, 1000000);

    return EFI_SUCCESS;
}
