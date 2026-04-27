#pragma once

// Use ur own memory.h because it's not safe.

#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>

namespace mem
{
    inline HANDLE pHandle;
    inline uintptr_t procID;

    uintptr_t GetProcID(const wchar_t* processName);   // ex: cs2.exe
    uintptr_t GetModuleBase(uintptr_t processID, const wchar_t* module);   // ex: client.dll | engine.dll

    inline void ReadBuffer(uintptr_t address, void* buffer, size_t size)
    {
        ReadProcessMemory(pHandle, (LPCVOID)address, buffer, size, nullptr);
    }


    inline std::string ReadString(uintptr_t address, size_t max_length = 128)
    {
        if (address == 0) return "";

        std::vector<char> buffer(max_length);
        SIZE_T bytesRead = 0;

        if (ReadProcessMemory(pHandle, reinterpret_cast<LPCVOID>(address), buffer.data(), max_length, &bytesRead))
        {

            if (bytesRead > 0 && bytesRead <= max_length)
            {
                buffer[bytesRead - 1] = '\0';
            }
            else
            {
                buffer[max_length - 1] = '\0';
            }

            return std::string(buffer.data());
        }

        return "";
    }


    inline std::wstring ReadWString(uintptr_t address, size_t max_length = 128)
    {
        if (address == 0) return L"";

        std::vector<wchar_t> buffer(max_length);
        SIZE_T bytesRead = 0;

        if (ReadProcessMemory(pHandle, reinterpret_cast<LPCVOID>(address), buffer.data(), max_length * sizeof(wchar_t), &bytesRead))
        {

            size_t charCount = bytesRead / sizeof(wchar_t);
            if (charCount > 0 && charCount <= max_length)
            {
                buffer[charCount - 1] = L'\0';
            }
            else
            {
                buffer[max_length - 1] = L'\0';
            }

            return std::wstring(buffer.data());
        }

        return L"";
    }

    template <typename R>
    R ReadMem(uintptr_t address)
    {
        R mit;
        ReadProcessMemory(pHandle, (LPCVOID)address, &mit, sizeof(R), nullptr);
        return mit;
    }

    template <typename R>
    bool WriteToMem(uintptr_t address, R value)
    {
        return WriteProcessMemory(pHandle, (LPVOID)address, &value, sizeof(R), nullptr);
    }
}