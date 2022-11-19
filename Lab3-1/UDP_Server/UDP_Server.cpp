#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
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
#pragma pack(1)//按字节对齐
struct Packet//报文格式
{
	//SYN=0x01  ACK=0x02  FIN=0x04  PACKET=0x08  StartFile=0x10  EndFile=0x20 File=0x40
	int Flags = 0;		//标志位
	DWORD SendIP;		//发送端IP
	DWORD RecvIP;		//接收端IP
	u_short Port;		//端口
	u_short	Protocol;	//协议类型
	int Seq;	
	int Ack;	
	int index;			//文件分片传输 第几片
	int length;			//Data段长度
	u_short checksum;	//校验和
	char Data[BUF_SIZE];//报文数据段
};
#pragma pack()//恢复4Byte编址格式

SOCKET sockServer;
struct sockaddr_in sockAddr;
int addr_len = sizeof(struct sockaddr_in);

string nowTime;
string Log;
string path = "C:\\Users\\000\\Desktop\\测试文件\\接收文件\\";
int state = CLOSE;		//状态
int sendSeq = 0;		//消息序列号
char recvData[10000][BUF_SIZE];	//接收到的数据
clock_t clockStart;		//计时器
clock_t clockEnd;

string getTime() {
	time_t timep;
	time(&timep); //获取time_t类型的当前时间
	char tmp[64];
	strftime(tmp, sizeof(tmp), "%Y/%m/%d %H:%M:%S", localtime(&timep));//对日期和时间进行格式化
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
	if (t->Flags & 0x04)//flag的右第二位是0还是1
		return 1;
	return 0;
}
void setFIN(Packet* t)
{
	t->Flags |= 0x04;
}
bool checkPacket(Packet* t) {
	if (t->Flags & 0x08)//flag的右第二位是0还是1
		return 1;
	return 0;
}
void setPacket(Packet* t) {
	t->Flags |= 0x08;
}

bool checkCheckSum(Packet* t){
	int sum = 0;
	u_char* packet = (u_char*)t;
	for (int i = 0; i < 16; i++){
		sum += packet[2 * i] << 8 + packet[2 * i + 1];
		while (sum > 0xFFFF){
			int sumh = sum >> 16;
			int suml = sum & 0xFFFF;
			sum = sumh + suml;
		}
	}
	//校验和与报文中该字段相加，等于0xFFFF则校验成功
	if (t->checksum + (u_short)sum == 0xFFFF) {
		return true;
	}
	return false;
}
void setCheckSum(Packet* t){
	int sum = 0;
	u_char* packet = (u_char*)t;
	for (int i = 0; i < 16; i++)//取报文的前16组，每组16bit，共计32字节256bit
	{
		sum += packet[2 * i] << 8 + packet[2 * i + 1];
		while (sum > 0xFFFF){
			int sumh = sum >> 16;
			int suml = sum & 0xFFFF;
			sum = sumh + suml;
		}
	}
	t->checksum = ~(u_short)sum;//按位取反
}

