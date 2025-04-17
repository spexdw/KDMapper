#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include "kdm/kdmapper.hpp"
#include "kdm/utils.hpp"
#include "kdm/intel_driver.hpp"

HANDLE iqvw64e_device_handle;

LONG WINAPI SimpleCrashHandler(EXCEPTION_POINTERS* ExceptionInfo) {
    if (ExceptionInfo && ExceptionInfo->ExceptionRecord)
        std::cout << "[!!] Crash at address 0x" << ExceptionInfo->ExceptionRecord->ExceptionAddress
        << " by 0x" << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode << std::endl;
    else
        std::cout << "[!!] Crash detected" << std::endl;

    if (iqvw64e_device_handle)
        intel_driver::Unload(iqvw64e_device_handle);

    return EXCEPTION_EXECUTE_HANDLER;
}

int main() {
    SetConsoleTitle(L"KDMapper");
    SetUnhandledExceptionFilter(SimpleCrashHandler);

    // Display banner
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 11); // Cyan
    std::cout << R"(
  _  _______  __  __                             
 | |/ /  __ \|  \/  |                            
 | ' /| |  | | \  / | __ _ _ __  _ __   ___ _ __ 
 |  < | |  | | |\/| |/ _` | '_ \| '_ \ / _ \ '__|
 | . \| |__| | |  | | (_| | |_) | |_) |  __/ |   
 |_|\_\_____/|_|  |_|\__,_| .__/| .__/ \___|_|   
                          | |   | |              
                          |_|   |_|              
    )" << std::endl;
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15); // White
    std::cout << "Developed by SpeXD" << std::endl;

    // Get driver path
    std::string driver_path_str;
    std::cout << "[>] Enter full driver path (e.g. C:\\drivers\\test.sys): ";
    std::getline(std::cin, driver_path_str);

    // Convert string to wstring
    std::wstring driver_path(driver_path_str.begin(), driver_path_str.end());

    // Check if file exists
    DWORD fileAttributes = GetFileAttributesW(driver_path.c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        std::cout << "[-] ERROR: File does not exist or is inaccessible." << std::endl;
        std::cout << "[+] Press any key to exit..." << std::endl;
        std::cin.get();
        return -1;
    }

    std::cout << "[+] Target driver: " << driver_path_str << std::endl;
    std::cout << "[*] Loading Intel driver..." << std::endl;

    // Load Intel driver
    iqvw64e_device_handle = intel_driver::Load();

    if (iqvw64e_device_handle == INVALID_HANDLE_VALUE) {
        std::cout << "[-] ERROR: Failed to load Intel driver." << std::endl;
        std::cout << "[+] Press any key to exit..." << std::endl;
        std::cin.get();
        return -1;
    }

    std::cout << "[+] Intel driver loaded successfully! Handle: 0x" << std::hex << (uint64_t)iqvw64e_device_handle << std::dec << std::endl;
    std::cout << "[*] Reading driver file into memory..." << std::endl;

    // Read driver file
    std::vector<uint8_t> raw_image = { 0 };
    if (!utils::ReadFileToMemory(driver_path, &raw_image)) {
        std::cout << "[-] ERROR: Failed to read driver file." << std::endl;
        intel_driver::Unload(iqvw64e_device_handle);
        std::cout << "[+] Press any key to exit..." << std::endl;
        std::cin.get();
        return -1;
    }

    std::cout << "[+] Driver read successfully! Size: " << raw_image.size() << " bytes" << std::endl;
    std::cout << "[*] Mapping driver into kernel..." << std::endl;

    // Map driver
    NTSTATUS exitCode = 0;
    if (!kdmapper::MapDriver(iqvw64e_device_handle, raw_image.data(), 0, 0, false, true, kdmapper::AllocationMode::AllocatePool, false, nullptr, &exitCode)) {
        std::cout << "[-] ERROR: Failed to map driver!" << std::endl;
        intel_driver::Unload(iqvw64e_device_handle);
        std::cout << "[+] Press any key to exit..." << std::endl;
        std::cin.get();
        return -1;
    }

    std::cout << "[+] Driver mapped successfully!" << std::endl;
    std::cout << "[+] Driver entry returned: 0x" << std::hex << exitCode << std::dec << std::endl;

    // Unload Intel driver
    std::cout << "[*] Unloading Intel driver..." << std::endl;
    if (!intel_driver::Unload(iqvw64e_device_handle)) {
        std::cout << "[-] WARNING: Failed to unload Intel driver!" << std::endl;
    }
    else {
        std::cout << "[+] Intel driver unloaded successfully!" << std::endl;
    }

    std::cout << "[+] Operation completed successfully!" << std::endl;
    std::cout << "[+] Press any key to exit..." << std::endl;
    std::cin.get();

    return 0;
}