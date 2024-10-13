#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>
#include <string>
#include "utils.h"



// Function to get a process ID from a process name (improvement needed)
DWORD GetProcessIdByName(LPCSTR process_name) {
    // Create a process snapshot
    HANDLE snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, false);
    if (snapshot_handle != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 process_entry;
        ZeroMemory(process_entry.szExeFile, sizeof(process_entry.szExeFile));

        // Iterate through process entries
        do {
            if (lstrcmpi(process_entry.szExeFile, process_name) == 0) {
                CloseHandle(snapshot_handle);
                return process_entry.th32ProcessID;
            }
        } while (Process32Next(snapshot_handle, &process_entry));

        CloseHandle(snapshot_handle);
    }
    return 0;
}


bool IsHandleValid(HANDLE handle) {
    if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Check if the handle is within the reserved range for pseudo-handles
    if (reinterpret_cast<ULONG_PTR>(handle) & 0xFFFFFFFF00000000) {
        return true;
    }

    // Use DuplicateHandle to verify if the handle is valid
    HANDLE duplicate = NULL;
    BOOL result = DuplicateHandle(
        GetCurrentProcess(),
        handle,
        GetCurrentProcess(),
        &duplicate,
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS
    );

    if (result) {
        CloseHandle(duplicate);
        return true;
    }

    // Check if the error is due to an invalid handle
    DWORD error = GetLastError();
    return (error != ERROR_INVALID_HANDLE);
}

void CleanUpAndExit(const std::string& error_message, DWORD error_code = GetLastError()) {
    // Log the error message and code
    std::cerr << "Error: " << error_message << std::endl;
    if (error_code != 0) {
        char* error_text = nullptr;
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error_code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&error_text,
            0,
            NULL
        );
        if (error_text) {
            std::cerr << "Error code " << error_code << ": " << error_text << std::endl;
            LocalFree(error_text);
        }
        else {
            std::cerr << "Unknown error code: " << error_code << std::endl;
        }
    }

    if (g_process_handle && g_process_handle != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(g_process_handle)) {
            DWORD close_error = GetLastError();
            std::cerr << "Failed to close process handle. Error code: " << close_error << std::endl;
        }
        g_process_handle = NULL;
    }

    std::exit(EXIT_FAILURE);
}


HANDLE bypass(DWORD target_process_id) {
    HMODULE ntdll_module = GetModuleHandleA("ntdll");

    // Get necessary function addresses from ntdll.dll
    auto RtlAdjustPrivilege = reinterpret_cast<_RtlAdjustPrivilege>(GetProcAddress(ntdll_module, "RtlAdjustPrivilege"));
    auto NtQuerySystemInformation = reinterpret_cast<_NtQuerySystemInformation>(GetProcAddress(ntdll_module, "NtQuerySystemInformation"));
    auto NtDuplicateObject = reinterpret_cast<_NtDuplicateObject>(GetProcAddress(ntdll_module, "NtDuplicateObject"));
    auto NtOpenProcess = reinterpret_cast<_NtOpenProcess>(GetProcAddress(ntdll_module, "NtOpenProcess"));

    BOOLEAN old_privileges;  // Stores old privileges
    RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, TRUE, FALSE, &old_privileges);  // Enable SeDebugPrivilege

    // Initialize OBJECT_ATTRIBUTES
    OBJECT_ATTRIBUTES obj_attributes = InitObjectAttributes(nullptr, 0, nullptr, nullptr);
    CLIENT_ID client_id = { 0 };

    DWORD size = sizeof(SYSTEM_HANDLE_INFORMATION);
    g_handle_info = reinterpret_cast<SYSTEM_HANDLE_INFORMATION*>(new byte[size]);
    ZeroMemory(g_handle_info, size);

    NTSTATUS status = 0;
    // Increase allocated memory until it fits all handles
    do {
        delete[] g_handle_info;
        size *= 1.5;

        try {
            g_handle_info = reinterpret_cast<SYSTEM_HANDLE_INFORMATION*>(new byte[size]);
        }
        catch (const std::bad_alloc&) {
            CleanUpAndExit("Memory allocation failed");
        }

        Sleep(1);
    } while ((status = NtQuerySystemInformation(SystemHandleInformation, g_handle_info, size, nullptr)) == STATUS_INFO_LENGTH_MISMATCH);

    if (!NT_SUCCESS(status)) {
        CleanUpAndExit("NtQuerySystemInformation failed");
    }

    // Iterate through each handle in the system
    for (unsigned int i = 0; i < g_handle_info->HandleCount; ++i) {
        static DWORD open_handle_count = 0;
        GetProcessHandleCount(GetCurrentProcess(), &open_handle_count);

        // Detect handle leakage
        if (open_handle_count > 50) {
            CleanUpAndExit("Handle leakage detected");
        }

        // Skip invalid handles
        if (!IsHandleValid(reinterpret_cast<HANDLE>(g_handle_info->Handles[i].Handle))) {
            continue;
        }

        // Only consider process handles (type 0x7)
        if (g_handle_info->Handles[i].ObjectTypeNumber != ProcessHandleType) {
            continue;
        }

        // Set client ID for the process with the handle to the target
        client_id.UniqueProcess = reinterpret_cast<DWORD*>(g_handle_info->Handles[i].ProcessId);

        if (g_process_handle) {
            CloseHandle(g_process_handle);
        }

        // Open process with duplicate handle permissions
        status = NtOpenProcess(&g_process_handle, PROCESS_DUP_HANDLE, &obj_attributes, &client_id);
        if (!IsHandleValid(g_process_handle) || !NT_SUCCESS(status)) {
            continue;
        }

        // Duplicate the handle from the target process
        status = NtDuplicateObject(g_process_handle, reinterpret_cast<HANDLE>(g_handle_info->Handles[i].Handle), NtCurrentProcess, &g_hijacked_handle, PROCESS_ALL_ACCESS, 0, 0);
        if (!IsHandleValid(g_hijacked_handle) || !NT_SUCCESS(status)) {
            continue;
        }

        // Check if the duplicated handle belongs to the target process
        if (GetProcessId(g_hijacked_handle) != target_process_id) {
            CloseHandle(g_hijacked_handle);
            continue;
        }

        break;
    }

    CleanUpAndExit("Success");
    return g_hijacked_handle;
}