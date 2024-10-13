#include <iostream>
#include <thread>
#include <atomic>
#include <Windows.h>
#include <chrono>

typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );

int main()
{

    // Load ntdll.dll manually
    HMODULE hNtdll = LoadLibraryA("ntdll.dll");

    if (!hNtdll) {
        std::cerr << "Failed to load ntdll.dll." << std::endl;
        return 1;
    }

    // Get the address of NtQuerySystemInformation
    NtQuerySystemInformation_t NtQuerySystemInformation =
        (NtQuerySystemInformation_t)GetProcAddress(hNtdll, "NtQuerySystemInformation");

    if (!NtQuerySystemInformation) {
        std::cerr << "Failed to get address of NtQuerySystemInformation." << std::endl;
        return 1;
    }

    ULONG len = 0;
    NtQuerySystemInformation(0, nullptr, 0, &len);  // Query system information

    std::cout << "Called NtQuerySystemInformation." << std::endl;


    // gen random number 0-255
    std::srand(std::time(0));
    std::atomic<int> randomNum(std::rand() % 256);

    std::cout << "addr_: 0x" << std::hex << reinterpret_cast<uintptr_t>(&randomNum) << std::endl;
    std::cout << "value: 0x" << std::hex << randomNum.load() << std::endl;

    int oldValue = randomNum.load();

    std::thread monitorThread([&]() {
        while (true)
        {
            int newValue = randomNum.load();
            if (newValue != oldValue)
            {
                std::cout << "value: 0x" << std::hex << newValue << std::endl;
                oldValue = newValue;
            }
           
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        });

   
    for (;;)
    {
        
    }

    
    monitorThread.join();
    FreeLibrary(hNtdll);
    return 0;
}
