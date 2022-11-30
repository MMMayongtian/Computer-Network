#include "Packet.h"
void sendPacket(Packet* sendP) {
	setPacket(sendP);//��ʾ������Ϣ���ڣ����������ʽrecv�ж��Ƿ���յ�����Ϣ
	setCheckSum(sendP);//����У���
	//sendP->Flags |= 0x40;
	if (sendto(sockServer, (char*)sendP, sizeof(Packet), 0, (struct sockaddr*)&sockAddr, sizeof(sockaddr)) != SOCKET_ERROR) {
		nowTime = getTime();
		Log = PacketLog(sendP);
		cout << nowTime << " [ SEND  ] " << Log << endl;
		//cout <<"[ SEND ]" << endl;
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
		//cout << "[ RESV ]" << endl;
		return true;
	}
	return false;
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
	nowTime = getTime();
	cout << nowTime << " [ ERROR ] " << "Disconnection failure!" << endl;
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

void outFile(char* fileName, int PacketNum, int fileStart) {
	nowTime = getTime();
	cout << nowTime << " [ FOUT  ] " << "Start to output file: " << fileName << endl;
	ofstream fout(fileName, ofstream::binary);

	Packet* file;
	int i = fileStart;
	while (true) {
		file = (Packet*)(buf[i++]);
		for (int j = 0; j < file->length; j++) {
			fout << file->Data[j];
		}
		if (checkEnd(file)) {
			break;
		}
	}

	fout.close();
	nowTime = getTime();
	cout << nowTime << " [ FOUT  ] " << "File " << fileName << " output succeeded!" << endl;
}
int recvFile(Packet recvFileName) {
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
	cout << nowTime << " [ FRECV ] " << "Start to receive file: " << fileName << " PacketNum = " << PacketNum << endl;
	Packet recvP;

	wndStart = recvFileName.Seq + 1;
	wndEnd = wndStart + WND_SIZE;
	wndPointer = wndStart;//ָ���һ���ȴ�����������ȷ�ϵ�
	bool flag = true;
	clockStart = clock();
	while (1) {
		if (recvPacket(&recvP) && checkFile(&recvP)) {
			if (recvP.Seq == wndPointer) {
				flag = true;
				memcpy(buf[wndPointer++], &recvP, sizeof(Packet));
				if (checkEnd(&recvP)) {
					wndStart = wndPointer;
					wndEnd = wndPointer;	
					setACK(&sendACK, &recvP);
					sendPacket(&sendACK);	
					nowTime = getTime();
					cout << nowTime << " [ INFO  ] " << "End receive file!" << endl;
					break;
				}
				if (wndPointer == wndEnd) {	//�ۻ���
					wndStart = wndEnd;
					wndEnd = wndStart + WND_SIZE;	//�����´���
					setACK(&sendACK, &recvP);
					clock_t a = clock();
					clock_t b = clock();
					while ((b - a) < 5) {
						b = clock();
					}
					sendPacket(&sendACK);	//ȷ�Ͼɴ���
					//nowTime = getTime();
					//cout << nowTime << " [ INFO  ] " << "Window slide!  wndStart=" << wndStart << " wndEnd=" << wndEnd << endl;
					//clockStart = clock();
				}
			}
			else if (recvP.Seq > wndPointer) { //���� ��ǰ����  Ӧ�����յ�������
				//nowTime = getTime();
				cout << nowTime << " [ INFO  ] " << "Out of order! Flag: " << flag << endl;
				if (flag) {
					flag = false;
					wndStart = wndPointer;
					wndEnd = wndStart + WND_SIZE;	//�����´���
					setACK(&sendACK, wndPointer);
					sendPacket(&sendACK);	//���·��� ��ȷ�ϻ����������һ��
					//clockStart = clock();
				}
			}
			else {
				//do nothing ��ȷ�Ϲ� ����
			}
		}
		//clockEnd = clock();
		//if (clockEnd - clockStart > 500) {
		//	wndStart = wndPointer;
		//	wndEnd = wndStart + WND_SIZE;	//�����´���
		//	setACK(&sendACK, wndPointer);
		//	sendPacket(&sendACK);	//���·��� ��ȷ�ϻ����������һ��
		//	nowTime = getTime();
		//	cout << nowTime << " [ TIMEO ] " << "Resend the ACK!" << endl;
		//	clockStart = clock();
		//}
	}
	nowTime = getTime();
	cout << nowTime << " [ FRECV ] " << "File " << fileName << " received successfully!" << endl;
	outFile(fileName, PacketNum, recvFileName.Seq + 1);
	return 1;
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
		Sleep(20);//����Ƶ�����տ���Ϣ
	}
	close();
	return 0;
}