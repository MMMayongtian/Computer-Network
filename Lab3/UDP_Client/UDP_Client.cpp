#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")  //加载 ws2_32.dll
#include<iostream>
#include<fstream>
using namespace std;

#define SERVER_PORT 3000
#define SERVER_IP "127.0.0.1"
#define BUF_SIZE 1024
#define SENT_TIMES 10
#define WAIT_TIME 1

#define CLOSE 0 
#define SYN_SENT 1
#define ESTABLISHED 2
#define FIN_WAIT_1 3
#define FIN_WAIT_2 4
#define TIME_WAIT 5
#pragma pack(1)	//按字节对齐
struct Packet	//报文格式
{
	//SYN=0x01  ACk=0x02  FIN=0x04  PACKET=0x08  StartFile=0x10  EndFile=0x20
	int Flags = 0;	//标志位
	//DWORD SendIP; //发送端IP
	//DWORD RecvIP; //接收端IP
	//u_short SendPort; //发送端端口
	//u_short RecvPort; //接收端端口
	int Seq;	//消息序号
	int Ack;	//恢复ack时确认对方的消息的序号
	int index;	//文件分片传输 第几片
	int length;	//Data段长度
	//int fill;	//补零成为16bit的倍数，便于计算校验和
	u_short checksum;//校验和
	char Data[BUF_SIZE];//报文的具体内容
};
#pragma pack()//恢复4Byte编址格式

SOCKET sockClient;
struct sockaddr_in sockAddr;
int addr_len = sizeof(struct sockaddr_in);

string path = "C:\\Users\\000\\Desktop\\测试文件\\测试文件\\";
int state = CLOSE;		//客户端状态
int sendSeq = 0;		//客户端消息序列号
char dataToSend[10000][BUF_SIZE];	//要发送的数据
int PacketNum = 0;		//需要分片数量
int data_p = 0;			//数据存储位置
clock_t clockStart;
clock_t clockEnd;

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

void setStart(Packet* t)
{
	t->Flags |= 0x10;
}
void setEnd(Packet* t)
{
	t->Flags |= 0x20;
}

void sendPacket(Packet* sendP) {
	setPacket(sendP);	//表示该条消息存在，方便非阻塞式recv判断是否接收到了消息
	setCheckSum(sendP);	//设置校验和
	if (sendto(sockClient, (char*)sendP, sizeof(Packet), 0, (struct sockaddr*)&sockAddr, sizeof(sockaddr)) == SOCKET_ERROR) {

	}
}
bool recvPacket(Packet* recvP) {
	memset(recvP->Data, 0, sizeof(recvP->Data));
	recvfrom(sockClient, (char*)recvP, sizeof(Packet), 0, (struct sockaddr*)&sockAddr, &addr_len);
	if (checkPacket(recvP)) {
		return true;
	}
	return false;
}

bool stopWaitRecv(Packet* recvP, Packet* sendP){
	while (1){
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
	while (1){
		if (recvPacket(recvP)) {
			if (checkACK(recvP) && recvP->Ack == (sendP->Seq + 1)){	//收到对sendP的确认报文
				return 1;
			}
		}
		clockEnd = clock();
		if (retransNum == SENT_TIMES) {	//到达最大重发次数
			return 0;
		}
		if ((clockEnd - clockStart) / CLOCKS_PER_SEC >= WAIT_TIME){	//超时重发
			retransNum++;
			clockStart = clock();
			cout << "重传" << retransNum << endl;
			sendPacket(sendP);
		}
	}
	return 0;//发送失败
}

bool breakConnection() //断开连接
{
	Packet sendFIN, recvACK, recvFIN, sendACK;

	sendFIN.Seq = sendSeq++;
	setFIN(&sendFIN);

	if (stopWaitSend(&sendFIN, &recvACK)) {
		state = FIN_WAIT_1;
	}
	if (stopWaitRecv(&recvFIN, &sendACK)) {
		state = FIN_WAIT_2;
	}
	_sleep(2);//MSE
	cout << "断开连接" << endl;
	return 1;//stopwait函数返回0，即没有收到对方返回的消息
}
bool buildConnection()	//建立连接
{
	Packet sendSYN, recvACK, sendACK;

	sendSYN.Seq = sendSeq++;
	setSYN(&sendSYN);

	if(stopWaitSend(&sendSYN,&recvACK)){
		if (checkSYN(&recvACK)) {
			setACK(&sendACK, &recvACK);
			sendPacket(&sendACK);
			cout << "成功建立连接" << endl;
			return 1;
		}
	}
	return 0;
}

void readFile(char* fileName)//读取文件
{
	string filename(fileName);
	ifstream in(filename, ifstream::binary);
	if (!in){
		cout << "文件无效" << endl;
		return;
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
}
int sendFile(char* fileName)	//发送文件
{
	cout << "开始发送文件" << endl;

	readFile(fileName);//读入文件
	clock_t timestart = clock();//timer

	int nameLength = strlen(fileName);
	//组装文件开始消息
	Packet sendFileName, recvACK;;
	setStart(&sendFileName);
	sendFileName.Seq = sendSeq++;
	sendFileName.index = PacketNum;
	sendFileName.length = nameLength;

	for (int i = 0; i < nameLength; i++) {
		sendFileName.Data[i] = fileName[i];
	}

	if (!stopWaitSend(&sendFileName, &recvACK)) {//发送消息(文件名)  超时重传
		cout << "文件传输失败" << endl;
		return 0;
	}

	for (int i = 0; i <= PacketNum; i++) {
		Packet sendFile;
		sendFile.Seq = sendSeq++;
		sendFile.index = i;
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
			cout << "文件发送失败" << endl;
			return 0;
		}
	}
	//吞吐率计算
	clock_t timeend = clock();
	double endtime = (double)(timeend - timestart) / CLOCKS_PER_SEC;
	cout << "Total time:" << endtime << endl;		//s为单位
	cout << "吞吐率：" << (double)(PacketNum + 1) * sizeof(Packet) * 8 / endtime / 1024 / 1024 << "Mbps" << endl;
	cout << "文件发送成功" << endl;
	return 1;//发送成功
}
//初始化工作
void init()
{
	//加载套接字库（必须） 
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
	sockClient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockClient == INVALID_SOCKET) {
		cout << "Socket creation failed!" << endl;
		return;
	}
	//设置套接字为非阻塞模式
	int iMode = 1; //1：非阻塞，0：阻塞 
	ioctlsocket(sockClient, FIONBIO, (u_long FAR*) & iMode);//非阻塞设置 

	sockAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(3000);

	cout << "Client is created successfully" << endl;
}
void close() {
	closesocket(sockClient);//关闭socket
	WSACleanup();
}

int main() {
	init();

	buildConnection();
	char fileName[20] = "1.jpg";

	sendFile(fileName);
	breakConnection();
	close();
}