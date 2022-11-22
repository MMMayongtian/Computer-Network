//#include "Packet.h"
#pragma once
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")  //���� ws2_32.dll
#include<time.h>
#include<ostream>
#include<fstream>
#include<string>
#include<iostream>
using namespace std;
#define SERVER_PORT 3000
#define BUF_SIZE 1024
#define RETRANSMISSION_TIMES 10
#define WAIT_TIME 100

#define CLOSE 0 
#define LISTEN 1
#define SYN_RCVD 3
#define ESTABLISHED 4
#define CLOSE_WAIT 6
#define LAST_ACK 8
#define WND_SIZE 100

#pragma pack(1)//���ֽڶ���
struct Packet//���ĸ�ʽ
{
	//SYN=0x01  ACK=0x02  FIN=0x04  PACKET=0x08  StartFile=0x10  EndFile=0x20 File=0x40
	int Flags = 0;		//��־λ
	DWORD SendIP;		//���Ͷ�IP
	DWORD RecvIP;		//���ն�IP
	u_short Port;		//�˿�
	u_short	Protocol;	//Э������
	int Seq = -1;
	int Ack = -1;
	int index = -1;			//�ļ���Ƭ���� �ڼ�Ƭ
	int length = -1;			//Data�γ���
	//int window = -1;
	u_short checksum;	//У���
	char Data[BUF_SIZE];//�������ݶ�
};
#pragma pack()//�ָ�4Byte��ַ��ʽ

SOCKET sockServer;
struct sockaddr_in sockAddr;
int addr_len = sizeof(struct sockaddr_in);

string nowTime;
string Log;
string path = "C:\\Users\\000\\Desktop\\�����ļ�\\�����ļ�\\";
int state = CLOSE;		//״̬
int sendSeq = 0;		//��Ϣ���к�
char recvData[10000][BUF_SIZE];	//���յ�������
char buf[10000][1058];
int bufIndex = 0;
clock_t clockStart;		//��ʱ��
clock_t clockEnd;

int wndStart;
int wndEnd;
int wndPointer;
int Seq;
string getTime() {
	time_t timep;
	time(&timep); //��ȡtime_t���͵ĵ�ǰʱ��
	char tmp[64];
	strftime(tmp, sizeof(tmp), "%Y/%m/%d %H:%M:%S", localtime(&timep));//�����ں�ʱ����и�ʽ��
	return tmp;
}

bool checkSYN(Packet* t)
{
	if (t->Flags & 0x01)
		return 1;
	return 0;
}
void setSYN(Packet* t)
{
	t->Flags |= 0x01;
}
bool checkACK(Packet* t) {
	if (t->Flags & 0x02)
		return 1;
	return 0;
}
void setACK(Packet* t, Packet* u) {
	t->Flags |= 0x02;
	t->Ack = u->Seq + 1;
}
bool checkFIN(Packet* t)
{
	if (t->Flags & 0x04)//flag���ҵڶ�λ��0����1
		return 1;
	return 0;
}
void setFIN(Packet* t)
{
	t->Flags |= 0x04;
}
bool checkPacket(Packet* t) {
	if (t->Flags & 0x08)//flag���ҵڶ�λ��0����1
		return 1;
	return 0;
}
void setPacket(Packet* t) {
	t->Flags |= 0x08;
}

bool checkCheckSum(Packet* t) {
	int sum = 0;
	u_char* packet = (u_char*)t;
	for (int i = 0; i < 16; i++) {
		sum += packet[2 * i] << 8 + packet[2 * i + 1];
		while (sum > 0xFFFF) {
			int sumh = sum >> 16;
			int suml = sum & 0xFFFF;
			sum = sumh + suml;
		}
	}
	//У����뱨���и��ֶ���ӣ�����0xFFFF��У��ɹ�
	if (t->checksum + (u_short)sum == 0xFFFF) {
		return true;
	}
	return false;
}
void setCheckSum(Packet* t) {
	int sum = 0;
	u_char* packet = (u_char*)t;
	for (int i = 0; i < 16; i++)//ȡ���ĵ�ǰ16�飬ÿ��16bit������32�ֽ�256bit
	{
		sum += packet[2 * i] << 8 + packet[2 * i + 1];
		while (sum > 0xFFFF) {
			int sumh = sum >> 16;
			int suml = sum & 0xFFFF;
			sum = sumh + suml;
		}
	}
	t->checksum = ~(u_short)sum;//��λȡ��
}

