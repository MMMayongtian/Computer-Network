#include <winsock2.h>
#include <thread>
#pragma comment (lib, "ws2_32.lib")  //���� ws2_32.dll
#include<iostream>
#include<iomanip>
using namespace std;

#define MAX_CLIENT 50 //���ͻ�����
#define MAX_BYTES 512 //��Ϣ����ֽ���

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
    time(&timep); //��ȡtime_t���͵ĵ�ǰʱ��
    char tmp[64];
    strftime(tmp, sizeof(tmp), "%Y/%m/%d %H:%M:%S", localtime(&timep));//�����ں�ʱ����и�ʽ��
    return tmp;
}
string nowTime;

DWORD WINAPI recvThreadFunc(LPVOID clientId) {
    //��ͻ���ͨѶ�����ܿͻ�����Ϣ��ת��
    int id = (int)clientId;
    while (true) {

        char pClnt[20];//˽����
        char buf_r[MAX_BYTES] = { 0 };//ԭ��Ϣ
        char buf_rt[MAX_BYTES] = { 0 };//˽����Ϣ
        bool pflag = false;//˽�ı�־

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
     
            if (buf_r[0] == '[') {//�ж��Ƿ�˽��
                int index = 1;
                while (buf_r[index] != ']' && index < 20) {//��ȡ˽���û���
                    pClnt[index - 1] = buf_r[index];
                    index++;
                }
                pClnt[index-1] = '\0';
                //cout <<"__ "<< pClnt << endl;
                strncpy(buf_rt, buf_r + index + 1, 100 - index);//��ȡ˽����Ϣ
 
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
   
    //��ͻ���ͨѶ�����ͻ��������
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
    //��������ʼ��ģ��
    nowTime = getTime();
    cout << nowTime << " [ INFO  ] Start Server Manger" << endl;
    cout << nowTime << " [ GET   ] Input the server name: ";
    cin >> serverName;
    nowTime = getTime();
    cout << nowTime << " [ GET   ] Input the port for wait the connections from clients: ";
    cin >> port;
    //��ʼ�� DLL
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        nowTime = getTime();
        cout << nowTime << " [ OK    ] WSAStartup Error : " << WSAGetLastError() << endl;
        return WSAGetLastError();
    }
    nowTime = getTime();
    cout << nowTime << " [ OK    ] WSAStartup Complete!" << endl;
    //������ʽ�׽���
    //��һ������ Э��أ�AF_INET��ipv4��AF_INET6��ipv6��AF_UNIX������ͨ�ţ�
    //�ڶ������� ���ͣ�SOCK_STREAM��TCP����SOCK_DGRAM��UDP���ݱ���SOCK_RAW��ԭʼ�׽��֣�
    //���������� һ������0����ȷ���׽���ʹ�õ�Э��غ�����ʱ�����������ֵ��Ϊ0  
    SOCKET servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (servSock == INVALID_SOCKET) {
        nowTime = getTime();
        cout << nowTime << " [ OK    ] socket error:" << WSAGetLastError() << endl;
        return WSAGetLastError();
    }
    nowTime = getTime();
    cout << nowTime << " [ OK    ] Socket Created!" << endl;

    //���׽��� �˿ں�ip
    struct sockaddr_in sockAddr;
    memset(&sockAddr, 0, sizeof(sockAddr));  //ÿ���ֽڶ���0���
    sockAddr.sin_family = AF_INET;  //ʹ��IPv4��ַ
    sockAddr.sin_addr.s_addr = inet_addr("0.0.0.0");  //�����IP��ַ
    sockAddr.sin_port = htons(port);  //�˿�

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
    //�������״̬
    listen(servSock, 20);
    nowTime = getTime();
    cout << nowTime << " [ INFO  ] Start listening..." << endl;

    //���տͻ�������
    while (true) {
        sockaddr_in  addrClnt;
        int len = sizeof(sockaddr_in);
        //���ܳɹ� ������ClientͨѶ��Scoket
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

            //�����߳� ��������clientͨѶ���׽���
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
    //�رռ����׽���
    closesocket(servSock);
    nowTime = getTime();
    cout << nowTime << " [ INFO  ] Close Socket!" << endl;
    //��ֹ DLL ��ʹ��
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