#include<iostream>
#include<stdio.h>
#include<conio.h>
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
enum  ack_type  //���ֽ��յĳ̶�
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
//�ͻ����ϴ��ļ�
int upload(SOCKET serverSocket, sockaddr_in client_addr, char* filename)
{
	FILE* file;
	if ((file = fopen(filename, "rb")) != NULL)
	{
		cout << "�ļ��Ѿ�����" << endl;
		system("dir test.txt");
		getch();
		fclose(file);
		return -1;
	}
	if ((file = fopen(filename, "w+b")) == NULL)
	{
		cout << "�ļ�����ʧ��" << endl;
		return -1;
	}
	cout << "��׼���ý����ļ���" << endl;
	//���ñ��ػ���ͱ���
	char send_buffer[1536] = { 0 };
	char recv_buffer[1536] = { 0 };
	fd_set fdr;
	int ret = 0;
	//unsigned short  num = 0;//�������
	unsigned short expectedseqnum = 0;
	int package_length = 0;
	int filelength = 0;
	while (true)
	{
		struct timeval timeout = { 0,3000 };
		FD_ZERO(&fdr);
		FD_SET(serverSocket, &fdr);
		ret = select(serverSocket, &fdr, NULL, NULL, &timeout);
		if (ret == SOCKET_ERROR)
		{
			cout << "socket����  " << WSAGetLastError() << endl;
			fclose(file);
			return -1;
		}
		if (ret == 0)
		{
			//package_length = make_package_ack(expectedseqnum, send_buffer);
			sendto(serverSocket, send_buffer, package_length, 0, (sockaddr*)&client_addr, sizeof(sockaddr));
		}
		else if (FD_ISSET(serverSocket, &fdr))
		{
			*recv_buffer = { 0 };
			int len = sizeof(sockaddr);
			ret = recvfrom(serverSocket, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&client_addr, &len);
			if (recv_buffer[0] == package_data)
			{
				unsigned short recv_data_num = MAKEWORD(recv_buffer[2], recv_buffer[1]);
				//���յ���ȷ��ŵ����ݲ�������û����
				if (recv_data_num == expectedseqnum && !(Corrupt(recv_buffer, ret)))
				{
					*send_buffer = { 0 };
					package_length = make_package_ack(expectedseqnum, send_buffer);
					//sendto(serverSocket, send_buffer, package_length, 0, (sockaddr*)&client_addr, sizeof(sockaddr));
					unsigned short recv_data_length = MAKEWORD(recv_buffer[4], recv_buffer[3]);
					if (recv_buffer[5] == FILE_LAST_ACK)
					{
						fwrite(&recv_buffer[6], 1, recv_data_length, file);
						filelength = filelength + recv_data_length;
						package_length = make_package_ack(expectedseqnum, send_buffer);
						sendto(serverSocket, send_buffer, package_length, 0, (sockaddr*)&client_addr, sizeof(sockaddr));
						cout << "�ɹ��ļ���Ϊ" << filename << "��СΪ" << filelength << "���ļ���" << endl;
						fclose(file);
						return 0;
					}
					else
					{
						fwrite(&recv_buffer[6], 1, 1024, file);
						filelength = filelength + 1024;
						//�ڴ��������
						expectedseqnum++;
						//cout << expectedseqnum << endl;
					}
				}
				else
					sendto(serverSocket, send_buffer, package_length, 0, (sockaddr*)&client_addr, sizeof(sockaddr));
			}
		}
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
	SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, 0);  //�������ݱ��׽���
	if (serverSocket == INVALID_SOCKET)
	{
		cout << "�׽��ִ���ʧ��" << endl;
		return -1;
	}
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(69);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	if ((bind(serverSocket, (struct sockaddr*)&addr, sizeof(addr))) != 0)
	{
		cout << "��ʧ��" << endl;
		return -1;
	}
	//׼������
	cout << "******************������׼�����******************" << endl;
	//��ʼ���ͻ��˵�ַ
	sockaddr_in client_addr;
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(INADDR_ANY);//4001);
	client_addr.sin_addr.s_addr = INADDR_ANY;//inet_addr(" 127.0.0.1");//	 inet_addr("10.136.138.243");

	int package_length = 0;
	int ret = 0;
	fd_set fdr;
	while (true)
	{
		char recv_buffer[1536] = { 0 };
		/*��ָ�����ļ������������,�ڶ��ļ����������Ͻ�������ǰ�����������г�ʼ����
		�������գ�������ϵͳ�����ڴ�ռ��ͨ����������մ������Խ���ǲ���֪�ġ�*/
		FD_ZERO(&fdr);
		/*�������ļ�����������������һ���µ��ļ�������*/
		FD_SET(serverSocket, &fdr);
		/*���ض�Ӧλ��ȻΪ1��fd������,����select���鿴�׽����Ƿ�ɶ�,���һ��������
		select�ĳ�ʱʱ�䣬�������������Ҫ��������ʹselect��������״̬����һ������
		NULL���βδ��룬��������ʱ��ṹ�����ǽ�select��������״̬��
		һ���ȵ������ļ�������������ĳ���ļ������������仯Ϊֹ��*/
		int ret = select(serverSocket, &fdr, NULL, NULL, NULL);
		if (ret == SOCKET_ERROR)
		{
			cout << "socket����  " << WSAGetLastError() << endl;
			return -1;
		}
		else
		{
			if (FD_ISSET(serverSocket, &fdr))//�ú�FD_ISSET���ĳ��fd�ں���select���ú���Ӧλ�Ƿ���ȻΪ1
			{
				int len = sizeof(sockaddr);
				ret = recvfrom(serverSocket, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&client_addr, &len);
				if (recv_buffer[0] == package_req_1)
				{
					char filename1[256];
					unsigned short filename1_length = recv_buffer[1];
					char send_buffer[1536] = { 0 };
					for (int i = 0; i < filename1_length; i++)
					{
						filename1[i] = recv_buffer[2 + i];
					}
					filename1[filename1_length] = '\0';
					package_length = make_package_req(package_req_2, filename1, send_buffer);
					ret = sendto(serverSocket, send_buffer, package_length, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					//�������Կͻ��˵����ֻظ�
					struct timeval timeout = { 50,0 };
					fd_set fdr;
					FD_ZERO(&fdr);
					FD_SET(serverSocket, &fdr);
					ret = select(serverSocket, &fdr, NULL, NULL, NULL);
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
					*recv_buffer = { 0 };
					ret = recvfrom(serverSocket, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&client_addr, &len);
					//�ȴ����յ����Կͻ��˵ĵ���������
					char filename3[256];
					unsigned short filename3_length = recv_buffer[1];
					for (int i = 0; i < filename3_length; i++)
					{
						filename3[i] = recv_buffer[2 + i];
					}
					filename3[filename3_length] = '\0';
					//���������ֳɹ�
					if (recv_buffer[0] == package_req_3 && (strcmp(filename1, filename3) == 0))
					{
						upload(serverSocket, client_addr, filename3);
					}
				}
			}
		}
	}
}