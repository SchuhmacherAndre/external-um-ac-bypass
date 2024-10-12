#include <iostream>

int main()
{
    // gen random number 0-255
    std::srand(std::time(0));
    int randomNum = std::rand() % 256;

    // cast to ptr to truncate prefix 0s
    std::cout << "addr_: 0x" << std::hex << reinterpret_cast<uintptr_t>(&randomNum) << std::endl;
    std::cout << "value: 0x" << std::hex << randomNum << std::endl;

    return 1;
}