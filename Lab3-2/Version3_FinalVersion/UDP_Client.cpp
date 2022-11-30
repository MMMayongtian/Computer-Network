#include"Packet.h"
int discard = 100;
bool stop = false;
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
			if (checkACK(&recvFIN) && checkFIN(&recvFIN) && recvFIN.Ack == sendSeq) {	//收到对sendP的确认报文
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
		if (stop) {
			continue;
		}
		if (i >= bufIndex) {
			continue;
		}
		if (!retrans) {
			if(i < wndEnd) {
				if (i == discard) {
					discard = 498;
					i++;
					continue;
				}
				sendPacket((Packet*)buf[i]);
				cout << nowTime << " [ WNDP  ] " << "windowsStart=" << wndStart << " wndEnd=" << wndEnd << " Seq=" << ((Packet*)buf[i])->Seq << endl;
				i++;
			}
		}
		else {
			retrans = false;
			end = wndEnd;
			for (i = wndStart; i < end; i++) {
				sendPacket((Packet*)buf[i]);
				cout << nowTime << " [ GBNS  ] " << "r windowsStart=" << wndStart << " wndEnd=" << wndEnd << " Seq=" << ((Packet*)buf[i])->Seq << endl;
			}
		}
	}
	cout << nowTime << " [ WSEND ] " << "The buffer has been sent!" << endl;
	return;
}
void wndSlide() {
	bufStop = false;
	wndStart = sendSeq;
	wndEnd = wndStart + WND_SIZE;
	thread sendThread(sendBuf);	//启动Buf发送线程
	Packet recvP;
	int AckSize = 0;
	wndPointer = wndStart + 1;	//已确认序列号
	cout << bufIndex << endl;
	clockStart = clock();
	retrans = true;
	while (true) {
		if (recvPacket(&recvP)) {
			if (checkACK(&recvP) && recvP.Ack > wndStart) {
				if (recvP.Ack >= bufIndex) {
					bufStop = true;
					sendThread.join();
					return;
				}
				int nowSize = recvP.Ack - wndStart;
				sendSeq = recvP.Ack;
				wndStart = recvP.Ack;
				stop = true;
				wndEnd = (wndStart + WND_SIZE) >= bufIndex ? bufIndex : (wndStart + WND_SIZE);
				if (nowSize >= AckSize) {
					AckSize = nowSize;
				}
				else {
					retrans = true;
				}
				stop = false;
				clockStart = clock();
				continue;
				//cout << "recv:" << recvP.Ack << endl;
			}
		}
		clockEnd = clock();
		if (clockEnd - clockStart > 200) {
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
		cout << nowTime << " [ RINC  ] " << "Please enter the Server IP address: ";
		char IP[20] = "127.0.0.1";
		cin >> IP;
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