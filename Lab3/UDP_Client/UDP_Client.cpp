#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")  //���� ws2_32.dll
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
#pragma pack(1)	//���ֽڶ���
struct Packet	//���ĸ�ʽ
{
	//SYN=0x01  ACk=0x02  FIN=0x04  PACKET=0x08  StartFile=0x10  EndFile=0x20
	int Flags = 0;	//��־λ
	//DWORD SendIP; //���Ͷ�IP
	//DWORD RecvIP; //���ն�IP
	//u_short SendPort; //���Ͷ˶˿�
	//u_short RecvPort; //���ն˶˿�
	int Seq;	//��Ϣ���
	int Ack;	//�ָ�ackʱȷ�϶Է�����Ϣ�����
	int index;	//�ļ���Ƭ���� �ڼ�Ƭ
	int length;	//Data�γ���
	//int fill;	//�����Ϊ16bit�ı��������ڼ���У���
	u_short checksum;//У���
	char Data[BUF_SIZE];//���ĵľ�������
};
#pragma pack()//�ָ�4Byte��ַ��ʽ

SOCKET sockClient;
struct sockaddr_in sockAddr;
int addr_len = sizeof(struct sockaddr_in);

string path = "C:\\Users\\000\\Desktop\\�����ļ�\\�����ļ�\\";
int state = CLOSE;		//�ͻ���״̬
int sendSeq = 0;		//�ͻ�����Ϣ���к�
char dataToSend[10000][BUF_SIZE];	//Ҫ���͵�����
int PacketNum = 0;		//��Ҫ��Ƭ����
int data_p = 0;			//���ݴ洢λ��
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

bool checkCheckSum(Packet* t)//���շ���У��ͽ��м���
{
	int sum = 0;
	u_char* packet = (u_char*)t;
	for (int i = 0; i < 16; i++)
	{
		sum += packet[2 * i] << 8 + packet[2 * i + 1];
		while (sum >= 0x10000)
		{//���
			int t = sum >> 16;//���㷽��������У�����ͬ
			sum += t;
		}
	}
	//�Ѽ��������У��ͺͱ����и��ֶε�ֵ��ӣ��������0xffff����У��ɹ�
	if (t->checksum + (u_short)sum == 65535)
		return true;
	return true;
}
void setCheckSum(Packet* t)//���ͷ�����У��Ͳ�������Ӧλ��
{
	int sum = 0;
	u_char* packet = (u_char*)t;//��1BΪ��λ���д���ע������Ϊunsigned���ͣ������ں������ʱ�ᰴ��Ĭ�����λ�������ţ�Ӱ�������
	for (int i = 0; i < 16; i++)//ȡ���ĵ�ǰ16�飬ÿ��16bit������32�ֽ�256bit
	{
		sum += packet[2 * i] << 8 + packet[2 * i + 1];
		while (sum >= 0x10000)
		{//���
			int t = sum >> 16;//�����λ�ع���������λ
			sum += t;
		}
	}
	t->checksum = ~(u_short)sum;//��λȡ��������У�����
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
	setPacket(sendP);	//��ʾ������Ϣ���ڣ����������ʽrecv�ж��Ƿ���յ�����Ϣ
	setCheckSum(sendP);	//����У���
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
		if (recvPacket(recvP)) {		//�յ��Է���������ϢrecvP
			if (checkCheckSum(recvP)) {	//У��ͼ��ɹ�
				setACK(sendP, recvP);	//�ظ������յ���ϢrecvP��ack��ϢsendP
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
	clockStart = clock();	//��ʼ��ʱ
	int retransNum = 0;		//�ط�����
	while (1){
		if (recvPacket(recvP)) {
			if (checkACK(recvP) && recvP->Ack == (sendP->Seq + 1)){	//�յ���sendP��ȷ�ϱ���
				return 1;
			}
		}
		clockEnd = clock();
		if (retransNum == SENT_TIMES) {	//��������ط�����
			return 0;
		}
		if ((clockEnd - clockStart) / CLOCKS_PER_SEC >= WAIT_TIME){	//��ʱ�ط�
			retransNum++;
			clockStart = clock();
			cout << "�ش�" << retransNum << endl;
			sendPacket(sendP);
		}
	}
	return 0;//����ʧ��
}

bool breakConnection() //�Ͽ�����
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
	cout << "�Ͽ�����" << endl;
	return 1;//stopwait��������0����û���յ��Է����ص���Ϣ
}
bool buildConnection()	//��������
{
	Packet sendSYN, recvACK, sendACK;

	sendSYN.Seq = sendSeq++;
	setSYN(&sendSYN);

	if(stopWaitSend(&sendSYN,&recvACK)){
		if (checkSYN(&recvACK)) {
			setACK(&sendACK, &recvACK);
			sendPacket(&sendACK);
			cout << "�ɹ���������" << endl;
			return 1;
		}
	}
	return 0;
}

void readFile(char* fileName)//��ȡ�ļ�
{
	string filename(fileName);
	ifstream in(filename, ifstream::binary);
	if (!in){
		cout << "�ļ���Ч" << endl;
		return;
	}

	PacketNum = 0;
	data_p = 0;
	char Char = in.get();	//�����ַ�char
	while (in) {
		dataToSend[PacketNum][data_p] = Char;	//�ļ����ݴ���dataToSend��
		data_p++;
		if (data_p % BUF_SIZE == 0) {	//����������� ��һ�����ݰ�
			PacketNum++;
			data_p = 0;
		}
		Char = in.get();
	}
	in.close();
}
int sendFile(char* fileName)	//�����ļ�
{
	cout << "��ʼ�����ļ�" << endl;

	readFile(fileName);//�����ļ�
	clock_t timestart = clock();//timer

	int nameLength = strlen(fileName);
	//��װ�ļ���ʼ��Ϣ
	Packet sendFileName, recvACK;;
	setStart(&sendFileName);
	sendFileName.Seq = sendSeq++;
	sendFileName.index = PacketNum;
	sendFileName.length = nameLength;

	for (int i = 0; i < nameLength; i++) {
		sendFileName.Data[i] = fileName[i];
	}

	if (!stopWaitSend(&sendFileName, &recvACK)) {//������Ϣ(�ļ���)  ��ʱ�ش�
		cout << "�ļ�����ʧ��" << endl;
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
			cout << "�ļ�����ʧ��" << endl;
			return 0;
		}
	}
	//�����ʼ���
	clock_t timeend = clock();
	double endtime = (double)(timeend - timestart) / CLOCKS_PER_SEC;
	cout << "Total time:" << endtime << endl;		//sΪ��λ
	cout << "�����ʣ�" << (double)(PacketNum + 1) * sizeof(Packet) * 8 / endtime / 1024 / 1024 << "Mbps" << endl;
	cout << "�ļ����ͳɹ�" << endl;
	return 1;//���ͳɹ�
}
//��ʼ������
void init()
{
	//�����׽��ֿ⣨���룩 
	WSADATA wsaData;
	//����dll�ļ� �汾 2.2   Scoket ��   
	int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err != 0) {
		//�Ҳ��� winsock.dll 
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
	//�����׽���Ϊ������ģʽ
	int iMode = 1; //1����������0������ 
	ioctlsocket(sockClient, FIONBIO, (u_long FAR*) & iMode);//���������� 

	sockAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(3000);

	cout << "Client is created successfully" << endl;
}
void close() {
	closesocket(sockClient);//�ر�socket
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