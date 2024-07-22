#include <windows.h>
#include <iostream>
#include <sstream>

#define PIPE_NAME L"\\\\.\\pipe\\AsyncPipe"
#define BUFFER_SIZE 1024

void ProcessClient() {
    HANDLE hPipe = CreateNamedPipe(
        PIPE_NAME,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        BUFFER_SIZE,
        BUFFER_SIZE,
        0,
        NULL);

    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateNamedPipe failed, GLE=" << GetLastError() << std::endl;
        return;
    }

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (overlapped.hEvent == NULL) {
        std::cerr << "CreateEvent failed, GLE=" << GetLastError() << std::endl;
        CloseHandle(hPipe);
        return;
    }

    BOOL connected = ConnectNamedPipe(hPipe, &overlapped);
    if (!connected && GetLastError() != ERROR_IO_PENDING) {
        std::cerr << "ConnectNamedPipe failed, GLE=" << GetLastError() << std::endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    std::cout << "Waiting for client to connect..." << std::endl;

    if (WaitForSingleObject(overlapped.hEvent, INFINITE) != WAIT_OBJECT_0) {
        std::cerr << "WaitForSingleObject failed, GLE=" << GetLastError() << std::endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    char buffer[BUFFER_SIZE] = { 0 };
    DWORD bytesRead = 0;

    BOOL readResult = ReadFile(hPipe, buffer, BUFFER_SIZE, &bytesRead, &overlapped);
    if (!readResult && GetLastError() != ERROR_IO_PENDING) {
        std::cerr << "ReadFile failed, GLE=" << GetLastError() << std::endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    if (WaitForSingleObject(overlapped.hEvent, INFINITE) != WAIT_OBJECT_0) {
        std::cerr << "WaitForSingleObject failed, GLE=" << GetLastError() << std::endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    if (!GetOverlappedResult(hPipe, &overlapped, &bytesRead, FALSE)) {
        std::cerr << "GetOverlappedResult failed, GLE=" << GetLastError() << std::endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    std::string receivedData(buffer, bytesRead);
    std::cout << "Received: " << receivedData << std::endl;

    std::istringstream iss(receivedData);
    double val1, val2;
    char op;
    iss >> val1 >> val2 >> op;

    double result = 0.0;
    switch (op) {
    case '+': result = val1 + val2; break;
    case '-': result = val1 - val2; break;
    case '*': result = val1 * val2; break;
    case '/': result = (val2 != 0) ? val1 / val2 : 0; break;
    default: std::cerr << "Invalid operator" << std::endl; break;
    }

    std::ostringstream oss;
    oss << result;
    std::string resultStr = oss.str();

    DWORD bytesWritten = 0;
    BOOL writeResult = WriteFile(hPipe, resultStr.c_str(), resultStr.size(), &bytesWritten, &overlapped);
    if (!writeResult && GetLastError() != ERROR_IO_PENDING) {
        std::cerr << "WriteFile failed, GLE=" << GetLastError() << std::endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    if (WaitForSingleObject(overlapped.hEvent, INFINITE) != WAIT_OBJECT_0) {
        std::cerr << "WaitForSingleObject failed, GLE=" << GetLastError() << std::endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    if (!GetOverlappedResult(hPipe, &overlapped, &bytesWritten, FALSE)) {
        std::cerr << "GetOverlappedResult failed, GLE=" << GetLastError() << std::endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    std::cout << "Result sent: " << resultStr << std::endl;

    CloseHandle(overlapped.hEvent);
    CloseHandle(hPipe);
}

int main() {
    while (true) {
        ProcessClient();
    }
    return 0;
}
