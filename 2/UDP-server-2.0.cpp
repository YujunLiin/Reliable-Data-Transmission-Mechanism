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
enum  ack_type  //区分接收的程度
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
//客户端上传文件
int upload(SOCKET serverSocket, sockaddr_in client_addr, char* filename)
{
	FILE* file;
	if ((file = fopen(filename, "rb")) != NULL)
	{
		cout << "文件已经存在" << endl;
		system("dir test.txt");
		getch();
		fclose(file);
		return -1;
	}
	if ((file = fopen(filename, "w+b")) == NULL)
	{
		cout << "文件创建失败" << endl;
		return -1;
	}
	cout << "已准备好接收文件！" << endl;
	//设置本地缓存和变量
	char send_buffer[1536] = { 0 };
	char recv_buffer[1536] = { 0 };
	fd_set fdr;
	int ret = 0;
	//unsigned short  num = 0;//接收序号
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
			cout << "socket出错  " << WSAGetLastError() << endl;
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
				//接收到正确序号的数据并且数据没有损坏
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
						cout << "成功文件名为" << filename << "大小为" << filelength << "的文件！" << endl;
						fclose(file);
						return 0;
					}
					else
					{
						fwrite(&recv_buffer[6], 1, 1024, file);
						filelength = filelength + 1024;
						//期待序号增加
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
		cout << "winsock加载失败" << endl;
		return -1;
	}
	if (HIBYTE(wsadata.wVersion) != 2 || LOBYTE(wsadata.wVersion) != 2)
	{
		cout << "版本请求失败" << endl;
		return -1;
	}
	SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, 0);  //创建数据报套接字
	if (serverSocket == INVALID_SOCKET)
	{
		cout << "套接字创建失败" << endl;
		return -1;
	}
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(69);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	if ((bind(serverSocket, (struct sockaddr*)&addr, sizeof(addr))) != 0)
	{
		cout << "绑定失败" << endl;
		return -1;
	}
	//准备就绪
	cout << "******************服务器准备完毕******************" << endl;
	//初始化客户端地址
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
		/*将指定的文件描述符集清空,在对文件描述符集合进行设置前，必须对其进行初始化，
		如果不清空，由于在系统分配内存空间后，通常并不作清空处理，所以结果是不可知的。*/
		FD_ZERO(&fdr);
		/*用于在文件描述符集合中增加一个新的文件描述符*/
		FD_SET(serverSocket, &fdr);
		/*返回对应位仍然为1的fd的总数,采用select来查看套节字是否可读,最后一个参数是
		select的超时时间，这个参数至关重要，它可以使select处于三种状态，第一，若将
		NULL以形参传入，即不传入时间结构，就是将select置于阻塞状态，
		一定等到监视文件描述符集合中某个文件描述符发生变化为止；*/
		int ret = select(serverSocket, &fdr, NULL, NULL, NULL);
		if (ret == SOCKET_ERROR)
		{
			cout << "socket出错  " << WSAGetLastError() << endl;
			return -1;
		}
		else
		{
			if (FD_ISSET(serverSocket, &fdr))//用宏FD_ISSET检查某个fd在函数select调用后，相应位是否仍然为1
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
					//接收来自客户端的握手回复
					struct timeval timeout = { 50,0 };
					fd_set fdr;
					FD_ZERO(&fdr);
					FD_SET(serverSocket, &fdr);
					ret = select(serverSocket, &fdr, NULL, NULL, NULL);
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
					*recv_buffer = { 0 };
					ret = recvfrom(serverSocket, recv_buffer, sizeof(recv_buffer), 0, (sockaddr*)&client_addr, &len);
					//等待接收到来自客户端的第三次握手
					char filename3[256];
					unsigned short filename3_length = recv_buffer[1];
					for (int i = 0; i < filename3_length; i++)
					{
						filename3[i] = recv_buffer[2 + i];
					}
					filename3[filename3_length] = '\0';
					//第三次握手成功
					if (recv_buffer[0] == package_req_3 && (strcmp(filename1, filename3) == 0))
					{
						upload(serverSocket, client_addr, filename3);
					}
				}
			}
		}
	}
}