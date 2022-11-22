#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
#include<iostream>
#include<time.h>
#include<string>
#include<fstream>
#include<thread>
using namespace std;

#define SERVER_PORT 3000
#define SERVER_IP "127.0.0.1"
#define BUF_SIZE 1024
#define RETRANSMISSION_TIMES 10
#define WAIT_TIME 200
#define MSL 200

#define CLOSE 0 
#define SYN_SENT 2
#define ESTABLISHED 4
#define FIN_WAIT_1 5
#define FIN_WAIT_2 7
#define TIME_WAIT 9
#define WND_SIZE 100

#pragma pack(1)	//按字节对齐
class Packet	//报文格式
{
public:
	//SYN=0x01  ACk=0x02  FIN=0x04  PACKET=0x08  StartFile=0x10  EndFile=0x20  File=0x40
	int Flags = 0;		//标志位
	DWORD SendIP;		//发送端IP
	DWORD RecvIP;		//接收端IP
	u_short Port;		//端口
	u_short	Protocol;	//协议类型
	int Seq = -1;
	int Ack = -1;
	int index = -1;			//文件分片传输 第几片
	int length = -1;			//Data段长度
	//int window = -1;
	u_short checksum;	//校验和
	char Data[BUF_SIZE];//报文数据段
};
#pragma pack()//恢复4Byte编址格式

SOCKET sockClient;
struct sockaddr_in sockAddr;
struct sockaddr_in sockAddrS;
int addr_len = sizeof(struct sockaddr_in);

string nowTime;
string Log;
int state = CLOSE;		//客户端状态
int sendSeq = 0;		//客户端消息序列号

char buf[10000][1058];
int bufIndex = 0;
int PacketNum = 0;		//需要分片数量
int data_p = 0;			//数据存储位置
clock_t clockStart;		//计时器
clock_t clockEnd;

int wndStart;
int wndEnd;
int wndPointer;
int Seq = 0;

string getTime() {
	time_t timep;
	time(&timep); //获取time_t类型的当前时间
	char tmp[64];
	strftime(tmp, sizeof(tmp), "%Y/%m/%d %H:%M:%S", localtime(&timep));//对日期和时间进行格式化
	return tmp;
}
//标志位设置与检查
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
	if (t->Flags & 0x04)
		return 1;
	return 0;
}
void setFIN(Packet* t)
{
	t->Flags |= 0x04;
}
bool checkPacket(Packet* t) {
	if (t->Flags & 0x08)
		return 1;
	return 0;
}
void setPacket(Packet* t) {
	t->Flags |= 0x08;
}
bool checkStart(Packet* t)
{
	if (t->Flags & 0x10)//flag的右第二位是0还是1
		return 1;
	return 0;
}
void setStart(Packet* t)
{
	t->Flags |= 0x10;
}
bool checkEnd(Packet* t)
{
	if (t->Flags & 0x20)
		return 1;
	return 0;
}
void setEnd(Packet* t)
{
	t->Flags |= 0x20;
}
bool checkFile(Packet* t)
{
	if (t->Flags & 0x40)
		return 1;
	return 0;
}
void setFile(Packet* t)
{
	t->Flags |= 0x40;
}
//校验和
bool checkCheckSum(Packet* t){
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
	//校验和与报文中该字段相加，等于0xFFFF则校验成功
	if (t->checksum + (u_short)sum == 0xFFFF) {
		return true;
	}
	return false;
}
void setCheckSum(Packet* t){
	int sum = 0;
	u_char* packet = (u_char*)t;
	for (int i = 0; i < 16; i++)//报文的前32字节256bit
	{
		sum += packet[2 * i] << 8 + packet[2 * i + 1];
		while (sum > 0xFFFF) {
			int sumh = sum >> 16;
			int suml = sum & 0xFFFF;
			sum = sumh + suml;
		}
	}
	t->checksum = ~(u_short)sum;//按位取反
}
//数据包日志
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
//收发包
void sendPacket(Packet* sendP) {
	setPacket(sendP);//表示该条消息存在，方便非阻塞式recv判断是否接收到了消息
	setCheckSum(sendP);//设置校验和
	//sendP->Flags |= 0x40;
	if (sendto(sockClient, (char*)sendP, sizeof(Packet), 0, (struct sockaddr*)&sockAddr, sizeof(sockaddr)) != SOCKET_ERROR) {
		nowTime = getTime();
		Log = PacketLog(sendP);
		cout << nowTime << " [ SEND  ] " << Log << endl;
	}
}
bool recvPacket(Packet* recvP) {
	memset(recvP, 0, sizeof(sizeof(&recvP)));
	int re = recvfrom(sockClient, (char*)recvP, sizeof(Packet), 0, (struct sockaddr*)&sockAddr, &addr_len);
	//if (checkPacket(recvP) && checkCheckSum(recvP)) {
	if (re != -1 && checkCheckSum(recvP)) {
		nowTime = getTime();
		Log = PacketLog(recvP);
		cout << nowTime << " [ RECV  ] " << Log << endl;
		return true;
	}
	return false;
}

