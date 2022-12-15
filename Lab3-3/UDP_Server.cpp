#include "Packet.h"
int wndSize = 0;
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
				clockStart = clock();
				flag = true;
				memcpy(buf[wndPointer++], &recvP, sizeof(Packet));
				wndSize = recvP.window;
				if (checkEnd(&recvP)) {
					wndStart = wndPointer;
					wndEnd = wndPointer;	
					setACK(&sendACK, &recvP);
					sendPacket(&sendACK);	
					nowTime = getTime();
					cout << nowTime << " [ INFO  ] " << "End receive file!" << endl;
					break;
				}
				if (wndPointer == wndStart + wndSize) {	//�ۻ���
					wndStart += wndSize;
					setACK(&sendACK, &recvP);
					//_sleep(5);
					sendPacket(&sendACK);	//ȷ�Ͼɴ���
				}
			}
			else if (recvP.Seq > wndPointer) { //���� ��ǰ����  Ӧ�����յ�������
				//nowTime = getTime();
				//cout << nowTime << " [ INFO  ] " << "Out of order! Flag: " << flag << endl;
				//if (flag) {
				//	flag = false;
					wndStart = wndPointer;
					setACK(&sendACK, wndPointer);
					sendPacket(&sendACK);	//���·��� ��ȷ�ϻ����������һ��
					clockStart = clock();
				//}
			}
			else {
				//do nothing ��ȷ�Ϲ� ����
			}
		}
		clockEnd = clock();
		if (clockEnd - clockStart > 500) {
			wndStart = wndPointer;
			wndEnd = wndStart + WND_SIZE;	//�����´���
			setACK(&sendACK, wndPointer);
			sendPacket(&sendACK);	//���·��� ��ȷ�ϻ����������һ��
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "Resend the ACK!" << endl;
			clockStart = clock();
		}
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