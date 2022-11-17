#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
#include<iostream>
#include<iomanip>
#include<ostream>
#include<fstream>
using namespace std;
#define SERVER_PORT 3000
#define BUF_SIZE 1024
#define SENT_TIMES 10
#define WAIT_TIME 5

#define CLOSE 0 
#define LISTEN 1
#define SYN_RCVD 2 
#define ESTABLISHED 3 
#define CLOSE_WAIT 4
#define LAST_ACK 5
#pragma pack(1)//按字节对齐
struct Packet//报文格式
{
	//SYN=0x01  ACk=0x02  FIN=0x04  PACKET=0x08  StartFile=0x10  EndFile=0x20
	int Flags = 0;//标志位
	//DWORD SendIP; //发送端IP
	//DWORD RecvIP; //接收端IP
	//u_short SendPort; //发送端端口
	//u_short	RecvPort; //接收端端口
	int Seq;	//消息序号
	int Ack;	//恢复ack时确认对方的消息的序号
	int index;	//文件分片传输 第几片
	int length;	//Data段长度
	//int fill;//补零成为16bit的倍数，便于计算校验和
	u_short checksum;//校验和
	char Data[BUF_SIZE];//报文的具体内容，本次实验简化为固定长度
};
#pragma pack()//恢复4Byte编址格式

SOCKET sockServer;
struct sockaddr_in sockAddr;
int addr_len = sizeof(struct sockaddr_in);

string nowTime;
string path = "C:\\Users\\000\\Desktop\\测试文件\\接收文件\\";
int state = CLOSE;	//状态
int sendSeq = 0;	//消息序列号
char recvData[10000][BUF_SIZE];	//接收到的数据
clock_t clockStart;
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

bool checkCheckSum(Packet* t)//接收方对校验和进行检验
{
	int sum = 0;
	u_char* packet = (u_char*)t;
	for (int i = 0; i < 16; i++)
	{
		sum += packet[2 * i] << 8 + packet[2 * i + 1];
		while (sum >= 0x10000)
		{//溢出
			int t = sum >> 16;//计算方法与设置校验和相同
			sum += t;
		}
	}
	//把计算出来的校验和和报文中该字段的值相加，如果等于0xffff，则校验成功
	if (t->checksum + (u_short)sum == 65535)
		return true;
	return true;
}
void setCheckSum(Packet* t)//发送方计算校验和并填入响应位置
{
	int sum = 0;
	u_char* packet = (u_char*)t;//以1B为单位进行处理，注意设置为unsigned类型，否则在后续相加时会按照默认最高位是正负号，影响计算结果
	for (int i = 0; i < 16; i++)//取报文的前16组，每组16bit，共计32字节256bit
	{
		sum += packet[2 * i] << 8 + packet[2 * i + 1];
		while (sum >= 0x10000)
		{//溢出
			int t = sum >> 16;//将最高位回滚添加至最低位
			sum += t;
		}
	}
	t->checksum = ~(u_short)sum;//按位取反，方便校验计算
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

void sendPacket(Packet* sendP) {
	setPacket(sendP);//表示该条消息存在，方便非阻塞式recv判断是否接收到了消息
	setCheckSum(sendP);//设置校验和
	if (sendto(sockServer, (char*)sendP, sizeof(Packet), 0, (struct sockaddr*)&sockAddr, sizeof(sockaddr)) == SOCKET_ERROR)
	{
	}
}
bool recvPacket(Packet* recvP) {
	memset(recvP->Data, 0, sizeof(recvP->Data));
	recvfrom(sockServer, (char*)recvP, sizeof(Packet), 0, (struct sockaddr*)&sockAddr, &addr_len);
	if (checkPacket(recvP)) {
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
		}
	}
	return 0;
}
bool stopWaitSend(Packet* sendP, Packet* recvP)
{
	sendPacket(sendP);
	clockStart = clock();	//开始计时
	int retransNum = 0;		//重发次数
	while (1) {
		if (recvPacket(recvP)) {
			if (checkACK(recvP) && recvP->Ack == (sendP->Seq + 1)) {	//收到对sendP的确认报文
				return 1;
			}
		}
		clockEnd = clock();
		if (retransNum == SENT_TIMES) {	//到达最大重发次数
			return 0;
		}
		if ((clockEnd - clockStart) / CLOCKS_PER_SEC >= WAIT_TIME) {	//超时重发
			retransNum++;
			clockStart = clock();
			cout << "重传" << retransNum << endl;
			sendPacket(sendP);
		}
	}
	return 0;//发送失败
}