bool checkStart(Packet* t)
{
	if (t->Flags & 0x10)//flag���ҵڶ�λ��0����1
		return 1;
	return 0;
}
bool checkEnd(Packet* t)
{
	if (t->Flags & 0x20)//flag���ҵڶ�λ��0����1
		return 1;
	return 0;
}
void setStart(Packet* t)
{
	t->Flags |= 0x10;
}
void setEnd(Packet* t)
{
	t->Flags |= 0x20;
}
bool checkFile(Packet* t)
{
	if (t->Flags & 0x40)//flag���ҵڶ�λ��0����1
		return 1;
	return 0;
}
void setFile(Packet* t)
{
	t->Flags |= 0x40;
}

string PacketLog(Packet* toLog) {
	string log = "[ ";
	if (checkSYN(toLog)) {
		log += "SYN ";
	}
	if (checkACK(toLog)) {
		log += "ACK ";
	}
	if (checkFIN(toLog)) {
		log += "FIN ";
	}
	if (checkStart(toLog)) {
		log += "SF ";
	}
	if (checkFile(toLog)) {
		log += "FILE ";
	}
	if (checkEnd(toLog)) {
		log += "EF ";
	}
	log += "] ";
	log += "Seq=";
	log += to_string(toLog->Seq);
	if (checkACK(toLog)) {
		log += " Ack=";
		log += to_string(toLog->Ack);
	}
	if (checkFile(toLog)) {
		log += " Index=";
		log += to_string(toLog->index);
	}
	log += " CheckNum=";
	log += to_string(toLog->checksum);
	return log;
}

class windoewsControl {
	bool wndAck[WND_SIZE];
public:
	bool isAck(int index) {
		return wndAck[index % WND_SIZE];
	}
	void clear(int index) {
		wndAck[index % WND_SIZE] = 0;
	}
	void set(int index) {
		wndAck[index % WND_SIZE] = 1;
	}
} wnd;
void sendPacket(Packet* sendP);
bool recvPacket(Packet* recvP);


void sendfile() {
	for (int i = wndStart; i < wndEnd; i++) {
		Packet t;
		wnd.set(i);
		sendPacket(&t);
	}
}
bool recvfile(Packet*recvSF) {
	Seq = recvSF->Seq;

	wndStart = Seq;
	wndEnd = wndStart + WND_SIZE;
	wndPointer = wndStart + 1;
	Packet recvP;
	while (wndPointer < wndEnd) {
		while (1) {
			if (recvPacket(&recvP)&&recvP.Seq==wndPointer) {
				wndPointer++;
				Packet sendACK;
				setACK(&sendACK, &recvP);
				sendPacket(&sendACK);
				if (wndPointer == wndEnd) {
					wndStart = wndEnd;
					wndEnd = wndStart + WND_SIZE;
				}
			}
		}
	}
	return 1;
}
void sendPacket(Packet* sendP) {
	setPacket(sendP);//��ʾ������Ϣ���ڣ����������ʽrecv�ж��Ƿ���յ�����Ϣ
	setCheckSum(sendP);//����У���
	//sendP->Flags |= 0x40;
	if (sendto(sockServer, (char*)sendP, sizeof(Packet), 0, (struct sockaddr*)&sockAddr, sizeof(sockaddr)) != SOCKET_ERROR){
		nowTime = getTime();
		Log = PacketLog(sendP);
		cout << nowTime << " [ SEND  ] " << Log << endl;
	}
}
bool recvPacket(Packet* recvP) {
	memset(recvP, 0, sizeof(sizeof(&recvP)));
	int re = recvfrom(sockServer, (char*)recvP, sizeof(Packet), 0, (struct sockaddr*)&sockAddr, &addr_len);
	//if (checkPacket(recvP) && checkCheckSum(recvP)) {
	if (re != -1 && checkCheckSum(recvP)) {
		nowTime = getTime();
		Log = PacketLog(recvP);
		cout << nowTime << " [ RECV  ] " << Log << endl;
		return true;
	}
	return false;
}