bool breakConnection() {
	if (state == CLOSE) {
		return 1;
	}
	nowTime = getTime();
	cout << nowTime << " [ BREAK ] " << "Start disconnecting!" << endl;

	Packet sendFIN, recvFIN, sendACK;
	sendFIN.Seq = sendSeq++;
	setFIN(&sendFIN);
	sendPacket(&sendFIN);
	state = FIN_WAIT_1;
	nowTime = getTime();
	cout << nowTime << " [ STATE ] " << "Enter the " << state << " state!" << endl;

	clockStart = clock();	//开始计时
	int retransNum = 0;		//重发次数
	while (1) {
		if (recvPacket(&recvFIN)) {
			if (checkACK(&recvFIN) &&checkFIN(&recvFIN) && recvFIN.Ack == sendSeq) {	//收到对sendP的确认报文
				sendACK.Seq = sendSeq;
				setACK(&sendACK, &recvFIN);	//回复对于收到消息recvP的ack消息sendP
				sendPacket(&sendACK);
				state = TIME_WAIT;
				nowTime = getTime();
				cout << nowTime << " [ STATE ] " << "Enter the " << state << " state!" << endl;
				break;
			}
		}
		clockEnd = clock();
		if (retransNum == RETRANSMISSION_TIMES) {	//到达最大重发次数
			state = ESTABLISHED;
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "The number of retransmission times is too many, fail to send!" << endl;
			cout << nowTime << " [ ERROR ] " << "Connection disconnection failure!" << endl;
			return 0;
		}
		if ((clockEnd - clockStart) >= WAIT_TIME) {	//超时重发
			retransNum++;
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "Too long waiting time, retrans the " << retransNum << " time!" << endl;
			sendPacket(&sendFIN);
			clockStart = clock();
		}
	}
	_sleep(2 * MSL);
	state = CLOSE;
	nowTime = getTime();
	cout << nowTime << " [ BREAK ] " << "The connection is down! Enter the " << state << " state!" << endl;
	sendSeq = 0;
	return 1;
}
bool buildConnection() {
	nowTime = getTime();
	cout << nowTime << " [ CONN  ] " << "Start building connections!" << endl;
	cout << nowTime << " [ CONN  ] " << "Starting synchronization!" << endl;

	Packet sendSYN, recvACK, sendACK;

	sendSYN.Seq = sendSeq++;
	setSYN(&sendSYN);
	state = SYN_SENT;
	nowTime = getTime();
	cout << nowTime << " [ STATE ] " << "Enter the " << state << " state!" << endl;

	sendPacket(&sendSYN);
	clockStart = clock();	//开始计时
	int retransNum = 0;		//重发次数
	while (1) {
		if (recvPacket(&recvACK)) {
			if (checkACK(&recvACK) && checkSYN(&recvACK) && recvACK.Ack == sendSeq) {	//收到对sendP的确认报文
				sendACK.Seq = sendSeq;
				setACK(&sendACK, &recvACK);
				sendPacket(&sendACK);
				state = ESTABLISHED;
				nowTime = getTime();
				cout << nowTime << " [ CONN  ] " << "The connection has been established successfully!" << endl;
				return 1;
			}
		}
		clockEnd = clock();
		if (retransNum == RETRANSMISSION_TIMES) {	//到达最大重发次数
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "The number of retransmission times is too many, fail to send!" << endl;
			break;
			return 0;
		}
		if ((clockEnd - clockStart) >= WAIT_TIME) {	//超时重发
			retransNum++;
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "Too long waiting time, retrans the " << retransNum << " time!" << endl;
			sendPacket(&sendSYN);
			clockStart = clock();
		}
	}
	state = CLOSE;
	nowTime = getTime();
	cout << nowTime << " [ STATE ] " << "Enter the " << state << " state!" << endl;
	cout << nowTime << " [ ERRO  ] " << "Connection establishment failure!" << endl;
	return 0;
}

