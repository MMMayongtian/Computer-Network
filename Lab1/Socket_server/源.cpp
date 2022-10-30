#include <winsock2.h>
#include <thread>
#pragma comment (lib, "ws2_32.lib")  //加载 ws2_32.dll
#include<iostream>
#include<iomanip>
using namespace std;

#define MAX_CLIENT 50 //最大客户端数
#define MAX_BYTES 512 //消息最大字节数

struct Client {
    SOCKET clientSocket;
    char clientName[20];
    bool flag = true;
};

Client Clients[MAX_CLIENT];
char serverName[20];
int clientNum = 0;
int port = 1234;

string getTime(){
    time_t timep;
    time(&timep); //获取time_t类型的当前时间
    char tmp[64];
    strftime(tmp, sizeof(tmp), "%Y/%m/%d %H:%M:%S", localtime(&timep));//对日期和时间进行格式化
    return tmp;
}
string nowTime;

DWORD WINAPI recvThreadFunc(LPVOID clientId) {
    //与客户端通讯，接受客户端信息并转发
    int id = (int)clientId;
    while (true) {

        char pClnt[20];//私聊名
        char buf_r[MAX_BYTES] = { 0 };//原消息
        char buf_rt[MAX_BYTES] = { 0 };//私聊消息
        bool pflag = false;//私聊标志

        int ret = recv(Clients[id].clientSocket, buf_r, MAX_BYTES, 0);
        if (ret <= 0) {
            nowTime = getTime();
            cout<< nowTime<< " [ LEFT  ] " << Clients[id].clientName << " disconnect!" << endl;
            Clients[id].flag = false;

            for (int i = 0; i < clientNum; i++) {
                if (!Clients[i].flag) {
                    continue;
                }
                char buf_s[MAX_BYTES] = { 0 };
                strcpy(buf_s, Clients[id].clientName);
                strcat(buf_s, "just left,bye!");
                send(Clients[i].clientSocket, buf_s, MAX_BYTES, 0);
            }

            return 0;
        }
        else if (ret > 0) {
     
            if (buf_r[0] == '[') {//判断是否私聊
                int index = 1;
                while (buf_r[index] != ']' && index < 20) {//截取私聊用户名
                    pClnt[index - 1] = buf_r[index];
                    index++;
                }
                pClnt[index-1] = '\0';
                //cout <<"__ "<< pClnt << endl;
                strncpy(buf_rt, buf_r + index + 1, 100 - index);//截取私聊信息
 
                for (int i = 0; i < clientNum; i++) {
                    if (strcmp(pClnt, Clients[i].clientName) == 0) {
                        //cout << "cname:" << Clients[i].clientName << endl;

                        char buf_s[MAX_BYTES] = { 0 };
                        strcpy(buf_s, Clients[id].clientName);
                        strcat(buf_s, " [private chat]: ");
                        strcat(buf_s, buf_rt);
                        send(Clients[i].clientSocket, buf_s, MAX_BYTES, 0);

                        char buf_s2[MAX_BYTES] = { 0 };
                        strcpy(buf_s2, "[private chat to ");
                        strcat(buf_s2, Clients[i].clientName);
                        strcat(buf_s2, "]: ");
                        strcat(buf_s2, buf_rt);
                        send(Clients[id].clientSocket, buf_s2, MAX_BYTES, 0);
                        pflag = true;
                    }
                }
            }
        }
        if (pflag) {
            nowTime = getTime();
            cout << nowTime << " [ PSEND ] " << Clients[id].clientName << " to " << pClnt << ": " << buf_rt << endl;
        }
        else {
            nowTime = getTime();
            cout << nowTime << " [ SEND  ] " << Clients[id].clientName << ": " << buf_r << endl;
            char buf_s[MAX_BYTES] = { 0 };
            for (int j = 0; j < clientNum; j++)
            {
                strcpy(buf_s, Clients[id].clientName);
                strcat(buf_s, ": ");
                strcat(buf_s, buf_r);
                if (!Clients[j].flag) {
                    continue;
                }
                int ret = send(Clients[j].clientSocket, buf_s, MAX_BYTES, 0);
            }

        }
    }
    return 0;
}
DWORD WINAPI sendThreadFunc(LPVOID clientId) {
   
    //与客户端通讯，发送或接受数据
    int id = (int)clientId;
    while (true) {
        char str[MAX_BYTES];
        cin >> str;
        char buf[MAX_BYTES];
        strcpy(buf, serverName);
        strcat(buf, ": ");
        strcat(buf, str);
        for(int i = 0; i < clientNum; i++) {
            if (!Clients[i].flag) {
                continue;
            }
            int ret = send(Clients[i].clientSocket, buf, MAX_BYTES, 0);
        }
        nowTime = getTime();
        cout << nowTime << " [ SSEND ] " << serverName << ": " << str << endl;
    }
    return 0;
}
SOCKET init() {
    //服务器初始化模块
    nowTime = getTime();
    cout << nowTime << " [ INFO  ] Start Server Manger" << endl;
    cout << nowTime << " [ GET   ] Input the server name: ";
    cin >> serverName;
    nowTime = getTime();
    cout << nowTime << " [ GET   ] Input the port for wait the connections from clients: ";
    cin >> port;
    //初始化 DLL
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        nowTime = getTime();
        cout << nowTime << " [ OK    ] WSAStartup Error : " << WSAGetLastError() << endl;
        return WSAGetLastError();
    }
    nowTime = getTime();
    cout << nowTime << " [ OK    ] WSAStartup Complete!" << endl;
    //创建流式套接字
    //第一个参数 协议簇（AF_INET，ipv4；AF_INET6，ipv6；AF_UNIX，本机通信）
    //第二个参数 类型（SOCK_STREAM，TCP流；SOCK_DGRAM，UDP数据报；SOCK_RAW，原始套接字）
    //第三个参数 一般设置0，当确定套接字使用的协议簇和类型时，这个参数的值就为0  
    SOCKET servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (servSock == INVALID_SOCKET) {
        nowTime = getTime();
        cout << nowTime << " [ OK    ] socket error:" << WSAGetLastError() << endl;
        return WSAGetLastError();
    }
    nowTime = getTime();
    cout << nowTime << " [ OK    ] Socket Created!" << endl;

    //绑定套接字 端口和ip
    struct sockaddr_in sockAddr;
    memset(&sockAddr, 0, sizeof(sockAddr));  //每个字节都用0填充
    sockAddr.sin_family = AF_INET;  //使用IPv4地址
    sockAddr.sin_addr.s_addr = inet_addr("0.0.0.0");  //具体的IP地址
    sockAddr.sin_port = htons(port);  //端口

    if (bind(servSock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR))) {
        nowTime = getTime();
        cout << nowTime << " [ OK    ] bind Error:" << WSAGetLastError() << endl;
        return WSAGetLastError();
    }
    nowTime = getTime();
    cout << nowTime << " [ OK    ] Bind Success!" << endl;

    return servSock;
}
void listen(SOCKET servSock) {
    //进入监听状态
    listen(servSock, 20);
    nowTime = getTime();
    cout << nowTime << " [ INFO  ] Start listening..." << endl;

    //接收客户端请求
    while (true) {
        sockaddr_in  addrClnt;
        int len = sizeof(sockaddr_in);
        //接受成功 返回与Client通讯的Scoket
        Clients[clientNum].clientSocket = accept(servSock, (SOCKADDR*)&addrClnt, &len);
        nowTime = getTime();
        cout << nowTime << " [ INFO  ] Start accept..." << endl;

        if (Clients[clientNum].clientSocket != INVALID_SOCKET) {
            nowTime = getTime();
            cout << nowTime << " [ OK    ] Accept Success!" << endl;

            send(Clients[clientNum].clientSocket, serverName, 20, 0);
            char buf_r[20] = { 0 };
            int ret = recv(Clients[clientNum].clientSocket, buf_r, 20, 0);
            strcpy(Clients[clientNum].clientName, buf_r);
            nowTime = getTime();
            cout << nowTime << " [ JOIN  ] " << Clients[clientNum].clientName << " just joins, welcome!" << endl;

            if (clientNum == 0) {
                HANDLE hThread = CreateThread(NULL, 0, sendThreadFunc, (LPVOID)clientNum, 0, NULL);
                CloseHandle(hThread);
                nowTime = getTime();
                cout << nowTime << " [ INFO  ] Send thread create success!" << endl;
            }

            //创建线程 并传入与client通讯的套接字
            HANDLE hThread = CreateThread(NULL, 0, recvThreadFunc, (LPVOID)clientNum, 0, NULL);
            CloseHandle(hThread);
            nowTime = getTime();
            cout << nowTime << " [ INFO  ] Client thread create success!" << endl;
            cout << nowTime << " [ INFO  ] Continue to listening..." << endl;

            clientNum++;
            for (int i = 0; i < clientNum; i++) {
                if (!Clients[i].flag) {
                    continue;
                }
                char buf_s[MAX_BYTES] = { 0 };
                strcpy(buf_s, Clients[clientNum - 1].clientName);
                strcat(buf_s," just joins,welcome!");
                send(Clients[i].clientSocket, buf_s, MAX_BYTES, 0);
            }
        }
        else {
            nowTime = getTime();
            cout << nowTime << " [ INFO  ] Connect error!" << endl;
            break;
        }
    }
}
void close(SOCKET servSock) {
    //关闭监听套接字
    closesocket(servSock);
    nowTime = getTime();
    cout << nowTime << " [ INFO  ] Close Socket!" << endl;
    //终止 DLL 的使用
    WSACleanup();
    nowTime = getTime();
    cout << nowTime << " [ INFO  ] Clean up WSA!" << endl;

}
int main() {

    SOCKET servSock = init();

    listen(servSock);

    system("pause");

    close(servSock);

    return 0;
}