bool stopWaitRecv(Packet* recvP, Packet* sendP) {
	while (1) {
		if (recvPacket(recvP)) {		//�յ��Է���������ϢrecvP
			if (checkCheckSum(recvP)) {	//У��ͼ��ɹ�
				setACK(sendP, recvP);	//�ظ������յ���ϢrecvP��ack��ϢsendP
				sendP->Seq = sendSeq++;
				sendPacket(sendP);
				memset((char*)sendP, 0, sizeof(Packet));
				return 1;
			}
			cout << "У��ʧ��" << endl;
		}
	}
	return 0;
}
bool stopWaitSend(Packet* sendP, Packet* recvP)
{
	sendPacket(sendP);
	if (state == CLOSE_WAIT) {
		state = LAST_ACK;
	}

	clockStart = clock();	//��ʼ��ʱ
	int retransNum = 0;		//�ط�����
	while (1) {
		if (recvPacket(recvP)) {
			if (checkACK(recvP) && recvP->Ack == (sendP->Seq + 1)) {	//�յ���sendP��ȷ�ϱ���
				return 1;
			}
		}
		clockEnd = clock();
		if (retransNum == RETRANSMISSION_TIMES) {	//��������ط�����
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "The number of retransmission times is too many, fail to send!" << endl;
			return 0;
		}
		if ((clockEnd - clockStart) >= WAIT_TIME) {	//��ʱ�ط�
			retransNum++;
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "Too long waiting time, retrans the " << retransNum << " time!" << endl;
			sendPacket(sendP);
			clockStart = clock();
		}
	}
	return 0;//����ʧ��
}

bool breakConnection(Packet finPacket) {
	if (state == CLOSE) {
		return 1;
	}
	state = CLOSE_WAIT;
	nowTime = getTime();
	cout << nowTime << " [ STATE ] " << "Start disconnecting, Enter the " << state << " state!" << endl;

	Packet sendFIN, recvACK;
	sendFIN.Seq = sendSeq++;
	setFIN(&sendFIN);
	setACK(&sendFIN, &finPacket);
	sendPacket(&sendFIN);

	clockStart = clock();	//��ʼ��ʱ
	int retransNum = 0;		//�ط�����
	while (1) {
		if (recvPacket(&recvACK)) {
			if (checkACK(&recvACK) && recvACK.Ack == sendSeq) {	//�յ���sendP��ȷ�ϱ���
				state = CLOSE;
				nowTime = getTime();
				cout << nowTime << " [ BREAK ] " << "The connection is down! Enter the " << state << " state!" << endl;
				sendSeq = 0;
				return 1;
			}
		}
		clockEnd = clock();
		if (retransNum == RETRANSMISSION_TIMES) {	//��������ط�����
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "The number of retransmission times is too many, fail to send!" << endl;
			return 0;
		}
		if ((clockEnd - clockStart) >= WAIT_TIME) {	//��ʱ�ط�
			retransNum++;
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "Too long waiting time, retrans the " << retransNum << " time!" << endl;
			sendPacket(&sendFIN);
			clockStart = clock();
		}
	}
	return 0;
}
bool buildConnection(Packet synPacket) {
	state = SYN_RCVD;
	nowTime = getTime();
	cout << nowTime << " [ STATE ] " << "Enter the " << state << " state!" << endl;
	cout << nowTime << " [ CONN  ] " << "Start building connections!" << endl;
	cout << nowTime << " [ CONN  ] " << "Starting synchronization!" << endl;

	Packet sendACK, recvACK;
	setSYN(&sendACK);
	setACK(&sendACK, &synPacket);//�ظ�������ϢsynPacket��ACK
	sendACK.Seq = sendSeq++;//������ϢsendACK�ķ������.
	sendPacket(&sendACK);

	nowTime = getTime();
	cout << nowTime << " [ CONN  ] " << "Waiting . . ." << endl;
	clockStart = clock();
	int retransNum = 0;
	while (1) {
		if (recvPacket(&recvACK)) {
			if (checkACK(&recvACK) && recvACK.Ack == sendSeq) {
				state = ESTABLISHED;
				nowTime = getTime();
				cout << nowTime << " [ CONN  ] " << "The connection has been established successfully!" << endl;
				return 1;
			}
			clockEnd = clock();
			if (retransNum == RETRANSMISSION_TIMES) {
				nowTime = getTime();
				cout << nowTime << " [ TIMEO ] " << "The number of retransmission times is too many, fail to send!" << endl;
				cout << nowTime << " [ ERRO  ] " << "Connection establishment failure!" << endl;
				return 0;
			}
			if ((clockEnd - clockStart) >= WAIT_TIME) {
				retransNum++;
				nowTime = getTime();
				cout << nowTime << " [ TIMEO ] " << "Too long waiting time, retrans the " << retransNum << " time!" << endl;
				sendPacket(&sendACK);
				clockStart = clock();
			}
		}
	}
	nowTime = getTime();
	cout << nowTime << " [ ERRO  ] " << "Connection establishment failure!" << endl;
	return 0;
}


