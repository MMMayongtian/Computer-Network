#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
#include<iostream>
#include<time.h>
#include<string>
#include<fstream>
using namespace std;

#define SERVER_PORT 3000
#define SERVER_IP "127.0.0.1"
#define BUF_SIZE 1024
#define RETRANSMISSION_TIMES 10
#define WAIT_TIME 100
#define MSL 200

#define CLOSE 0 
#define SYN_SENT 2
#define ESTABLISHED 4
#define FIN_WAIT_1 5
#define FIN_WAIT_2 7
#define TIME_WAIT 9
#pragma pack(1)	//按字节对齐
struct Packet	//报文格式
{
	//SYN=0x01  ACk=0x02  FIN=0x04  PACKET=0x08  StartFile=0x10  EndFile=0x20  File=0x40
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

SOCKET sockClient;
struct sockaddr_in sockAddr;
struct sockaddr_in sockAddrS;
int addr_len = sizeof(struct sockaddr_in);

string path = "C:\\Users\\000\\Desktop\\测试文件\\测试文件\\";
string nowTime;
string Log;
int state = CLOSE;		//客户端状态
int sendSeq = 0;		//客户端消息序列号
char dataToSend[10000][BUF_SIZE];	//要发送的数据
int PacketNum = 0;		//需要分片数量
int data_p = 0;			//数据存储位置
clock_t clockStart;		//计时器
clock_t clockEnd;

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
	setPacket(sendP);
	setCheckSum(sendP);
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
	if (re != -1 && checkCheckSum(recvP)) {
		nowTime = getTime();
		Log = PacketLog(recvP);
		cout << nowTime << " [ RECV  ] " << Log << endl;
		return true;
	}
	return false;
}

bool stopWaitRecv(Packet* recvP, Packet* sendP){
	while (1){
		if (recvPacket(recvP)) {		//收到对方发来的消息recvP
			sendP->Seq = sendSeq++;
			setACK(sendP, recvP);	//回复对于收到消息recvP的ack消息sendP
			sendPacket(sendP);
			memset((char*)sendP, 0, sizeof(Packet));
			return 1;
		}
	}
	return 0;
}
bool stopWaitSend(Packet* sendP, Packet* recvP) {
	sendPacket(sendP);
	if (checkFIN(sendP) && state != FIN_WAIT_1) {
		state = FIN_WAIT_1;
		nowTime = getTime();
		cout << nowTime << " [ STATE ] " << "Enter the " << state << " state!" << endl;
	}

	clockStart = clock();	//开始计时
	int retransNum = 0;		//重发次数
	while (1){
		if (recvPacket(recvP)) {
			if (checkACK(recvP) && recvP->Ack == (sendP->Seq + 1)){	//收到对sendP的确认报文
				return 1;
			}
		}
		clockEnd = clock();
		if (retransNum == RETRANSMISSION_TIMES) {	//到达最大重发次数
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "The number of retransmission times is too many, fail to send!" << endl;
			return 0;
		}
		if ((clockEnd - clockStart)>= WAIT_TIME){	//超时重发
			retransNum++;
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "Too long waiting time, retrans the "<< retransNum <<" time!" << endl;
			sendPacket(sendP);
			clockStart = clock();
		}
	}
	return 0;//发送失败
}

