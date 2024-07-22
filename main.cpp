#include <windows.h>
#include <iostream>
#include <sstream>

#define PIPE_NAME L"\\\\.\\pipe\\AsyncPipe"
#define BUFFER_SIZE 1024
using namespace std;

void ProcessClient() {
    // !! FILE_FLAG_OVERLAPPED !! -> ASENKRON çalýþmayý saðlar... BU olmadan read,write iþlemleri tamamlanana kadar bekler //ReadFileEx ve WriteFileEx OVERLAPPED flag olmadan çalýþmaz
    HANDLE hPipe = CreateNamedPipe(
        PIPE_NAME,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, // DUPLEX -> Hem Client hem de Server read - write iznine sahip
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        BUFFER_SIZE,
        BUFFER_SIZE,
        0,
        NULL);

    if (hPipe == INVALID_HANDLE_VALUE) {
        cerr << "CreateNamedPipe failed, GLE=" << GetLastError() << endl;
        return;
    }

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (overlapped.hEvent == NULL) {
        cerr << "CreateEvent failed, GLE=" << GetLastError() << endl;
        CloseHandle(hPipe);
        return;
    }

    BOOL connected = ConnectNamedPipe(hPipe, &overlapped);
    if (!connected && GetLastError() != ERROR_IO_PENDING) {
       cerr << "ConnectNamedPipe failed, GLE=" << GetLastError() <<endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

   cout << "Waiting for client to connect..." << endl;
    //dwMilliseconds parametresi:
    // is INFINITE, the function will return only when the object is signaled.
    // is 0 , if object not signaled -> wait state'e girme hemen return
    // is non-zero, object signalled olana göre ya da interval geçene kadar fonksiyon bekler.

    if (WaitForSingleObject(overlapped.hEvent, INFINITE) != WAIT_OBJECT_0) {
        cerr << "WaitForSingleObject failed, GLE=" << GetLastError() << endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    char buffer[BUFFER_SIZE] = { 0 };
    DWORD bytesRead = 0;

    BOOL readResult = ReadFile(hPipe, buffer, BUFFER_SIZE, &bytesRead, &overlapped);
    if (!readResult && GetLastError() != ERROR_IO_PENDING) {
        cerr << "ReadFile failed, GLE=" << GetLastError() << endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    if (WaitForSingleObject(overlapped.hEvent, INFINITE) != WAIT_OBJECT_0) {
        cerr << "WaitForSingleObject failed, GLE=" << GetLastError() << endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    if (!GetOverlappedResult(hPipe, &overlapped, &bytesRead, FALSE)) {
        cerr << "GetOverlappedResult failed, GLE=" << GetLastError() << endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    string receivedData(buffer, bytesRead);
    cout << "Received: " << receivedData << endl;

    istringstream iss(receivedData);
    double val1, val2;
    char op;
    iss >> val1 >> val2 >> op;

    double result = 0.0;
    switch (op) {
    case '+': result = val1 + val2; break;
    case '-': result = val1 - val2; break;
    case '*': result = val1 * val2; break;
    case '/': result = (val2 != 0) ? val1 / val2 : 0; break;
    default: cerr << "Invalid operator" << endl; break;
    }

    ostringstream oss;
    oss << result;
    string resultStr = oss.str();

    DWORD bytesWritten = 0;
    BOOL writeResult = WriteFile(hPipe, resultStr.c_str(), resultStr.size(), &bytesWritten, &overlapped);
    if (!writeResult && GetLastError() != ERROR_IO_PENDING) {
        cerr << "WriteFile failed, GLE=" << GetLastError() << endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    if (WaitForSingleObject(overlapped.hEvent, INFINITE) != WAIT_OBJECT_0) {
        cerr << "WaitForSingleObject failed, GLE=" << GetLastError() << endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    if (!GetOverlappedResult(hPipe, &overlapped, &bytesWritten, FALSE)) {
        cerr << "GetOverlappedResult failed, GLE=" << GetLastError() << endl;
        CloseHandle(overlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

   cout << "Result sent: \n" << resultStr << endl;

    CloseHandle(overlapped.hEvent);
    CloseHandle(hPipe);
}

int main() {
    while (true) {
        ProcessClient();
    }
    return 0;
}