bool readFile(char* fileName) {
	nowTime = getTime();
	cout << nowTime << " [ INFO  ] " << "Start to input file: " << fileName << endl;
	string filename(fileName);
	ifstream in(filename, ifstream::binary);
	if (!in) {
		nowTime = getTime();
		cout << nowTime << " [ ERROR ] " << "File opening failed!" << endl;
		return 0;
	}

	bufIndex = sendSeq + 1;//为SF包预留
	PacketNum = 0;
	data_p = 0;

	Packet sendBuf;
	memset(&sendBuf, 0, sizeof(sizeof(&sendBuf)));
	char Char = in.get();	//读入字符char
	while (in) {
		sendBuf.Data[data_p] = Char;	//文件内容存入dataToSend中
		data_p++;
		if (data_p % BUF_SIZE == 0) {	//到达最大容量 下一个数据包
			sendBuf.index = PacketNum;
			sendBuf.length = data_p;
			sendBuf.Seq = bufIndex;
			setFile(&sendBuf);
			memcpy(buf[bufIndex], &sendBuf, 1058);
			memset(&sendBuf, 0, sizeof(sizeof(&sendBuf)));
			bufIndex++;
			PacketNum++;
			data_p = 0;
		}
		Char = in.get();
	}
	sendBuf.index = PacketNum++;
	sendBuf.length = data_p;
	sendBuf.Seq = bufIndex;
	setFile(&sendBuf);
	setEnd(&sendBuf);
	memcpy(buf[bufIndex++], &sendBuf, 1058);
	memset(&sendBuf, 0, sizeof(sizeof(&sendBuf)));

	in.close();
	nowTime = getTime();
	cout << nowTime << " [ FILE  ] " << "File " << fileName << " input succeeded!" << endl;
	return 1;
}
void sendBuf() {

	Packet recvP;
	bool trans = true;

	wndStart = sendSeq;
	wndEnd = wndStart + WND_SIZE;
	wndPointer = wndStart + 1;

	while (true) {
		if (wndStart >= bufIndex) {
			return;
		}
		if(trans = true){
			trans = false;
			clockStart = clock();
			for (int i = wndStart; i < wndEnd; i++) {
				sendPacket((Packet*)buf[i]);
				//cout << "wndEnd: " <<wndEnd << " bufIndex: " << bufIndex << " Seq: " << ((Packet*)buf[i])->Seq << " index: " << ((Packet*)buf[i])->index << " i: " << i << endl;
			}
		}
		if (recvPacket(&recvP) && checkACK(&recvP)) {
			wndStart = recvP.Ack;
			sendSeq = recvP.Ack;
			wndEnd = (wndStart + WND_SIZE) > bufIndex ? bufIndex : (wndStart + WND_SIZE);
			trans = true;
			continue;
		}
		clockEnd = clock();
		if (clockEnd - clockStart > 500) {
			trans = true;
		}
	}
}
int sendFile(char* fileName) {
	cout << "start to send File" << endl;
	if (state != ESTABLISHED) {
		return 0;
	}
	if (!readFile(fileName)) {
		return 0;
	}
	int nameLength = strlen(fileName);
	Packet sendFileName, recvACK;;
	setStart(&sendFileName);
	sendFileName.Seq = sendSeq++;
	sendFileName.index = PacketNum;
	sendFileName.length = nameLength;

	for (int i = 0; i < nameLength; i++) {
		sendFileName.Data[i] = fileName[i];
	}

	nowTime = getTime();
	cout << nowTime << " [ FSEND ] " << " Send the name of file: " << fileName << endl;
	sendPacket(&sendFileName);

	while (1) {
		if (recvPacket(&recvACK)) {
			if (checkACK(&recvACK) && recvACK.Ack == sendSeq) {
				break;
			}
		}
	}
	nowTime = getTime();
	cout << nowTime << " [ FSEND ] " << "Start to send file: " << fileName << endl;
	clock_t RPS_s = clock();
	sendBuf();
	clock_t PRS_d = clock();
	nowTime = getTime();
	cout << nowTime << " [ FSEND ] " << "File " << fileName << " sent successfully!" << endl;
	//吞吐率计算
	double totalTime = (double)(PRS_d - RPS_s) / CLOCKS_PER_SEC;
	double RPS = (double)(PacketNum) * sizeof(Packet) * 8 / totalTime / 1024 / 1024;
	nowTime = getTime();
	cout << nowTime << " [ INFO  ] " << "Total time: " << totalTime << "s\t\tRPS: " << RPS << " Mbps" << endl;
}

