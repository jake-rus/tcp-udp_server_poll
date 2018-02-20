/*
 * main.c
 *	Сервер tcp-udp с мультиплексированием poll
 *	tcp создаются процессами, завершение отслеживается каналом pipe
 *  Created on: 19 февр. 2018 г.
 *      Author: jake
 */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <pthread.h>

#define TCPPORT 1234
#define UDPPORT 1235

int main(void)
{
	int tcpSock,ws, udpSock; // сокеты
	char buf[BUFSIZ];		// буфер для сообщений
	int count_client=0;		// счетчик клиентов
	pid_t pid;		// ид процессов tcp
	struct sockaddr_in tcp_serv;
	struct sockaddr_in udp_serv, from;
	int fromlen = sizeof(from);
	struct pollfd fds[3];	// структура мультиплексора сокетов
	int fd[2];				// дескрипторы для pipe

	// обнуление длины и структур
	memset(&from,0,fromlen);
	bzero(&tcp_serv,sizeof(tcp_serv));
	bzero(&udp_serv,sizeof(udp_serv));

/* Инициализация tcp сокета */
	tcpSock = socket(AF_INET,SOCK_STREAM,0);
	tcp_serv.sin_port = htons(TCPPORT);
	tcp_serv.sin_family = AF_INET;
	tcp_serv.sin_addr.s_addr = htonl(INADDR_ANY);//0.0.0.0
	if (bind(tcpSock,(struct sockaddr*) &tcp_serv, sizeof(tcp_serv)) < 0)
	{
		perror("tcp binding error");
		exit(2);
	}
	listen(tcpSock,SOMAXCONN);
/*-------------------------*/
/* Инициализация udp сокета*/
	udpSock = socket(AF_INET,SOCK_DGRAM,0);
	if (udpSock < 0)
	{
		perror("socket error");
		exit(3);
	}
	udp_serv.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // или так
	udp_serv.sin_port = htons(UDPPORT);
	udp_serv.sin_family = AF_INET;
	if (bind(udpSock, (struct sockaddr*) &udp_serv, sizeof(udp_serv)) < 0)
	{
		perror("udp binding error");
		exit(4);
	}
/*-------------------------*/

/* Создаем канал между подпроцессами tcp и родителем */
	if(pipe(fd) < 0){
		perror("Can\'t create pipe");
		exit(5);
	}

/* НАБОР для poll - сокеты и канал для дочерних процессов*/
	fds[0].fd=tcpSock;
	fds[1].fd=udpSock;
	fds[2].fd=fd[0];
	fds[0].events=POLLIN;
	fds[1].events=POLLIN;
	fds[2].events=POLLIN;
	printf("Waiting connect...\n");

/* Цикличная обработка подключений*/
	while(1)
	{
		if (poll(fds, 3, -1)==-1) { // ждем клиента или сигнала об окончании
			perror("poll failed");
			exit(6);
		}
/* При поступлении сообщения от дечернего процесса принимаем сигнал завершения*/
		if (fds[2].revents == POLLIN)
		{
			read(fd[0],&pid,sizeof(unsigned));
			waitpid(pid,0,0);
		}

/* Появление tcp клиента*/
		if (fds[0].revents == POLLIN) {
			ws=accept(tcpSock,NULL,NULL);
			if (0==(pid=fork())) { // процесс для tcp-клиента
				close(tcpSock); // закрыть дескр. слушающего сервера
				if(recv(ws,buf,BUFSIZ,0)<0)
				{
					perror("recv error");
					exit(7);
				}
				strcat(buf,"->");
				send(ws,buf,strlen(buf)+1,0);
				sleep(1);
				shutdown(ws,SHUT_RDWR);
				close(ws);
				pid=getpid();
				write(fd[1],&pid,sizeof(unsigned));
				exit(0);
			} else {// основной процесс ждет завершения tcp-процессов отдельными потоками
				printf("incoming tcp-client - %d\n",++count_client);
				close(ws);
			}
			fds[0].revents=0;
			continue;
		}

/* Появление udp клиента*/
		if (fds[1].revents ==POLLIN) {
			printf("incoming udp-client - %d\n",++count_client);
			if (recvfrom(udpSock,buf,sizeof(buf),0,(struct sockaddr*)&from,&fromlen) <0)
			{
				perror("recvfrom error");
				exit(8);
			}
			//printf("%s\n",buf);
			strcat(buf,"->");
			sendto(udpSock,buf,strlen(buf)+1,0,(struct sockaddr*)&from, fromlen);
			memset(buf,0,BUFSIZ);
		}
	}

/* Закрытие сокетов*/
	shutdown(tcpSock,SHUT_RDWR);
	shutdown(udpSock,SHUT_RDWR);
	close(tcpSock);
	close(udpSock);
	return 0;
}