bool breakConnection(Packet finPacket) //断开连接
{
	state = CLOSE_WAIT;
	Packet sendACK, sendFIN, recvACK;
	setACK(&sendACK, &finPacket);
	sendPacket(&sendACK);

	setFIN(&sendFIN);//FIN=1
	sendFIN.Seq = sendSeq++;
	if (stopWaitSend(&sendFIN, &recvACK)) {
		state = CLOSE;
	}

	nowTime = getTime();
	cout << nowTime << " [ BREAK ] " << "The connection is down!" << endl;
	return 0;
}
bool buildConnection(Packet synPacket)//服务端建立连接
{
	nowTime = getTime();
	cout << nowTime << " [ CONN  ] " << "Receiving a SYN Packet!" << endl;
	nowTime = getTime();
	cout << nowTime << " [ CONN  ] " << "Start building connections!" << endl;
	//Packet synPacket为收到的带有syn标志位的消息
	Packet sendACK, recvACK;
	setSYN(&sendACK);
	setACK(&sendACK, &synPacket);//回复对于消息synPacket的ACK
	sendACK.Seq = sendSeq++;//设置消息sendACK的发送序号.

	sendPacket(&sendACK);
	nowTime = getTime();
	cout << nowTime << " [ CONN  ] " << "An ACK + SYN reply packet is sent!" << endl;
	cout << nowTime << " [ CONN  ] " << "Waiting . . ." << endl;

	int flag = 0;
	while (1)
	{
		if (recvPacket(&recvACK))//接收数据
		{
			if (checkCheckSum(&recvACK))//校验和检测成功
			{
				if (recvACK.Ack == sendSeq) {
					nowTime = getTime();
					cout << nowTime << " [ CONN  ] " << "The connection has been established!" << endl;
					return 1;
				}
			}
		}
	}
	nowTime = getTime();
	cout << nowTime << " [ ERRO  ] " << "Connection establishment failure!" << endl;
	//server端接收到syn消息就代表对于server端连接已经建立
}

void outfile(char* fileName, int PacketNum, int length, int lastLength)
{
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
}

int recvfile(Packet recvFileName)
{
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

	int lastLength = 0;
	for (int i = 0; i <= PacketNum; i++) {
		Packet recvFile, sendACK;
		memset(recvData[i], 0, BUF_SIZE);
		if (stopWaitRecv(&recvFile, &sendACK)) {
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
	outfile(fileName, PacketNum, BUF_SIZE, lastLength);
	cout << "File: " << fileName << " received successfully!" << endl;
	return 1;
}


//初始化工作
void init()
{
	WSADATA wsaData;
	//加载dll文件 版本 2.2   Scoket 库   
	int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err != 0) {
		//找不到 winsock.dll 
		cout << "WSAStartup failed with error:" << err << endl;
		return;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		cout << "Could not find a usable version of Winsock.dll" << endl;
		WSACleanup();
		return;
	}

	sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockServer == INVALID_SOCKET) {
		cout << "Socket creation failed!" << endl;
		return;
	}

	//设置套接字为非阻塞模式
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(sockServer, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置 

	sockAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(3000);
	err = bind(sockServer, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));
	if (err) {
		err = GetLastError();
		cout << "Could  not  bind  the  port" << SERVER_PORT << "for  socket. Error  code is" << err << endl;
		WSACleanup();
		return;
	}
	else {
		nowTime = getTime();
		cout << nowTime << " [ INFO  ] " << "Server is created successfully!" << endl;
	}
}
void close() {
	closesocket(sockServer);//关闭socket
	WSACleanup();
}


int main() {
	init();
	while (1)
	{
		Packet recvP;
		recvPacket(&recvP);
		if (checkPacket(&recvP))
		{
			if (checkSYN(&recvP)) {
				//cout << "收到" << recvP.Flags << endl;
				buildConnection(recvP);
			}
			else if (checkFIN(&recvP)) {
				breakConnection(recvP);
			}
			else if (checkStart(&recvP)) {
				recvfile(recvP);
			}
		}
		Sleep(20);//防止频繁接收空消息占用大量CPU资源
	}
	close();
	return 0;
}