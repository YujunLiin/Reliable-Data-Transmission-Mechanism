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
enum  ack_type//区分接收的程度
{
	FILE_FIRST_ACK,
	FILE_MIDDLE_ACK,
	FILE_LAST_ACK
};
/*  生成检验和：
	s：需检验的内容
	length：s的大小
	返回值为unsigned short型的checksum  */
unsigned short generate_checksum(const char* s, const int length) {
	unsigned int sum = 0;
	unsigned short temp1 = 0;//这里必须是无符号数，否则会因为符号扩展，得到一些奇奇怪怪的数
	unsigned short temp2 = 0;
	for (int i = 0; i < length; i += 2) {
		temp1 = s[i + 1] << 8;
		temp2 = s[i] & 0x00ff;//一定要把高8位置零！否则如果低8位最高位是1，那么高8位也会因为符号扩展而全为1
		sum += temp1 | temp2;
	}
	while (sum > 0xffff) {//把加法溢出的部分（存在高16位）加到低16位上
		sum = (sum & 0xffff) + (sum >> 16);
	}
	return (unsigned short)(~(sum | 0xffff0000));//低16位取反，高16位全为0
}
/*检验是否损毁*/
int Corrupt(const char* s, const int length)
{
	unsigned int sum = 0;
	unsigned short temp1 = 0;//这里必须是无符号数，否则会因为符号扩展，得到一些奇奇怪怪的数
	unsigned short temp2 = 0;
	for (int i = 0; i < length; i += 2) {
		temp1 = s[i + 1] << 8;
		temp2 = s[i] & 0x00ff;//一定要把高8位置零！否则如果低8位最高位是1，那么高8位也会因为符号扩展而全为1
		sum += temp1 | temp2;
	}
	while (sum > 0xffff) {//把加法溢出的部分（存在高16位）加到低16位上
		sum = (sum & 0xffff) + (sum >> 16);
	}
	if (sum == 0xFFFF)
		return 0;
	else
		return 1;
}
/*  生成确认数据报
第一个字节：package_ack
第二个字节：序号的高位
第三个字节：序号的低位
第四个字节：检验和 checksum的低位
第五个字节：检验和 checksum的高位
*/
int make_package_ack(unsigned short num, char* buffer)
{
	int position = 0;
	buffer[position++] = package_ack;
	buffer[position++] = (char)(num >> 8);//序号的高位
	buffer[position++] = (char)num;
	buffer[position++] = 0;//字节对齐
	unsigned short temp = generate_checksum(buffer, position);
	buffer[position++] = (char)(temp);
	buffer[position++] = (char)(temp >> 8);
	return position;
}
/*  Package_req:
第一个字节：package_req_1,package_req_2,package_req_3三种之一，表示三次握手进行的程度
第二个字节：文件名称长度
字节对齐 文件名称
 */