void outFile(char* fileName, int PacketNum, int fileStart){
	nowTime = getTime();
	cout << nowTime << " [ FOUT  ] " << "Start to output file: " << fileName << endl;
	ofstream fout(fileName, ofstream::binary);

	Packet* file;
	int i = fileStart;
	while (true) {
		file = (Packet*)(buf[i++]);
		for (int j = 0; j < file->length; j++)
			fout << file->Data[j];
		if (checkEnd(file)) {
			break;
		}
	}

	fout.close();
	nowTime = getTime();
	cout << nowTime << " [ FOUT  ] " << "File " << fileName << " output succeeded!" << endl;
}
int recvFile(Packet recvFileName){
	Packet sendACK;
	sendACK.Seq = sendSeq;
	setACK(&sendACK, &recvFileName);
	sendPacket(&sendACK);
	memset(&sendACK, 0, sizeof(Packet));
	//��ȡ�ļ���
	int PacketNum = recvFileName.index;
	int nameLength = recvFileName.length;
	char* fileName = new char[nameLength + 1];
	memset(fileName, 0, nameLength + 1);
	for (int i = 0; i < nameLength; i++) {
		fileName[i] = recvFileName.Data[i];
	}
	fileName[nameLength] = '\0';
	nowTime = getTime();
	cout << nowTime << " [ FRECV ] " << "Start to receive file: " << fileName << endl;

	Packet recvP;
	cout << "PacketNum: " << PacketNum << endl;

	//Seq = recvFileName.Seq + 1;
	wndStart = recvFileName.Seq + 1;
	wndEnd = wndStart + WND_SIZE;
	wndPointer = wndStart;

	clockStart = clock();
	while (1) {
		if (recvPacket(&recvP) && checkFile(&recvP) && recvP.Seq == wndPointer) {
			memcpy(buf[wndPointer++], &recvP, sizeof(Packet));
			//setACK(&sendACK, &recvP);
			//sendPacket(&sendACK);
			//for (int j = 0; j < recvP.length; j++) {
			//	recvData[recvP.index][j] = recvP.Data[j];
			//}
			//if (recvP.index == PacketNum) {
			//	lastLength = recvP.length;
			//}
			if (checkEnd(&recvP)) {
				setACK(&sendACK, &recvP);
				sendPacket(&sendACK);
				break;
			}
			if (wndPointer == wndEnd) {
				wndStart = wndEnd;
				wndEnd = wndStart + WND_SIZE;
				setACK(&sendACK, &recvP);
				sendPacket(&sendACK);
				clockStart = clock();
			}
			else {
				clockEnd = clock();
				if (clockEnd - clockStart > 300) {
					wndStart = wndPointer;
					wndEnd = wndStart + WND_SIZE;
					setACK(&sendACK, &recvP);
					sendPacket(&sendACK);
					clockStart = clock();
				}
			}

		}
		//else {
		//	clockEnd = clock();
		//	if (clockEnd - clockStart > 200) {
		//		setACK(&sendACK, &recvP);
		//		sendPacket(&sendACK);
		//		wndStart = wndPointer;
		//		wndEnd = wndStart + WND_SIZE;
		//		clockStart = clock();
		//	}
		//}

	}
	//int lastLength = 0;
	//for (int i = 0; i <= PacketNum; i++) {
	//	Packet recvFile, sendACK;
	//	memset(recvData[i], 0, BUF_SIZE);
	//	if (stopWaitRecv(&recvFile, &sendACK)) {
	//		if (recvFile.index != i) {
	//			return 0;
	//		}
	//		for (int j = 0; j < recvFile.length; j++) {
	//			recvData[i][j] = recvFile.Data[j];
	//		}
	//	}
	//	else {
	//		cout << "����0" << endl;
	//		return 0;
	//	}
	//	if (i == PacketNum) {
	//		lastLength = recvFile.length;
	//		if (!checkEnd(&recvFile)) {
	//			cout << "����1" << endl;
	//			return 0;
	//		}
	//	}
	//}
	//nowTime = getTime();
	//cout << nowTime << " [ FRECV ] " << "File "<<fileName<<" received successfully!"  << endl;
	outFile(fileName, PacketNum, recvFileName.Seq + 1);
	return 1;
}

