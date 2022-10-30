#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
#include<iostream>
using namespace std;
#define MAX_BYTES 512
char clientName[20];
char serverName[20];
int port = 1234;
string IP;
bool flag = true;

string getTime() {
    time_t timep;
    time(&timep); //获取time_t类型的当前时间
    char tmp[64];
    strftime(tmp, sizeof(tmp), "%Y/%m/%d %H:%M:%S", localtime(&timep));//对日期和时间进行格式化
    return tmp;
}
string nowTime;

DWORD WINAPI recvThreadFunc(LPVOID lpThreadParameter)
{
    SOCKET sock = (SOCKET)lpThreadParameter;
    while (true) {
        char buf_r[MAX_BYTES] = { 0 };
        int ret = recv(sock, buf_r, MAX_BYTES, 0);
        time_t t = time(NULL);
        struct tm p;
        localtime_s(&p, &t);
        if (ret > 0) {
            nowTime = getTime();
            cout << nowTime << " " << buf_r << endl;
        }
        else {
            flag = false;
            return 0;
        }
    }
}
DWORD WINAPI sendThreadFunc(LPVOID lpThreadParameter)
{
    SOCKET sock = (SOCKET)lpThreadParameter;
    while (true) {
        char buf[MAX_BYTES];
        cin >> buf;
        if (strcmp(buf, "exit") == 0) {
            flag = false;
            return 0;
        }
        int ret = send(sock, buf, MAX_BYTES, 0);
        if (ret <= 0) {
            return 0;
        }
    }
}

SOCKET init() {
    nowTime = getTime();
    cout << nowTime << " 请输入客户端名称: ";
    cin >> clientName;
    nowTime = getTime();
    cout << nowTime << " 请输入服务器IP地址: ";
    cin >> IP;
    nowTime = getTime();
    cout << nowTime << " 请输入端口号: ";
    cin >> port;

    //初始化 DLL
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        nowTime = getTime();
        cout << nowTime << " WSAStartup Error:" << WSAGetLastError() << endl;
        flag = false;
        return WSAGetLastError();
    }

    //创建流式套接字
    SOCKET servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (servSock == INVALID_SOCKET) {
        nowTime = getTime();
        cout << nowTime << " socket error:" << WSAGetLastError() << endl;
        flag = false;
        return WSAGetLastError();
    }
    return servSock;
}
void connect(SOCKET servSock) {
    //向服务器发起请求
    struct sockaddr_in sockAddr;
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(IP.c_str());
    sockAddr.sin_port = htons(port);

    if (connect(servSock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
        nowTime = getTime();
        cout << nowTime << " connect error:" << GetLastError() << endl;
        flag = false;
        return ;
    }

    //接收服务器传回的数据
    char buf_r[20] = { 0 };
    recv(servSock, buf_r, 20, 0);
    strcpy(serverName, buf_r);
    nowTime = getTime();
    cout << nowTime << " 链接服务器 " << serverName << " 成功!" << endl;
    send(servSock, clientName, 20, 0);

    HANDLE hThread = CreateThread(NULL, 0, recvThreadFunc, (LPVOID)servSock, 0, NULL);
    CloseHandle(hThread);
    hThread = CreateThread(NULL, 0, sendThreadFunc, (LPVOID)servSock, 0, NULL);
    CloseHandle(hThread);
    nowTime = getTime();
    cout << nowTime << " 创建线程成功!" << endl;

}
void close(SOCKET servSock) {


    //关闭套接字
    closesocket(servSock);

    //终止使用 DLL
    WSACleanup();
}
int main(){

    SOCKET servSock = init();
    
    connect(servSock);

    while (flag) {
        
    }

    nowTime = getTime();
    cout << nowTime << " 链接已断开..." << endl;

    close(servSock);

    system("pause");


   
    return 0;
}