#include <windows.h>
#include <iostream>
#include <sstream>

#define PIPE_NAME L"\\\\.\\pipe\\AsyncPipe"
#define BUFFER_SIZE 1024
using namespace std;

void CALLBACK ReadCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
void CALLBACK WriteCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);

HANDLE hPipe;
char buffer[BUFFER_SIZE];

void ProcessClient() {
    hPipe = CreateNamedPipe(
        PIPE_NAME,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
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

    OVERLAPPED connectOverlapped = {};
    connectOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (connectOverlapped.hEvent == NULL) {
        cerr << "CreateEvent failed, GLE=" << GetLastError() << endl;
        CloseHandle(hPipe);
        return;
    }

    BOOL connected = ConnectNamedPipe(hPipe, &connectOverlapped);
    if (!connected && GetLastError() != ERROR_IO_PENDING) {
        if (GetLastError() == ERROR_PIPE_CONNECTED) {
            SetEvent(connectOverlapped.hEvent);
        }
        else {
            cerr << "ConnectNamedPipe failed, GLE=" << GetLastError() << endl;
            CloseHandle(hPipe);
            CloseHandle(connectOverlapped.hEvent);
            return;
        }
    }

    cout << "Waiting for client to connect..." << endl;

    DWORD waitResult = WaitForSingleObjectEx(connectOverlapped.hEvent, INFINITE, TRUE);
    if (waitResult != WAIT_OBJECT_0) {
        cerr << "WaitForSingleObjectEx failed, GLE=" << GetLastError() << endl;
        CloseHandle(connectOverlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    CloseHandle(connectOverlapped.hEvent);

    OVERLAPPED readOverlapped = {};
    readOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (readOverlapped.hEvent == NULL) {
        cerr << "CreateEvent failed, GLE=" << GetLastError() << endl;
        CloseHandle(hPipe);
        return;
    }

    BOOL readResult = ReadFileEx(hPipe, buffer, BUFFER_SIZE, &readOverlapped, ReadCompletionRoutine);
    if (!readResult && GetLastError() != ERROR_IO_PENDING) {
        cerr << "ReadFileEx failed, GLE=" << GetLastError() << endl;
        CloseHandle(readOverlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    // Keep the main thread running to allow the completion routine to execute.
    while (true) {
        SleepEx(INFINITE, TRUE);
    }
}

void CALLBACK ReadCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
    if (dwErrorCode != ERROR_SUCCESS) {
        cerr << "ReadCompletionRoutine failed, GLE=" << dwErrorCode << endl;
        CloseHandle(lpOverlapped->hEvent);
        CloseHandle(hPipe);
        return;
    }

    int val1, val2;
    char op;

    string receivedData(buffer, dwNumberOfBytesTransfered);
    cout << "Received: " << receivedData << endl;

    istringstream iss(receivedData);
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

    OVERLAPPED writeOverlapped = {};
    writeOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (writeOverlapped.hEvent == NULL) {
        cerr << "CreateEvent failed, GLE=" << GetLastError() << endl;
        CloseHandle(lpOverlapped->hEvent);
        CloseHandle(hPipe);
        return;
    }

    BOOL writeResult = WriteFileEx(hPipe, resultStr.c_str(), resultStr.size(), &writeOverlapped, WriteCompletionRoutine);
    if (!writeResult && GetLastError() != ERROR_IO_PENDING) {
        cerr << "WriteFileEx failed, GLE=" << GetLastError() << endl;
        CloseHandle(lpOverlapped->hEvent);
        CloseHandle(writeOverlapped.hEvent);
        CloseHandle(hPipe);
        return;
    }

    CloseHandle(lpOverlapped->hEvent);
}

void CALLBACK WriteCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
    if (dwErrorCode != ERROR_SUCCESS) {
        cerr << "WriteCompletionRoutine failed, GLE=" << dwErrorCode << endl;
        CloseHandle(lpOverlapped->hEvent);
        CloseHandle(hPipe);
        return;
    }

    cout << "Result sent" << endl;

    CloseHandle(lpOverlapped->hEvent);
    CloseHandle(hPipe);
}

int main() {
    while (true) {
        ProcessClient();
    }
    return 0;
}