bool breakConnection() {
	if (state == CLOSE) {
		return 1;
	}

	nowTime = getTime();
	cout << nowTime << " [ BREAK ] " << "Start disconnecting!" << endl;

	Packet sendFIN, recvACK, recvFIN, sendACK;

	sendFIN.Seq = sendSeq++;
	setFIN(&sendFIN);

	if (stopWaitSend(&sendFIN, &recvACK)) {
		state = FIN_WAIT_2;
		nowTime = getTime();
		cout << nowTime << " [ STATE ] " << "Enter the " << state << " state!" << endl;
	}
	else {
		state = ESTABLISHED;
		nowTime = getTime();
		cout << nowTime << " [ ERROR ] " << "Connection disconnection failure!" << endl;
		return 0;
	}
	if (stopWaitRecv(&recvFIN, &sendACK)) {
		state = TIME_WAIT;
		nowTime = getTime();
		cout << nowTime << " [ STATE ] " << "Enter the " << state << " state!" << endl;
	}
	else {
		state = ESTABLISHED;
		nowTime = getTime();
		cout << nowTime << " [ STATE ] " << "Connection disconnection failure!" << endl;
		return 0;
	}
	_sleep(2*MSL);
	state = CLOSE;
	nowTime = getTime();
	cout << nowTime << " [ BREAK ] " << "The connection is down! Enter the " << state << " state!" << endl;
	sendSeq = 0;
	return 1;
}
bool buildConnection() {
	//if (state == ESTABLISHED) {
	//	return 1;
	//}

	nowTime = getTime();
	cout << nowTime << " [ CONN  ] " << "Start building connections!" << endl;
	cout << nowTime << " [ CONN  ] " << "Starting synchronization!" << endl;

	Packet sendSYN, recvACK, sendACK;

	sendSYN.Seq = sendSeq++;
	setSYN(&sendSYN);
	state = SYN_SENT;
	nowTime = getTime();
	cout << nowTime << " [ STATE ] " << "Enter the " << state << " state!" << endl;

	if(stopWaitSend(&sendSYN,&recvACK)){
		if (checkSYN(&recvACK)) {
			sendACK.Seq = sendSeq++;
			setACK(&sendACK, &recvACK);
			sendPacket(&sendACK);
			state = ESTABLISHED;
			nowTime = getTime();
			cout << nowTime << " [ CONN  ] " << "The connection has been established successfully!" << endl;
			return 1;
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
	cout << nowTime << " [ FIN   ] " << "Start to input file: " << fileName << endl;

	string filename(fileName);
	ifstream in(filename, ifstream::binary);
	if (!in){
		nowTime = getTime();
		cout << nowTime << " [ ERROR ] " << "File opening failed!" << endl;
		return 0;
	}

	PacketNum = 0;
	data_p = 0;
	char Char = in.get();	//读入字符char
	while (in) {
		dataToSend[PacketNum][data_p] = Char;	//文件内容存入dataToSend中
		data_p++;
		if (data_p % BUF_SIZE == 0) {	//到达最大容量 下一个数据包
			PacketNum++;
			data_p = 0;
		}
		Char = in.get();
	}
	in.close();
	nowTime = getTime();
	cout << nowTime << " [ FIN   ] " << "File " << fileName << " input succeeded!" << endl;
	return 1;
}
int sendFile(char* fileName) {
	if (state != ESTABLISHED) {
		return 0;
	}
	if (!readFile(fileName)) {
		return 0;
	}
	clock_t timeStart = clock();//timer

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
	cout << nowTime << " [ FSEND ] " << "Start to send file: " << fileName << endl;

	if (!stopWaitSend(&sendFileName, &recvACK)) {
		nowTime = getTime();
		cout << nowTime << " [ ERROR ] " << "File send failure!" << endl;
		return 0;
	}

	for (int i = 0; i <= PacketNum; i++) {
		Packet sendFile;
		sendFile.Seq = sendSeq++;
		sendFile.index = i;
		setFile(&sendFile);
		if (i == PacketNum) {
			setEnd(&sendFile);
			sendFile.length = data_p;
			for (int j = 0; j < data_p; j++) {
				sendFile.Data[j] = dataToSend[i][j];
			}
		}
		else {
			sendFile.length = BUF_SIZE;
			for (int j = 0; j < BUF_SIZE; j++) {
				sendFile.Data[j] = dataToSend[i][j];
			}
		}
		if (!stopWaitSend(&sendFile, &recvACK)) {
			nowTime = getTime();
			cout << nowTime << " [ ERROR ] " << "File send failure!" << endl;
			return 0;
		}
	}
	nowTime = getTime();
	cout << nowTime << " [ FSEND ] " << "File " << fileName << " sent successfully!" << endl;
	//吞吐率计算
	clock_t timeEnd = clock();
	double totalTime = (double)(timeEnd - timeStart) / CLOCKS_PER_SEC;
	double RPS = (double)(PacketNum + 2) * sizeof(Packet) * 8 / totalTime / 1024 / 1024;
	nowTime = getTime();
	cout << nowTime << " [ INFO  ] " << "Total time: " << totalTime << "s\t\tRPS: " << RPS << " Mbps" << endl;;
	return 1;
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
		nowTime = getTime();
		cout << nowTime << " [ RINC  ] " << "Please enter the Server IP address: ";
		char IP[20] = "127.0.0.1";
		cin >> IP;
		sockAddr.sin_addr.s_addr = inet_addr(IP);

		if (buildConnection()) {

			nowTime = getTime();
			cout << nowTime << " [ RINC  ] " << "Send File(1) Or Disconnect(0) :\t";
			cin >> Control;
			while(Control){
				nowTime = getTime();
				cout << nowTime << " [ RINC  ] " << "Please enter the name of file to send: ";
				char fileName[20] = "helloworld.txt";
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