//
//    Copyright (C) Microsoft.  All rights reserved.
// Licensed under the terms described in the LICENSE file in the root of this project.
//

#include "stdafx.h"

// Helper class for calling WSL Functions:
// https://msdn.microsoft.com/en-us/library/windows/desktop/mt826874(v=vs.85).aspx
WslApiLoader g_wslApi(DistributionInfo::Name);

static HRESULT InstallDistribution();
static HRESULT SetDefaultUser(std::wstring_view userName);

HRESULT InstallDistribution()
{
    // Register the distribution.
    Helpers::PrintMessage(MSG_STATUS_INSTALLING);
    HRESULT hr = g_wslApi.WslRegisterDistribution();
    if (FAILED(hr)) {
        return hr;
    }

    // Delete /etc/resolv.conf to allow WSL to generate a version based on Windows networking information.
    DWORD exitCode;
    hr = g_wslApi.WslLaunchInteractive(L"/bin/rm /etc/resolv.conf", true, &exitCode);
    if (FAILED(hr)) {
        return hr;
    }

    // Create a user account.
    Helpers::PrintMessage(MSG_CREATE_USER_PROMPT);
    std::wstring userName;
    do {
        userName = Helpers::GetUserInput(MSG_ENTER_USERNAME, 32);

    } while (!DistributionInfo::CreateUser(userName));

    // Set this user account as the default.
    hr = SetDefaultUser(userName);
    if (FAILED(hr)) {
        return hr;
    }

    return hr;
}

HRESULT SetDefaultUser(std::wstring_view userName)
{
    // Query the UID of the given user name and configure the distribution
    // to use this UID as the default.
    ULONG uid = DistributionInfo::QueryUid(userName);
    if (uid == UID_INVALID) {
        return E_INVALIDARG;
    }

    HRESULT hr = g_wslApi.WslConfigureDistribution(uid, WSL_DISTRIBUTION_FLAGS_DEFAULT);
    if (FAILED(hr)) {
        return hr;
    }

    return hr;
}

int wmain(int argc, wchar_t const *argv[])
{
    // Update the title bar of the console window.
    SetConsoleTitleW(DistributionInfo::WindowTitle.c_str());

    // Initialize a vector of arguments.
    std::vector<std::wstring_view> arguments;
    for (int index = 1; index < argc; index += 1) {
        arguments.push_back(argv[index]);
    }

    // Ensure that the Windows Subsystem for Linux optional component is installed.
    DWORD exitCode = 1;
    if (!g_wslApi.WslIsOptionalComponentInstalled()) {
        Helpers::PrintMessage(MSG_MISSING_OPTIONAL_COMPONENT);
        if (arguments.empty()) {
            Helpers::PromptForInput();
        }

        return exitCode;
    }

    // Install the distribution if it is not already.
    HRESULT hr = S_OK;
    if (!g_wslApi.WslIsDistributionRegistered()) {
        hr = InstallDistribution();
        if (FAILED(hr)) {
            if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
                Helpers::PrintMessage(MSG_INSTALL_ALREADY_EXISTS);
            }

        } else {
            Helpers::PrintMessage(MSG_INSTALL_SUCCESS);
        }
    }

    // Parse the command line arguments.
    if (SUCCEEDED(hr)) {
        if (arguments.empty()) {
            hr = g_wslApi.WslLaunchInteractive(L"", false, &exitCode);

        } else if ((arguments[0] == L"run") ||
                   (arguments[0] == L"/c") ||
                   (arguments[0] == L"-c")) {

            std::wstring command;
            for (size_t index = 1; index < arguments.size(); index += 1) {
                command += L" ";
                command += arguments[index];
            }

            hr = g_wslApi.WslLaunchInteractive(command.c_str(), true, &exitCode);

        } else if (arguments[0] == L"config") {
            hr = E_INVALIDARG;
            if (arguments.size() == 3) {
                if (arguments[1] == L"--default-user") {
                    hr = SetDefaultUser(arguments[2]);
                }
            }

            if (SUCCEEDED(hr)) {
                exitCode = 0;
            }

        } else {
            Helpers::PrintMessage(MSG_USAGE);
            return exitCode;
        }
    }

    // If an error was encountered, print an error message.
    if (FAILED(hr)) {
        if (hr == HRESULT_FROM_WIN32(ERROR_LINUX_SUBSYSTEM_NOT_PRESENT)) {
            Helpers::PrintMessage(MSG_MISSING_OPTIONAL_COMPONENT);

        } else {
            Helpers::PrintErrorMessage(hr);
        }

        if (arguments.empty()) {
            Helpers::PromptForInput();
        }
    }

    return SUCCEEDED(hr) ? exitCode : 1;
}