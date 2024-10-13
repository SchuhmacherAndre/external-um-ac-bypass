#include <Windows.h>
#include <cstdint>

// Macros
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004)
#define NtCurrentProcess ((HANDLE)(LONG_PTR)-1)

// Structures
typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWCH   Buffer;
} UNICODE_STRING, * PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG           Length;
    HANDLE          RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG           Attributes;
    PVOID           SecurityDescriptor;
    PVOID           SecurityQualityOfService;
} OBJECT_ATTRIBUTES, * POBJECT_ATTRIBUTES;

typedef struct _CLIENT_ID {
    PVOID UniqueProcess;
    PVOID UniqueThread;
} CLIENT_ID, * PCLIENT_ID;

typedef struct _SYSTEM_HANDLE {
    ULONG ProcessId;
    BYTE ObjectTypeNumber;
    BYTE Flags;
    USHORT Handle;
    PVOID Object;
    ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE, * PSYSTEM_HANDLE;

typedef struct _SYSTEM_HANDLE_INFORMATION {
    ULONG HandleCount;
    SYSTEM_HANDLE Handles[1];
} SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;

// Function prototypes
typedef NTSTATUS(NTAPI* _NtDuplicateObject)(
    HANDLE SourceProcessHandle,
    HANDLE SourceHandle,
    HANDLE TargetProcessHandle,
    PHANDLE TargetHandle,
    ACCESS_MASK DesiredAccess,
    ULONG Attributes,
    ULONG Options
    );

typedef NTSTATUS(NTAPI* _RtlAdjustPrivilege)(
    ULONG Privilege,
    BOOLEAN Enable,
    BOOLEAN CurrentThread,
    PBOOLEAN Enabled
    );

typedef NTSTATUS(NTAPI* _NtOpenProcess)(
    PHANDLE            ProcessHandle,
    ACCESS_MASK        DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    PCLIENT_ID         ClientId
    );

typedef NTSTATUS(NTAPI* _NtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );

// Utility function
template <typename T>
T ReadProcessMemory(HANDLE handle, uintptr_t address) noexcept {
    T buffer;
    ::ReadProcessMemory(handle, reinterpret_cast<PVOID>(address), &buffer, sizeof(buffer), nullptr);
    return buffer;
}

// Function to initialize the OBJECT_ATTRIBUTES structure
OBJECT_ATTRIBUTES InitObjectAttributes(PUNICODE_STRING name, ULONG attributes, HANDLE root_dir, PSECURITY_DESCRIPTOR security_descriptor) {
    OBJECT_ATTRIBUTES object_attributes;
    object_attributes.Length = sizeof(OBJECT_ATTRIBUTES);
    object_attributes.ObjectName = name;
    object_attributes.Attributes = attributes;
    object_attributes.RootDirectory = root_dir;
    object_attributes.SecurityDescriptor = security_descriptor;
    return object_attributes;
}


// Constants
constexpr ULONG SE_DEBUG_PRIVILEGE = 20;
constexpr ULONG ProcessHandleType = 0x7;
constexpr ULONG SystemHandleInformation = 16;

// Variables
SYSTEM_HANDLE_INFORMATION* g_handle_info = nullptr;
HANDLE g_process_handle = nullptr;
HANDLE g_hijacked_handle = nullptr;