#pragma once
#include "DeviceInfo.h"
#include <string>
#include <vector>
#include <windows.h>

namespace DriverNest::Ui {

class DetailsPane {
public:
    static constexpr const wchar_t* WindowClass = L"DriverNestDetailsClass";

    static void RegisterClass();
    static HWND Create(HWND hwndParent, int x, int y, int cx, int cy, HWND hwndMain);

    static void Init(HWND hwnd, HWND hwndMain);
    static void SetDevice(HWND hwnd, const Hw::Device& device);
    static void SetCandidate(HWND hwnd,
                             const Hw::Device& device,
                             const std::wstring& candidateInf,
                             const std::wstring& candidateVersion,
                             const std::wstring& signer,
                             int score,
                             std::vector<std::wstring> reasonTags);

    static void RefreshFromSelection(HWND hwndList, HWND hwndDetails, HWND hwndMain);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

} // namespace DriverNest::Ui