bool checkStart(Packet* t)
{
	if (t->Flags & 0x10)//flag的右第二位是0还是1
		return 1;
	return 0;
}
bool checkEnd(Packet* t)
{
	if (t->Flags & 0x20)//flag的右第二位是0还是1
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
	if (t->Flags & 0x40)//flag的右第二位是0还是1
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

void sendPacket(Packet* sendP) {
	setPacket(sendP);//表示该条消息存在，方便非阻塞式recv判断是否接收到了消息
	setCheckSum(sendP);//设置校验和
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
		if (recvPacket(recvP)) {		//收到对方发来的消息recvP
			if (checkCheckSum(recvP)) {	//校验和检测成功
				setACK(sendP, recvP);	//回复对于收到消息recvP的ack消息sendP
				sendP->Seq = sendSeq++;
				sendPacket(sendP);
				memset((char*)sendP, 0, sizeof(Packet));
				return 1;
			}
			cout << "校验失败" << endl;
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

	clockStart = clock();	//开始计时
	int retransNum = 0;		//重发次数
	while (1) {
		if (recvPacket(recvP)) {
			if (checkACK(recvP) && recvP->Ack == (sendP->Seq + 1)) {	//收到对sendP的确认报文
				return 1;
			}
		}
		clockEnd = clock();
		if (retransNum == RETRANSMISSION_TIMES) {	//到达最大重发次数
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "The number of retransmission times is too many, fail to send!" << endl;
			return 0;
		}
		if ((clockEnd - clockStart) >= WAIT_TIME) {	//超时重发
			retransNum++;
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "Too long waiting time, retrans the " << retransNum << " time!" << endl;
			sendPacket(sendP);
			clockStart = clock();
		}
	}
	return 0;//发送失败
}

bool breakConnection(Packet finPacket) {
	if (state == CLOSE) {
		return 1;
	}
	state = CLOSE_WAIT;
	nowTime = getTime();
	cout << nowTime << " [ STATE ] " << "Start disconnecting, Enter the " << state << " state!" << endl;

	Packet sendACK, sendFIN, recvACK;
	sendACK.Seq = sendSeq++;
	setACK(&sendACK, &finPacket);
	sendPacket(&sendACK);

	setFIN(&sendFIN);//FIN=1
	sendFIN.Seq = sendSeq++;
	if (stopWaitSend(&sendFIN, &recvACK)) {
		state = CLOSE;
		nowTime = getTime();
		cout << nowTime << " [ BREAK ] " << "The connection is down! Enter the " << state << " state!" << endl;
		sendSeq = 0;
		return 1;
	}
	return 0;
}
bool buildConnection(Packet synPacket) {
	//if (state == ESTABLISHED) {
	//	return 1;
	//}
	state = SYN_RCVD;
	nowTime = getTime();
	cout << nowTime << " [ STATE ] " << "Enter the " << state << " state!" << endl;
	cout << nowTime << " [ CONN  ] " << "Start building connections!" << endl;
	cout << nowTime << " [ CONN  ] " << "Starting synchronization!" << endl;

	Packet sendACK, recvACK;
	setSYN(&sendACK);
	setACK(&sendACK, &synPacket);//回复对于消息synPacket的ACK
	sendACK.Seq = sendSeq++;//设置消息sendACK的发送序号.
	sendPacket(&sendACK);

	nowTime = getTime();
	cout << nowTime << " [ CONN  ] " << "Waiting . . ." << endl;
	int flag = 0;
	while (1) {
		if (recvPacket(&recvACK)) {
			if (recvACK.Ack == sendSeq) {
				state = ESTABLISHED;
				nowTime = getTime();
				cout << nowTime << " [ CONN  ] " << "The connection has been established successfully!" << endl;
				return 1;
			}
		}
	}
	nowTime = getTime();
	cout << nowTime << " [ ERRO  ] " << "Connection establishment failure!" << endl;
	return 0;
}

void outFile(char* fileName, int PacketNum, int length, int lastLength){
	nowTime = getTime();
	cout << nowTime << " [ FOUT  ] " << "Start to output file: " << fileName << endl;
	ofstream fout(fileName, ofstream::binary);
	for (int i = 0; i < PacketNum; i++)
	{
		for (int j = 0; j < length; j++)
			fout << recvData[i][j];
	}
	for (int j = 0; j < lastLength; j++) {
		fout << recvData[PacketNum][j];
	}
	fout.close();
	nowTime = getTime();
	cout << nowTime << " [ FOUT  ] " << "File " << fileName << " output succeeded!" << endl;
}
int recvFile(Packet recvFileName){
	Packet sendACK;
	sendACK.Seq = sendSeq++;
	setACK(&sendACK, &recvFileName);
	sendPacket(&sendACK);

	//获取文件名
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

	int lastLength = 0;
	for (int i = 0; i <= PacketNum; i++) {
		Packet recvFile, sendACK;
		memset(recvData[i], 0, BUF_SIZE);
		if (stopWaitRecv(&recvFile, &sendACK)) {
			if (recvFile.index != i) {
				return 0;
			}
			for (int j = 0; j < recvFile.length; j++) {
				recvData[i][j] = recvFile.Data[j];
			}
		}
		else {
			cout << "出错0" << endl;
			return 0;
		}
		if (i == PacketNum) {
			lastLength = recvFile.length;
			if (!checkEnd(&recvFile)) {
				cout << "出错1" << endl;
				return 0;
			}
		}
	}
	nowTime = getTime();
	cout << nowTime << " [ FRECV ] " << "File "<<fileName<<" received successfully!"  << endl;
	outFile(fileName, PacketNum, BUF_SIZE, lastLength);
	return 1;
}

void init()
{
	WSADATA wsaData;
	//加载dll文件 版本 2.2   Scoket 库   
	int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err != 0) {
		//找不到 winsock.dll 
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

	//设置套接字为非阻塞模式
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置 

	//sockAddr.sin_addr.s_addr = inet_addr("10.130.78.9");  //具体的IP地址
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
	closesocket(sockServer);//关闭socket
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
				buildConnection(recvP);
			}
			else if (checkFIN(&recvP) && state == ESTABLISHED) {
				breakConnection(recvP);
			}
			else if (checkStart(&recvP) && state == ESTABLISHED) {
				recvFile(recvP);
			}
		}
		Sleep(20);//避免频繁接收空消息
	}
	close();
	return 0;
}