void init()
{
	//加载套接字库
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

	sockClient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockClient == INVALID_SOCKET) {
		nowTime = getTime();
		cout << nowTime << " [ ERROR ] " << "Socket creation failed!" << endl;
		return;
	}
	//设置套接字为非阻塞模式
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(sockClient, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置 

	sockAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(4000);

	nowTime = getTime();
	cout << nowTime << " [ INFO  ] " << "Client is created successfully" << endl;
}
void close() {
	closesocket(sockClient);//关闭socket
	nowTime = getTime();
	cout << nowTime << " [ INFO  ] " << "The Socket was successfully closed!" << endl;
	WSACleanup();
	nowTime = getTime();
	cout << nowTime << " [ INFO  ] " << "The WSA was successfully cleaned up" << endl;
}

int main() {

	init();

	bool Control = 0;
	nowTime = getTime();
	cout << nowTime << " [ RINC  ] " << "Connect(1) Or Exit(0) :\t";
	cin >> Control;

	while (Control) {
		//nowTime = getTime();
		//cout << nowTime << " [ RINC  ] " << "Please enter the Server IP address: ";
		//char IP[20] = "127.0.0.1";
		//cin >> IP;
		//sockAddr.sin_addr.s_addr = inet_addr(IP);

		if (buildConnection()) {

			nowTime = getTime();
			cout << nowTime << " [ RINC  ] " << "Send File(1) Or Disconnect(0) :\t";
			cin >> Control;
			while(Control){
				nowTime = getTime();
				cout << nowTime << " [ RINC  ] " << "Please enter the name of file to send: ";
				char fileName[20] = "1.jpg";
				cin >> fileName;
				sendFile(fileName);

				nowTime = getTime();
				cout << nowTime << " [ RINC  ] " << "Send File(1) Or Disconnect(0) :\t";
				cin >> Control;
			}
			breakConnection();
		}
		nowTime = getTime();
		cout << nowTime << " [ RINC  ] " << "Connect(1) Or Exit(0) :\t";
		cin >> Control;
	}

	close();
} 