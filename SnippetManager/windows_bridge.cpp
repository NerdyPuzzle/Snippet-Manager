#include "windows_bridge.h"
#include <Windows.h>
#include "imgui.h"


const char* GetClipboardTextW(void* user_data)
{
    const char* text = nullptr;
    if (OpenClipboard(NULL))
    {
        HANDLE hClipboardData = GetClipboardData(CF_TEXT);
        if (hClipboardData != NULL)
        {
            char* clipboardText = static_cast<char*>(GlobalLock(hClipboardData));
            if (clipboardText != nullptr)
            {
                text = _strdup(clipboardText);
                GlobalUnlock(hClipboardData);
            }
        }
        CloseClipboard();
    }
    return text;
}

// Define your own SetClipboardText function
void SetClipboardTextW(void* user_data, const char* text)
{
    if (OpenClipboard(NULL))
    {
        EmptyClipboard();
        HGLOBAL hClipboardData = GlobalAlloc(GMEM_DDESHARE, strlen(text) + 1);
        if (hClipboardData != NULL)
        {
            char* pchData = static_cast<char*>(GlobalLock(hClipboardData));
            if (pchData != nullptr)
            {
                strcpy_s(pchData, strlen(text) + 1, text);
                GlobalUnlock(hClipboardData);
                SetClipboardData(CF_TEXT, hClipboardData);
            }
        }
        CloseClipboard();
    }
}