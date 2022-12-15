#include "Packet.h"

int discard = 100;
bool stop = false;

int ssthresh = 32;
int states = 0; //1 慢启动  2 拥塞避免  3 快速恢复


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

	bufIndex = sendSeq + 1;
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
			memcpy(buf[bufIndex], &sendBuf, sizeof(Packet));
			memset(&sendBuf, 0, sizeof(sizeof(&sendBuf)));
			bufIndex++;
			PacketNum++;
			data_p = 0;
		}
		Char = in.get();
	}
	sendBuf.index = PacketNum;
	sendBuf.length = data_p;
	sendBuf.Seq = bufIndex;
	setFile(&sendBuf);
	setEnd(&sendBuf);
	memcpy(buf[bufIndex++], &sendBuf, sizeof(Packet));
	memset(&sendBuf, 0, sizeof(sizeof(&sendBuf)));

	in.close();
	nowTime = getTime();
	cout << nowTime << " [ INFO  ] " << "File " << fileName << " input succeeded!" << endl;
	return 1;
}
void sendBuf() {
	int i = wndEnd;
	int end = wndEnd;
	while (!bufStop) {
		//_sleep(5);

		if (stop) {
			continue;
		}
		if (i >= bufIndex) {
			continue;
		}
		if (!retrans) {
			if(i < wndStart + wndSize) {
	/*			if (i == discard) {
					discard += 100;
					i++;
					continue;
				}*/
				sendPacket((Packet*)buf[i]);
				cout << nowTime << " [ WNDP  ] " <<"states:"<<states<< " windowsStart=" << wndStart << " wndSize=" << wndSize << " Seq=" << ((Packet*)buf[i])->Seq << endl;
				i++;
			}
		}
		else {
			retrans = false;
			i = wndStart;
			cout<<"reset i from wndStart"<<endl;
			//end = wndEnd;
			//for (i = wndStart; i < end; i++) {
			//	sendPacket((Packet*)buf[i]);
			//	cout << nowTime << " [ GBNS  ] " << "r windowsStart=" << wndStart << " wndEnd=" << wndEnd << " Seq=" << ((Packet*)buf[i])->Seq << endl;
			//}
		}
	}
	cout << nowTime << " [ WSEND ] " << "The buffer has been sent!" << endl;
	return;
}

void wndSlide() {
	states = 1;
	bufStop = false;
	wndStart = sendSeq;
	wndEnd = wndStart + wndSize;
	thread sendThread(sendBuf);	//启动Buf发送线程
	Packet recvP;
	int ackCount = 0;
	int ackLast = 0;
	wndPointer = wndStart + 1;	//已确认序列号
	clockStart = clock();
	retrans = true;
	while (true) {
		if (recvPacket(&recvP)&&checkACK(&recvP)) {
			if (recvP.Ack == ackLast) {
				//cout << "recv same ack:" << ackLast<<" ackCount:"<<ackCount << endl;
				ackCount++;
				if (ackCount >= 3) {
					if (states == 1 || states == 2) {
						
						stop = true;
						retrans = true;
						ssthresh = wndSize / 2;
						wndSize = ssthresh + 3;
						wndStart = ackLast;
						states = 3;
						clockStart = clock();
						//cout << "recv 3 ack same  wndSize:" <<wndSize<<" wndStart:"<<wndStart<<" ssthresh:"<<ssthresh<< endl;
						stop = false;
					}
					if (states == 3) {
					}
				}
			}
			if (recvP.Ack > wndStart) {
				ackLast = recvP.Ack;
				if (recvP.Ack >= bufIndex) {
					bufStop = true;
					sendThread.join();
					return;
				}
				stop = true;
				if (states == 1) {
					wndSize += recvP.Ack - wndStart;
					if (wndSize >= ssthresh) {
						states = 2;
					}
				}
				else if (states == 2) {
					wndSize++;
				}
				else if (states == 3) {
					wndSize += ackCount;
					ackCount = 0;
					states = 2;
				}
				sendSeq = recvP.Ack;
				wndStart = recvP.Ack; 
				stop = false;
				clockStart = clock();
				continue;
				//cout << "recv:" << recvP.Ack << endl;
			}
		}
		clockEnd = clock();
		if (clockEnd - clockStart > 2000) {
			retrans = true;
			cout << nowTime << " [ WSEND ] " << "ACK receive timeout!" << endl;
			clockStart = clock();
		}
		if (wndPointer == bufIndex + 1) {
			sendSeq = bufIndex;
			bufStop = true;
			sendThread.join();
			return;
		}
	}
}

int sendFile(char* fileName) {
	if (state != ESTABLISHED) {
		nowTime = getTime();
		cout << nowTime << " [ ERROR ] " << "Connection not established! " << fileName << endl;
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
	cout << nowTime << " [ FSEND ] " << "Send the name of file: " << fileName << endl;
	sendPacket(&sendFileName);
	clock_t start = clock();
	clock_t end;
	while (1) {
		if (recvPacket(&recvACK)) {
			if (checkACK(&recvACK) && recvACK.Ack == sendSeq) {
				break;
			}
		}
		end = clock();
		if (end - start > 1000) {
			nowTime = getTime();
			cout << nowTime << " [ TIMEO ] " << "Time out! Stop to send file: " << fileName << endl;
			return 0;
		}
	}

	nowTime = getTime();
	cout << nowTime << " [ FSEND ] " << "Start to send file: " << fileName << endl;
	clock_t RPS_s = clock();
	wndSlide();
	clock_t PRS_d = clock();
	nowTime = getTime();
	cout << nowTime << " [ FSEND ] " << "File " << fileName << " sent successfully!" << endl;
	//吞吐率计算
	double totalTime = (double)(PRS_d - RPS_s) / CLOCKS_PER_SEC;
	double RPS = (double)(PacketNum) * sizeof(Packet) * 8 / totalTime / 1024 / 1024;
	nowTime = getTime();
	cout << nowTime << " [ INFO  ] " << "Total time: " << totalTime << "s\t\tRPS: " << RPS << " Mbps" << endl;
}

int main() {

	init();

	bool Control = 0;
	nowTime = getTime();
	cout << nowTime << " [ RINC  ] " << "Connect(1) Or Exit(0) :\t";
	cin >> Control;

	while (Control) {
		nowTime = getTime();
		//cout << nowTime << " [ RINC  ] " << "Please enter the Server IP address: ";
		char IP[20] = "127.0.0.1";
		//cin >> IP;
		sockAddr.sin_addr.s_addr = inet_addr(IP);

		if (buildConnection()) {
			nowTime = getTime();
			cout << nowTime << " [ RINC  ] " << "Send File(1) Or Disconnect(0) :\t";
			cin >> Control;
			while (Control) {
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