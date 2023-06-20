#include<iostream>
#include<stdio.h>
#include<conio.h>
#include<time.h>
#include<winsock2.h>
#pragma comment(lib,"Wsock32.lib")
#pragma warning(disable:4996)
using namespace std;
enum package_type
{
	package_ack,
	package_req_1,
	package_req_2,
	package_req_3,
	package_data
};
enum  ack_type//���ֽ��յĳ̶�
{
	FILE_FIRST_ACK,
	FILE_MIDDLE_ACK,
	FILE_LAST_ACK
};
enum congestion_stage
{
	slow_start,
	congestion_avoid,
	fast_recovery
};
/*  ���ɼ���ͣ�
	s������������
	length��s�Ĵ�С
	����ֵΪunsigned short�͵�checksum  */
unsigned short generate_checksum(const char* s, const int length) {
	unsigned int sum = 0;
	unsigned short temp1 = 0;//����������޷��������������Ϊ������չ���õ�һЩ����ֵֹ���
	unsigned short temp2 = 0;
	for (int i = 0; i < length; i += 2) {
		temp1 = s[i + 1] << 8;
		temp2 = s[i] & 0x00ff;//һ��Ҫ�Ѹ�8λ���㣡���������8λ���λ��1����ô��8λҲ����Ϊ������չ��ȫΪ1
		sum += temp1 | temp2;
	}
	while (sum > 0xffff) {//�Ѽӷ�����Ĳ��֣����ڸ�16λ���ӵ���16λ��
		sum = (sum & 0xffff) + (sum >> 16);
	}
	return (unsigned short)(~(sum | 0xffff0000));//��16λȡ������16λȫΪ0
}
/*�����Ƿ����*/
int Corrupt(const char* s, const int length)
{
	unsigned int sum = 0;
	unsigned short temp1 = 0;//����������޷��������������Ϊ������չ���õ�һЩ����ֵֹ���
	unsigned short temp2 = 0;
	for (int i = 0; i < length; i += 2) {
		temp1 = s[i + 1] << 8;
		temp2 = s[i] & 0x00ff;//һ��Ҫ�Ѹ�8λ���㣡���������8λ���λ��1����ô��8λҲ����Ϊ������չ��ȫΪ1
		sum += temp1 | temp2;
	}
	while (sum > 0xffff) {//�Ѽӷ�����Ĳ��֣����ڸ�16λ���ӵ���16λ��
		sum = (sum & 0xffff) + (sum >> 16);
	}
	if (sum == 0xFFFF)
		return 0;
	else
		return 1;
}
/*  ����ȷ�����ݱ�
��һ���ֽڣ�package_ack
�ڶ����ֽڣ���ŵĸ�λ
�������ֽڣ���ŵĵ�λ
���ĸ��ֽڣ������ checksum�ĵ�λ
������ֽڣ������ checksum�ĸ�λ
*/
int make_package_ack(unsigned short num, char* buffer)
{
	int position = 0;
	buffer[position++] = package_ack;
	buffer[position++] = (char)(num >> 8);//��ŵĸ�λ
	buffer[position++] = (char)num;
	buffer[position++] = 0;//�ֽڶ���
	unsigned short temp = generate_checksum(buffer, position);
	buffer[position++] = (char)(temp);
	buffer[position++] = (char)(temp >> 8);
	return position;
}
/*  Package_req:
��һ���ֽڣ�package_req_1,package_req_2,package_req_3����֮һ����ʾ�������ֽ��еĳ̶�
�ڶ����ֽڣ��ļ����Ƴ���
�ֽڶ��� �ļ�����
 */
