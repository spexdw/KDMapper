#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include "kdm/kdmapper.hpp"
#include "kdm/utils.hpp"
#include "kdm/intel_driver.hpp"

HANDLE iqvw64e_device_handle;

bool IsDriverLoaded(const std::string& driverName) {
    std::wstring devicePath = L"\\\\.\\" + std::wstring(driverName.begin(), driverName.end());
    HANDLE hDevice = CreateFileW(devicePath.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);

    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        return true;
    }
    return false;
}

bool UnloadCurrentDriver(const std::string& driverName) {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) return false;

    std::wstring wDriverName(driverName.begin(), driverName.end());
    SC_HANDLE service = OpenServiceW(scm, wDriverName.c_str(), SERVICE_STOP | DELETE);

    if (service) {
        SERVICE_STATUS status;
        ControlService(service, SERVICE_CONTROL_STOP, &status);
        CloseServiceHandle(service);
    }

    CloseServiceHandle(scm);

    HANDLE hDevice = CreateFileW((L"\\\\.\\" + wDriverName).c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
        return false;
    }

    return true;
}

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

    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 11);
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
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 15);
    std::cout << "Developed by SpeXD" << std::endl;

    std::string driver_path_str;
    std::cout << "[>] Enter full driver path (e.g. C:\\drivers\\test.sys): ";
    std::getline(std::cin, driver_path_str);

    std::wstring driver_path(driver_path_str.begin(), driver_path_str.end());

    DWORD fileAttributes = GetFileAttributesW(driver_path.c_str());
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
        std::cout << "[-] ERROR: File does not exist or is inaccessible." << std::endl;
        std::cout << "[+] Press any key to exit..." << std::endl;
        std::cin.get();
        return -1;
    }

    std::cout << "[+] Target driver: " << driver_path_str << std::endl;

    size_t lastSlash = driver_path_str.find_last_of("\\/");
    std::string fileName = (lastSlash != std::string::npos) ? driver_path_str.substr(lastSlash + 1) : driver_path_str;
    size_t lastDot = fileName.find_last_of(".");
    std::string driverBaseName = (lastDot != std::string::npos) ? fileName.substr(0, lastDot) : fileName;

    if (IsDriverLoaded(driverBaseName)) {
        std::cout << "[!] Driver already loaded: " << driverBaseName << std::endl;
        std::cout << "[*] Attempting to unload existing driver..." << std::endl;

        if (UnloadCurrentDriver(driverBaseName)) {
            std::cout << "[+] Existing driver unloaded successfully" << std::endl;
        }
        else {
            std::cout << "[-] Failed to unload existing driver. It may be in use or requires a reboot." << std::endl;
            std::cout << "[+] Press any key to exit..." << std::endl;
            std::cin.get();
            return -1;
        }
    }

    std::cout << "[*] Loading Intel driver..." << std::endl;
    iqvw64e_device_handle = intel_driver::Load();

    if (iqvw64e_device_handle == INVALID_HANDLE_VALUE) {
        std::cout << "[-] ERROR: Failed to load Intel driver." << std::endl;
        std::cout << "[+] Press any key to exit..." << std::endl;
        std::cin.get();
        return -1;
    }

    std::cout << "[+] Intel driver loaded successfully! Handle: 0x" << std::hex << (uint64_t)iqvw64e_device_handle << std::dec << std::endl;
    std::cout << "[*] Reading driver file into memory..." << std::endl;

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

    std::cout << "[*] Unloading Intel driver..." << std::endl;
    if (!intel_driver::Unload(iqvw64e_device_handle)) {
        std::cout << "[-] WARNING: Failed to unload Intel driver!" << std::endl;
    }
    else {
        std::cout << "[+] Intel driver unloaded successfully!" << std::endl;
    }

    std::cout << "[+] Operation completed successfully!" << std::endl;

    if (IsDriverLoaded(driverBaseName)) {
        std::cout << "[+] Driver is loaded and accessible" << std::endl;
    }
    else {
        std::cout << "[-] WARNING: Driver was mapped but does not appear to be accessible" << std::endl;
    }

    std::cout << "[+] Press any key to exit..." << std::endl;
    std::cin.get();

    return 0;
}