#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

int main()
{
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
    return 0;
}