int make_package_req(package_type Type, char* filename, char* buffer)
{
	int position = 0;
	buffer[position++] = Type;  //package_req_1,package_req_2,package_req_3����֮һ����ʾ�������ֽ��еĳ̶�
	buffer[position++] = (char)(strlen(filename));
	for (int i = 0; i < strlen(filename); i++)
	{
		buffer[position++] = filename[i];
	}
	//buffer[position++] = generate_checksum(buffer,position);
	return position;
}
/*  Package_data:
��һ���ֽ�:package_data
�ڶ����ֽڣ���ŵĸ�λ
�������ֽڣ���ŵĵ�λ
���ĸ��ֽڣ����ݴ�С�ĸ�λ
������ֽڣ����ݴ�С�ĵ�λ
�������ֽڣ��ļ�����λ����ΪFILE_LAST_ACK���ʾ�����ļ����һ���֣���FILE_FIRST_ACK��FILE_MIDDLE_ACK���ʾ�ļ���û���������
�ֽڶ��� �������ݣ����1025Bytes��
�����ڶ����ֽڣ������ checksum�ĵ�λ
���һ���ֽڣ������checksum�ĸ�λ  */
int make_package_data(unsigned short num, ack_type ifEnd, char* data, unsigned short data_size, char* buffer)
{
	int position = 0;
	buffer[position++] = package_data;
	buffer[position++] = (char)(num >> 8);
	buffer[position++] = (char)num;
	buffer[position++] = (char)(data_size >> 8);
	buffer[position++] = (char)data_size;
	buffer[position++] = (char)ifEnd;
	memcpy(&buffer[position], data, data_size);
	position = position + 1 + data_size;
	if (position % 2 != 0)
		buffer[position++] = '\0';
	unsigned short temp = generate_checksum(buffer, position);
	buffer[position++] = (char)(temp);
	buffer[position++] = (char)(temp >> 8);
	return position;
}
//�ϴ�
int upload(SOCKET clientSocket, char command[][256])
{   //���ñ��ػ���ͱ���
	char send_buffer[1536] = { 0 };  //�����ڷ�����������ʹ��**cache
	char recv_buffer[1536] = { 0 };
	char data_buffer[1024] = { 0 };
	//���÷������ĵ�ַ
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr.sin_port = htons(69);
	//��һ������
	int package_length = make_package_req(package_req_1, command[1], send_buffer);
	int ret = sendto(clientSocket, send_buffer, package_length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
	//�������Է����������ֻظ�
	struct timeval timeout = { 20,0 };
	fd_set fdr;
	FD_ZERO(&fdr);
	FD_SET(clientSocket, &fdr);
	ret = select(clientSocket, &fdr, NULL, NULL, NULL);
	if (ret == SOCKET_ERROR)
	{
		cout << "���ӷ�����ʧ��  " << WSAGetLastError() << endl;
		return -1;
	}
	if (ret == 0)
	{
		cout << "���ӳ�ʱ����������" << WSAGetLastError() << endl;
		return -1;
	}
	int len = sizeof(sockaddr);
	ret = recvfrom(clientSocket, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&server_addr, &len);
	//�ڶ������ֳɹ�
	if (recv_buffer[0] == package_req_2)
	{   //����������
		*send_buffer = { 0 };
		*recv_buffer = { 0 };
		package_length = make_package_req(package_req_3, command[1], send_buffer);
		ret = sendto(clientSocket, send_buffer, package_length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
		*send_buffer = { 0 };
		//�򿪱����ļ�
		FILE* file;
		size_t rlen = 0;
		if ((file = fopen(command[1], "rb")) == NULL)
		{
			//cout << command[1] << "��ʧ��" << endl;
			system("dir test.txt");
			getch();
			perror("failed");
			fclose(file);
			return -1;
		}
		cout << "�ļ��򿪳ɹ���" << endl;
		clock_t start, end;
		start = clock();
		unsigned int send_window_size = 300;//���ڴ�С
		unsigned short base = 0;//���ڻ�ַ���
		unsigned short nextseqnum = 0;//��һ���ȴ����͵����

		unsigned int congestion_window_size = 1;//ӵ�����ڴ�С
		unsigned int ssthresh = 300;//��ֵ
		unsigned int dupACKcount = 0;//����ackֵ
		unsigned int congestionavoidACKcount = 0;//���ڼ�����ӵ������׶ε�ʱ���յ����µ�ack
		congestion_stage congestion_state = slow_start;//��ӵ���׶�����Ϊ������

		struct cache_data
		{
			char send_buffer[1536] = { 0 };
			int package_length = 0;
		};
		cache_data cache[300];//���ڻ��淢���˵�����δ��ack���������ݺ����ݴ�С
		int cache_base = 0;//���ڼ�¼base��cache��λ�ã���ȡģ������ʵ��ѭ������
		//unsigned short num = 0;//���
		int filelength = 0;//��¼�ļ��ܴ�С
		ack_type state = FILE_FIRST_ACK;//����״̬Ϊ�ȴ���һ������ack
		int readlength = fread(data_buffer, 1, 1024, file);//��¼ÿ�ζ��������ļ����С
		filelength += readlength;
		if (readlength < 1024 || feof(file))
			state = FILE_LAST_ACK;//���߷�������������������ļ������һ����
		package_length = make_package_data(nextseqnum, state, data_buffer, readlength, cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer);
		cache[(nextseqnum - base + cache_base) % send_window_size].package_length = package_length;
		sendto(clientSocket, cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer, package_length, 0, (sockaddr*)&server_addr, sizeof(sockaddr));
		nextseqnum++;
		while (true)
		{
			struct timeval timeout = { 20,0 };
			*recv_buffer = { 0 };
			FD_ZERO(&fdr);
			FD_SET(clientSocket, &fdr);
			ret = select(clientSocket, &fdr, NULL, NULL, &timeout);
			if (ret == SOCKET_ERROR)
			{
				cout << "socket����  " << WSAGetLastError() << endl;
				fclose(file);
				return -1;
			}
			//��ʱ�ش�,����ӵ��״̬�Ĵ�ʩ����һ�µ�,����ӵ������Ϳ��ٻָ�����Ҫ�ص�������
			else if (ret == 0)
			{
					ssthresh = congestion_window_size / 2;
					congestion_window_size = 1;
					dupACKcount = 0;
					for (int i = base; i < nextseqnum; i++)
						sendto(clientSocket, cache[(i - base + cache_base) % send_window_size].send_buffer, cache[(i - base + cache_base) % send_window_size].package_length, 0, (sockaddr*)&server_addr, sizeof(sockaddr));
					congestion_state = slow_start;
			}
			else
			{
				len = sizeof(sockaddr);
				ret = recvfrom(clientSocket, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&server_addr, &len);
				unsigned short recv_ack = MAKEWORD(recv_buffer[2], recv_buffer[1]);
				//���յ���ȷ��ŷ�Χ�ڵ�ack��������û���𻵣���ȡ�ۼ�ȷ��
				if ((recv_ack >= base) && (recv_ack < nextseqnum) && !(Corrupt(recv_buffer, ret)))
				{
					for (int i = base; i <= recv_ack; i++)
						cout << "�ɹ��������Ϊ" << i << "��СΪ" << cache[(i - base + cache_base) % send_window_size].package_length-10 << "�����ݿ�" << endl;
					//�ж��ǲ��ǽ����ļ�����,���ǵĻ�״̬Ӧ���ǵȴ����һ��ack����nextseqnum���������ݿ����+1�����ҽ�������
					if (state == FILE_LAST_ACK && recv_ack == nextseqnum - 1)
					{
						cout << "�ɹ������ܳ�Ϊ" << filelength << "�ֽڵ�" << command[1] << "�ļ�" << endl;
						end = clock();
						long long time = ((long long)end - (long long)start) / CLOCKS_PER_SEC;
						cout << "����ʱ��Ϊ " << time << " ��" << endl;
						cout << "ƽ��������Ϊ " << ((long long)filelength * 125) / ((long long)time * 128*1024 ) << " Mbps" << endl;
						fclose(file);
						return 0;
					}
					//ӵ��״̬Ϊ���ٻָ�
					else if (congestion_state == fast_recovery)  
					{
						congestion_window_size = ssthresh;
						dupACKcount = 0;
						congestion_state = congestion_avoid;
					}
					//ӵ��״̬Ϊ����������ӵ������
					else 
					{
						switch (congestion_state)
						{
						case slow_start:
							congestion_window_size+=recv_ack-base+1;
							if (ssthresh < congestion_window_size)
								congestion_state = congestion_avoid;
							dupACKcount = 0;
							break;
						case congestion_avoid:
							congestionavoidACKcount += recv_ack - base + 1;
							while(congestionavoidACKcount > congestion_window_size)
							{
								congestionavoidACKcount -=congestion_window_size;
								congestion_window_size++;
							}
							dupACKcount = 0;
							break;
						default:
							break;
						}
						//�ȸ���cache_base,Ȼ���ٴ��ں��ƣ���Ϊ����cache_base��Ҫ�õ�base
						cache_base = (recv_ack + 1 - base + cache_base) % send_window_size;
						base = recv_ack + 1;
						//�����������������������ݣ���������ѭ����������һ��ѭ��ȥ�ȴ�ack
						if (nextseqnum >= base +(send_window_size > congestion_window_size ? congestion_window_size : send_window_size))
							continue;
						//������ʣ��������û�з��꣬����������ʣ�´���
						while (nextseqnum < base + (send_window_size > congestion_window_size ? congestion_window_size : send_window_size))
						{
							if (state == FILE_LAST_ACK)
								break;
							switch (state)
							{
							case FILE_FIRST_ACK:
								state = FILE_MIDDLE_ACK;
								readlength = fread(data_buffer, 1, 1024, file);
								filelength += readlength;
								if (readlength < 1024 || feof(file))
									state = FILE_LAST_ACK;
								memset(cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer, 0, sizeof(cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer));
								package_length = make_package_data(nextseqnum, state, data_buffer, readlength, cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer);
								cache[(nextseqnum - base + cache_base) % send_window_size].package_length = package_length;
								sendto(clientSocket, cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer, package_length, 0, (sockaddr*)&server_addr, sizeof(sockaddr));
								nextseqnum++;
								break;
							case FILE_MIDDLE_ACK:
								readlength = fread(data_buffer, 1, 1024, file);
								filelength += readlength;
								if (readlength < 1024 || feof(file))
									state = FILE_LAST_ACK;
								memset(cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer, 0, sizeof(cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer));
								package_length = make_package_data(nextseqnum, state, data_buffer, readlength, cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer);
								cache[(nextseqnum - base + cache_base) % send_window_size].package_length = package_length;
								sendto(clientSocket, cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer, package_length, 0, (sockaddr*)&server_addr, sizeof(sockaddr));
								nextseqnum++;
								break;
							}
						}
					}
				}
				//���յ���ǰ��ack��Ҳ��������ack
				else if (recv_ack < base && !(Corrupt(recv_buffer, ret)))
				{   //���ٻָ��׶Σ�����Ҫcongestion_window_size++���⻹��Ҫ�ڿɷ��ͷ�Χ�ڼ��������ļ�
					if (congestion_state == fast_recovery)
					{
						congestion_window_size++;
						//�����������������������ݣ���������ѭ����������һ��ѭ��ȥ�ȴ�ack
						if (nextseqnum >= base + (send_window_size > congestion_window_size ? congestion_window_size : send_window_size))
							continue;
						//������ʣ��������û�з��꣬����������ʣ�´���
						while (nextseqnum < base + (send_window_size > congestion_window_size ? congestion_window_size : send_window_size))
						{
							if (state == FILE_LAST_ACK)
								break;
							switch (state)
							{
							case FILE_FIRST_ACK:
								state = FILE_MIDDLE_ACK;
								readlength = fread(data_buffer, 1, 1024, file);
								filelength += readlength;
								if (readlength < 1024 || feof(file))
									state = FILE_LAST_ACK;
								memset(cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer, 0, sizeof(cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer));
								package_length = make_package_data(nextseqnum, state, data_buffer, readlength, cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer);
								cache[(nextseqnum - base + cache_base) % send_window_size].package_length = package_length;
								sendto(clientSocket, cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer, package_length, 0, (sockaddr*)&server_addr, sizeof(sockaddr));
								nextseqnum++;
								break;
							case FILE_MIDDLE_ACK:
								readlength = fread(data_buffer, 1, 1024, file);
								filelength += readlength;
								if (readlength < 1024 || feof(file))
									state = FILE_LAST_ACK;
								memset(cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer, 0, sizeof(cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer));
								package_length = make_package_data(nextseqnum, state, data_buffer, readlength, cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer);
								cache[(nextseqnum - base + cache_base) % send_window_size].package_length = package_length;
								sendto(clientSocket, cache[(nextseqnum - base + cache_base) % send_window_size].send_buffer, package_length, 0, (sockaddr*)&server_addr, sizeof(sockaddr));
								nextseqnum++;
								break;
							}
						}
					}
					//��������ӵ������׶�
					else
					{
						dupACKcount++;
						if (dupACKcount == 3)
						{
							ssthresh = congestion_window_size / 2;
							congestion_window_size = ssthresh+3;
							for (int i = base; i < nextseqnum; i++)
								sendto(clientSocket, cache[(i - base + cache_base) % send_window_size].send_buffer, cache[(i - base + cache_base) % send_window_size].package_length, 0, (sockaddr*)&server_addr, sizeof(sockaddr));

						}
					}
				}
			}
		}
	}
}
//������������в��
int stripcommand(char* s, char cmd[][256])
{
	int i = 0;
	char* token = NULL;
	char seps[] = " ,\t\n";
	token = strtok(s, seps);//�ֽ��ַ���Ϊһ���ַ�����sΪҪ�ֽ���ַ�����delimΪ�ָ����ַ���������delim�а������ַ����ᱻ�˵����������˵��ĵط���Ϊһ���ָ�Ľڵ�
	while (token != NULL)
	{
		if (i > 1) break;
		strcpy(cmd[i], token);
		token = strtok(NULL, seps);//�ڵ�һ�ε���ʱ��strtok()����������s�ַ���������ĵ����򽫲���s���ó�NULL��ÿ�ε��óɹ��򷵻�ָ�򱻷ָ��Ƭ�ε�ָ�롣
		i++;
	}
	return i;
}
//��������
void parse_command(SOCKET socket, char* s)
{
	char cmd[3][256];
	int count = 0;
	count = stripcommand(s, cmd);
	if (strcmp(cmd[0], "upload") == 0)
		upload(socket, cmd);
	if (strcmp(cmd[0], "exit") == 0)
	{
		closesocket(socket);
		cout << "�����ѶϿ�" << endl;
		system("pause");
		exit(0);
	}
}
int main()
{
	WORD require = MAKEWORD(2, 2);
	WSADATA wsadata;
	if (WSAStartup(require, &wsadata) != 0)
	{
		cout << "winsock����ʧ��" << endl;
		return -1;
	}
	if (HIBYTE(wsadata.wVersion) != 2 || LOBYTE(wsadata.wVersion) != 2)
	{
		cout << "�汾����ʧ��" << endl;
		return -1;
	}
	SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);  //�������ݱ��׽���
	if (clientSocket == INVALID_SOCKET)
	{
		cout << "�׽��ִ���ʧ��" << endl;
		return -1;
	}
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = 0;//htonl(INADDR_ANY);
	addr.sin_addr.S_un.S_addr = 0;// INADDR_ANY;
	if ((bind(clientSocket, (struct sockaddr*)&addr, sizeof(addr))) != 0)
	{
		cout << "��ʧ��" << endl;
		return -1;
	}

	//׼������
	cout << "******************�ͻ���׼�����******************" << endl;
	cout << "����������" << endl;
	cout << "--upload  �ϴ��ļ���\n--exit:�˳�\n";
	while (true)
	{
		char command[256];
		fflush(stdin);
		cout << ">> ";
		gets_s(command);
		parse_command(clientSocket, command);
	}
	return 0;
}