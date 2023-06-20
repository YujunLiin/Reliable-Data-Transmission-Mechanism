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
	char send_buffer[1536] = { 0 };
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
			system("dir 1.txt");
			getch();
			perror("failed");
			fclose(file);
			return -1;
		}
		cout << "�ļ��򿪳ɹ���" << endl;
		clock_t start, end;
		start = clock();
		unsigned short num = 0;//���
		int filelength = 0;//��¼�ļ��ܴ�С
		ack_type state = FILE_FIRST_ACK;//����״̬Ϊ�ȴ���һ������ack
		size_t readlength = fread(data_buffer, 1, 1024, file);//��¼ÿ�ζ��������ļ����С
		filelength += readlength;
		if (readlength < 1024 || feof(file))
			state = FILE_LAST_ACK;//���߷�������������������ļ������һ����
		package_length = make_package_data(num, state, data_buffer, readlength, send_buffer);
		sendto(clientSocket, send_buffer, package_length, 0, (sockaddr*)&server_addr, sizeof(sockaddr));
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
			//��ʱ�ش�
			else if (ret == 0)
			{
				sendto(clientSocket, send_buffer, package_length, 0, (sockaddr*)&server_addr, sizeof(sockaddr));
			}
			else
			{
				len = sizeof(sockaddr);
				ret = recvfrom(clientSocket, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&server_addr, &len);
				unsigned short recv_ack = MAKEWORD(recv_buffer[2], recv_buffer[1]);
				//���յ���ȷ��ŵ�ack��������û����
				if (recv_ack == num && !(Corrupt(recv_buffer, ret)))
				{
					cout << "�ɹ��������Ϊ" << num << "��СΪ" << readlength << "�����ݿ�" << endl;
					//��ת���
					if (num == 0)
						num = 1;
					else
						num = 0;
					switch (state)
					{
					case FILE_FIRST_ACK:
						state = FILE_MIDDLE_ACK;
						readlength = fread(data_buffer, 1, 1024, file);
						filelength += readlength;
						if (readlength < 1024 || feof(file))
							state = FILE_LAST_ACK;
						*send_buffer = { 0 };
						package_length = make_package_data(num, state, data_buffer, readlength, send_buffer);
						sendto(clientSocket, send_buffer, package_length, 0, (sockaddr*)&server_addr, sizeof(sockaddr));
						break;
					case FILE_MIDDLE_ACK:
						readlength = fread(data_buffer, 1, 1024, file);
						filelength += readlength;
						if (readlength < 1024 || feof(file))
							state = FILE_LAST_ACK;
						*send_buffer = { 0 };
						package_length = make_package_data(num, state, data_buffer, readlength, send_buffer);
						sendto(clientSocket, send_buffer, package_length, 0, (sockaddr*)&server_addr, sizeof(sockaddr));
						break;
					case FILE_LAST_ACK:
						end = clock();
						long long time = (long long)(end - start) / CLOCKS_PER_SEC;
						cout << "�ɹ������ܳ�Ϊ " << filelength << " �ֽڵ��ļ���" << endl;
						cout << "����ʱ��Ϊ " << time<< " ��" << endl;
						cout << "ƽ��������Ϊ " << (filelength * 125) / (time * 128 * 1024) << " Mbps" << endl;
						fclose(file);
						return 0;
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
	addr.sin_port = htonl(INADDR_ANY);
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
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