void init()
{
	WSADATA wsaData;
	//����dll�ļ� �汾 2.2   Scoket ��   
	int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err != 0) {
		//�Ҳ��� winsock.dll 
		nowTime = getTime();
		cout << nowTime << " [ ERROR ] " << "WSAStartup failed with error:" << err << endl;
		return;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		nowTime = getTime();
		cout << nowTime << " [ ERROR ] " << "Could not find a usable version of Winsock.dll" << endl;
		WSACleanup();
		return;
	}

	sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockServer == INVALID_SOCKET) {
		nowTime = getTime();
		cout << nowTime << " [ ERROR ] " << "Socket creation failed!" << endl;
		return;
	}

	//�����׽���Ϊ������ģʽ
	int iMode = 1; //1����������0������ 
	ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);//���������� 

	//sockAddr.sin_addr.s_addr = inet_addr("10.130.78.9");  //�����IP��ַ
	sockAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(4000);

	err = bind(sockServer, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));
	if (err) {
		err = GetLastError();
		nowTime = getTime();
		cout << nowTime << " [ ERROR ] " << "Could  not  bind  the  port" << SERVER_PORT << "for  socket. Error  code is" << err << endl;
		WSACleanup();
		return;
	}
	else {
		nowTime = getTime();
		cout << nowTime << " [ INFO  ] " << "Server is created successfully!" << endl;
	}
	state = CLOSE;
	nowTime = getTime();
	cout << nowTime << " [ STATE ] " << "Enter the " << state << " state!" << endl;
}
void close() {
	closesocket(sockServer);//�ر�socket
	nowTime = getTime();
	cout << nowTime << " [ INFO  ] " << "The Socket was successfully closed!" << endl;
	WSACleanup();
	nowTime = getTime();
	cout << nowTime << " [ INFO  ] " << "The WSA was successfully cleaned up" << endl;
}

int main() {
	
	init();

	state = LISTEN;
	nowTime = getTime();
	cout << nowTime << " [ STATE ] " << "Enter the " << state << " state!" << endl;

	while (1) {
		Packet recvP;
		if (recvPacket(&recvP)) {
			if (checkSYN(&recvP)) {
				cout << "recv a SYN" << endl;
				buildConnection(recvP);
			}
			else if (checkFIN(&recvP) && state == ESTABLISHED) {
				breakConnection(recvP);
			}
			else if (checkStart(&recvP) && state == ESTABLISHED) {
				recvFile(recvP);
			}
		}
		Sleep(20);//����Ƶ�����տ���Ϣ
	}
	close();
	return 0;
}