int make_package_req(package_type Type, char* filename, char* buffer)
{
	int position = 0;
	buffer[position++] = Type;  //package_req_1,package_req_2,package_req_3三种之一，表示三次握手进行的程度
	buffer[position++] = (char)(strlen(filename));
	for (int i = 0; i < strlen(filename); i++)
	{
		buffer[position++] = filename[i];
	}
	//buffer[position++] = generate_checksum(buffer,position);
	return position;
}
/*  Package_data:
第一个字节:package_data
第二个字节：序号的高位
第三个字节：序号的地位
第四个字节：数据大小的高位
第五个字节：数据大小的地位
第六个字节：文件结束位（若为FILE_LAST_ACK则表示这是文件最后一部分，而FILE_FIRST_ACK和FILE_MIDDLE_ACK则表示文件还没传输结束）
字节对齐 传输数据（最大1025Bytes）
倒数第二个字节：检验和 checksum的低位
最后一个字节：检验和checksum的高位  */
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
//上传
int upload(SOCKET clientSocket, char command[][256])
{   //设置本地缓存和变量
	char send_buffer[1536] = { 0 };
	char recv_buffer[1536] = { 0 };
	char data_buffer[1024] = { 0 };
	//设置服务器的地址
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr.sin_port = htons(69);
	//第一次握手
	int package_length = make_package_req(package_req_1, command[1], send_buffer);
	int ret = sendto(clientSocket, send_buffer, package_length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
	//接收来自服务器的握手回复
	struct timeval timeout = { 20,0 };
	fd_set fdr;
	FD_ZERO(&fdr);
	FD_SET(clientSocket, &fdr);
	ret = select(clientSocket, &fdr, NULL, NULL, NULL);
	if (ret == SOCKET_ERROR)
	{
		cout << "连接服务器失败  " << WSAGetLastError() << endl;
		return -1;
	}
	if (ret == 0)
	{
		cout << "连接超时，结束连接" << WSAGetLastError() << endl;
		return -1;
	}
	int len = sizeof(sockaddr);
	ret = recvfrom(clientSocket, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&server_addr, &len);
	//第二次握手成功
	if (recv_buffer[0] == package_req_2)
	{   //第三次握手
		*send_buffer = { 0 };
		*recv_buffer = { 0 };
		package_length = make_package_req(package_req_3, command[1], send_buffer);
		ret = sendto(clientSocket, send_buffer, package_length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
		*send_buffer = { 0 };
		//打开本地文件
		FILE* file;
		size_t rlen = 0;
		if ((file = fopen(command[1], "rb")) == NULL)
		{
			//cout << command[1] << "打开失败" << endl;
			system("dir 1.txt");
			getch();
			perror("failed");
			fclose(file);
			return -1;
		}
		cout << "文件打开成功！" << endl;
		clock_t start, end;
		start = clock();
		unsigned short num = 0;//序号
		int filelength = 0;//记录文件总大小
		ack_type state = FILE_FIRST_ACK;//设置状态为等待第一个数据ack
		size_t readlength = fread(data_buffer, 1, 1024, file);//记录每次读出来的文件块大小
		filelength += readlength;
		if (readlength < 1024 || feof(file))
			state = FILE_LAST_ACK;//告诉服务器端这是所传输的文件的最后一部分
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
				cout << "socket出错  " << WSAGetLastError() << endl;
				fclose(file);
				return -1;
			}
			//超时重传
			else if (ret == 0)
			{
				sendto(clientSocket, send_buffer, package_length, 0, (sockaddr*)&server_addr, sizeof(sockaddr));
			}
			else
			{
				len = sizeof(sockaddr);
				ret = recvfrom(clientSocket, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&server_addr, &len);
				unsigned short recv_ack = MAKEWORD(recv_buffer[2], recv_buffer[1]);
				//接收到正确序号的ack并且数据没有损坏
				if (recv_ack == num && !(Corrupt(recv_buffer, ret)))
				{
					cout << "成功接收序号为" << num << "大小为" << readlength << "的数据块" << endl;
					//反转序号
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
						cout << "成功发送总长为 " << filelength << " 字节的文件！" << endl;
						cout << "传输时间为 " << time<< " 秒" << endl;
						cout << "平均吞吐率为 " << (filelength * 125) / (time * 128 * 1024) << " Mbps" << endl;
						fclose(file);
						return 0;
					}
				}
			}
		}
	}
}
//把命令输入进行拆分
int stripcommand(char* s, char cmd[][256])
{
	int i = 0;
	char* token = NULL;
	char seps[] = " ,\t\n";
	token = strtok(s, seps);//分解字符串为一组字符串。s为要分解的字符串，delim为分隔符字符串。所有delim中包含的字符都会被滤掉，并将被滤掉的地方设为一处分割的节点
	while (token != NULL)
	{
		if (i > 1) break;
		strcpy(cmd[i], token);
		token = strtok(NULL, seps);//在第一次调用时，strtok()必需给予参数s字符串，往后的调用则将参数s设置成NULL。每次调用成功则返回指向被分割出片段的指针。
		i++;
	}
	return i;
}
//调用命令
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
		cout << "连接已断开" << endl;
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
		cout << "winsock加载失败" << endl;
		return -1;
	}
	if (HIBYTE(wsadata.wVersion) != 2 || LOBYTE(wsadata.wVersion) != 2)
	{
		cout << "版本请求失败" << endl;
		return -1;
	}
	SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, 0);  //创建数据报套接字
	if (clientSocket == INVALID_SOCKET)
	{
		cout << "套接字创建失败" << endl;
		return -1;
	}
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htonl(INADDR_ANY);
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
	if ((bind(clientSocket, (struct sockaddr*)&addr, sizeof(addr))) != 0)
	{
		cout << "绑定失败" << endl;
		return -1;
	}

	//准备就绪
	cout << "******************客户端准备完毕******************" << endl;
	cout << "请输入命令" << endl;
	cout << "--upload  上传文件名\n--exit:退出\n";
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