#include <stdio.h>
#include <stdint.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <linux/version.h>
#include <linux/input.h>
#include <linux/usbdevice_fs.h>

#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <ctype.h>
#include <pthread.h>
//#include <syslog.h>

#include <sys/ioctl.h>
#include <termios.h>
#include "packet.h"

#define CLIENT_PORT 7777
#define SERVER_PORT 7000

#define CLIENT_XY				1
#define SERVER_COMMAND			1
#define USE_USB					0

#if CLIENT_XY
/* client*/
#define MAXSIZE 1024
struct sockaddr_in server_info;
struct hostent *he;
int socket_client_fd, num, ret;
int on = 1;

//#else
/* server*/
#define BACKLOG 10
struct sockaddr_in server;
struct sockaddr_in dest;
int socket_server_fd, client_fd, num;
socklen_t size;

int yes = 1;

#endif

void usb_port_init()
{
	system("/etc/init.d/udev restart");
	do {
		fd_maingame_event = open("/dev/input/event0", O_RDONLY | O_NOCTTY);
		printf("usb event port not available\n");
		usleep(10000);
	} while (fd_maingame_event == -1);

	printf("******usb event port opened successfully******\n");
	do {
		fd_usbcore_command = open("/dev/usbcommand", O_RDWR | O_NOCTTY);
		printf("usb command port not available\n");
		usleep(10000);
	} while (fd_maingame_command == -1);

	printf("******usb command port opened successfully******\n");
}

#if 1
void *rgs_command_process() {

//	fd_set read_maingame;
//	short int descriptor;
	while (1) {

		size = sizeof(struct sockaddr_in);

		if ((client_fd = accept(socket_server_fd, (struct sockaddr *) &dest,
				&size)) == -1) {
			perror("accept");
			exit(1);
		}
		printf("Server got connection from client %s\n",
				inet_ntoa(dest.sin_addr));

#if 0
		while (1) {
			memset(datastage, 0, sizeof(datastage));

			FD_ZERO(&read_maingame);
			FD_SET(fd_maingame_command, &read_maingame);

			/* 5 sec read timeout for main game*/
			timeout.tv_sec = 5;
			timeout.tv_nsec = 0;

			descriptor = pselect(fd_maingame_command + 1, &read_maingame,
					NULL, NULL, &timeout, NULL);

			if ((descriptor == 0) || (descriptor == -1))
			continue;

			if (FD_ISSET(fd_maingame_command, &read_maingame)) {

				cmd_len = read(fd_maingame_command, datastage, COM_LEN - 1);
				printf("Report length :%d\t DATA: ", cmd_len);
				for (i = 0; i < cmd_len; i++)
				printf(" %02x", datastage[i]);
				printf("\n");
			}
		}
#endif

		while (1) {
			char choice[COM_LEN_BUFFER] = { 0 };

			if ((num = recv(client_fd, choice, 1024, 0)) == -1) {
				perror("recv");
				exit(1);
			} else if (num == 0) {
				printf("Connection closed\n");
				//So I can now wait for another client
				break;
			}

			sem_wait(&lock);

			static unsigned char response_data[REPORT_LEN] = { 0 };

			struct setup_packet command_packet;
			command_packet.bmRequestType = choice[0];
			command_packet.bRequest = choice[1];
			command_packet.wValue = (choice[3] << 8 | choice[2]);
			command_packet.wIndex = (choice[5] << 8 | choice[4]);
			command_packet.wLength = (choice[7] << 8 | choice[6]);
			command_packet.response = response_data;

			printf("Request type : %x\n", command_packet.bmRequestType);
			printf("Request : %x\n", command_packet.bRequest);
			printf("Value : %x\n", command_packet.wValue);
			printf("Index : %x\n", command_packet.wIndex);
			printf("Length : %x\n", command_packet.wLength);

			memset(response_data, 0, sizeof(response_data));

			struct usbdevfs_ioctl COMMAND = { 0, COMMAND_REQUEST,
					&command_packet };

			ioctl(fd_usbcore_command, USBDEVFS_IOCTL, &COMMAND);

			printf("===============command response data===============\n");

			if (command_packet.wLength == 0)
				command_response_to_game((char*)response_data, 8);
			else
				command_response_to_game((char*)response_data, command_packet.wLength);

			sem_post(&lock);
		}
//		close(client_fd);
	}

	/* exit main game thread */
	pthread_exit((void *) NULL);

}
#endif

void send_to_game(int touch, unsigned int xsendtogame, unsigned int ysendtogame) {
//	const char* main_game = "/dev/hidg0";

//	if ((fd_main_game = open(main_game, O_RDWR | O_NOCTTY)) < 0) {
//		perror("main game port open failed\n");
//		return;
//	}

	struct Mouse_Data_Report Mouse_Report = { 0x01, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	Mouse_Report.Status = ((touch == 1) ? 0xc0 : 0x80);
	Mouse_Report.X_Compensated_lsb = (xsendtogame & 0x00ff);
	Mouse_Report.X_Compensated_msb = ((xsendtogame & 0xff00) >> 8);
	Mouse_Report.Y_Compensated_lsb = (ysendtogame & 0x00ff);
	Mouse_Report.Y_Compensated_msb = ((ysendtogame & 0xff00) >> 8);

	Mouse_Report.X_raw_lsb = 0;
	Mouse_Report.X_raw_msb = 0;
	Mouse_Report.Y_raw_lsb = 0;
	Mouse_Report.Y_raw_msb = 0;

	printf("Sending to main game touchstatus %02x, %d X %d \n", touch,
			xsendtogame, ysendtogame);

//	printf(
//			"=======packet data======: %x\t %x\t %x\t %x\t %x\t %x\t %x\t %x\t %x\t\n",
//			Mouse_Report.Status, Mouse_Report.X_Compensated_lsb,
//			Mouse_Report.X_Compensated_msb, Mouse_Report.Y_Compensated_lsb,
//			Mouse_Report.Y_Compensated_msb, Mouse_Report.X_raw_lsb,
//			Mouse_Report.X_raw_msb, Mouse_Report.Y_raw_lsb,
//			Mouse_Report.Y_raw_msb);

#if CLIENT_XY

	if ((send(socket_client_fd, &Mouse_Report, sizeof(struct Mouse_Data_Report),
			0)) == -1) {
		fprintf(stderr, "Failure Sending Message\n");
		close(socket_client_fd);
		exit(1);
	}

#endif
#if USE_USB
	if ((write(fd_maingame_event, &Mouse_Report, sizeof(struct Mouse_Data_Report))) == -1) {
		perror("fd_main_game_event coordinate write failed\n");
	}
#endif

}

void command_response_to_game(char *response, unsigned int cmdlength) {

	unsigned int i = 0;
	for (i = 0; i < cmdlength; i++) {
		printf(" %02x\t", response[i]);
	}
	printf("\n");
	if ((send(client_fd, response, cmdlength, 0)) == -1) {
		fprintf(stderr, "Failure Sending Message\n");
		close(client_fd);
		return;
	}
}

int main(int argc, char **argv) {
	/*	openlog("usb app", LOG_CONS, LOG_CONS);
	 setlogmask(LOG_UPTO(LOG_INFO));*/

	struct input_event copacket;
	int i, rd, touchstatus = 0, count = 0;
	signed int x = 0, y = 0;
	fd_set read_screen;

	pthread_t game_thread;
	int res;
	pthread_attr_t game_attr;

	usb_port_init();
//	if ((fd_maingame_command = open(main_game, O_RDWR | O_NOCTTY)) < 0) {
//		perror("maingame file failed\n");
//		return EXIT_FAILURE;
//	}
	if (sem_init(&lock, 0, 1) != 0) {
		printf("semaphore init failed\n");
		pthread_exit((void *) -1);
	}

	res = pthread_attr_init(&game_attr);
	if (res != 0) {
		printf("Attribute creation failed");
	}

	res = pthread_attr_setdetachstate(&game_attr, PTHREAD_CREATE_DETACHED);
	if (res != 0) {
		printf("Setting detached attribute failed");
	}
	/* creating threads */
	res = pthread_create(&game_thread, &game_attr, &rgs_command_process, NULL);
	if (res != 0) {
		printf("rgs command read maingame failed");
	}
#if SERVER_COMMAND
	/* server */
	if ((socket_server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Socket failure!!\n");
		exit(1);
	}

	if (setsockopt(socket_server_fd, SOL_SOCKET, SO_REUSEADDR, &yes,
			sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}
	memset(&server, 0, sizeof(server));
	memset(&dest, 0, sizeof(dest));
	server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_PORT);
	server.sin_addr.s_addr = INADDR_ANY;
	if ((bind(socket_server_fd, (struct sockaddr *) &server,
			sizeof(struct sockaddr))) == -1)
	{
		fprintf(stderr, "Binding Failure\n");
		exit(1);
	}

	if ((listen(socket_server_fd, BACKLOG)) == -1) {
		fprintf(stderr, "Listening Failure\n");
		exit(1);
	}
#endif

#if	CLIENT_XY
	sleep(10);
	if ((he = gethostbyname("10.10.10.10")) == NULL) {
		fprintf(stderr, "Cannot get host name\n");
		exit(1);
	}

	if ((socket_client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "Socket Failure!!\n");
		exit(1);
	}
	if (setsockopt(socket_client_fd, 0, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
	}

	memset(&server_info, 0, sizeof(server_info));
	server_info.sin_family = AF_INET;
	server_info.sin_port = htons(CLIENT_PORT);
	server_info.sin_addr = *((struct in_addr *) he->h_addr);
	if (connect(socket_client_fd, (struct sockaddr *) &server_info,
			sizeof(struct sockaddr)) < 0) {
		fprintf(stderr, "Connection Failure\n");
		perror("connect");
		exit(1);
	}
#endif

	sem_wait(&lock);
	while (1) {
#if 1
		FD_ZERO(&read_screen);
		FD_SET(fd_maingame_event, &read_screen);

		timeout.tv_sec = 1;
		timeout.tv_nsec = 0;

		count = pselect(fd_maingame_event + 1, &read_screen,
		NULL, NULL, &timeout, NULL);

		if ((count == 0) || (count == -1)) {
//			printf("%s----->%d\n", __func__, __LINE__);
			sem_post(&lock);
			continue;
		}

		if (FD_ISSET(fd_maingame_event, &read_screen)) {
//			printf("%s----->%d\n", __func__, __LINE__);
			rd = read(fd_maingame_event, &copacket, sizeof(struct input_event));

			if (rd == -1) {
//				printf("%s----->%d\n", __func__, __LINE__);
				printf("Error in reading\n");
//				return 0;
				sem_post(&lock);
				sleep(1);
				usb_port_init();
				continue;
			}

			if (rd < (int) sizeof(struct input_event)) {
				printf("expected %d bytes, got %d\n",
						(int) sizeof(struct input_event), rd);
				printf("touch screen port read failed\n");
				sem_post(&lock);
				continue;
			}

//			printf("no of bytes read : %d , Size of structure : %d \n", rd, sizeof(struct input_event));
			for (i = 0; i < rd / sizeof(struct input_event); i++) {
//				printf("***********************Type : %d , code :%d , co-ordinate : %d\n",
//											copacket.type, copacket.code, copacket.value);
				if (copacket.code == BTN_TOUCH && copacket.type == EV_KEY) {
//					printf("Type : %d , code :%d , touch status : %02x\n",
//							copacket.type, copacket.code, copacket.value);
					touchstatus = copacket.value;
				} else if (copacket.code == ABS_X && copacket.type == EV_ABS) {
//					printf("Type : %d , code :%d , x co-ordinate : %d\n",
//							copacket.type, copacket.code, copacket.value);
					x = copacket.value;
				} else if (copacket.code == ABS_Y && copacket.type == EV_ABS) {
//					printf("Type : %d , code :%d , y co-ordinate : %d\n",
//							copacket.type, copacket.code, copacket.value);
					y = copacket.value;
				} else {
//					printf("===============SYNC REPORT================\n");
					send_to_game(touchstatus, x, y);
				}
			}
		}
#endif
	}

	sem_post(&lock);
	close(socket_server_fd);
	close(socket_client_fd);
	return 0;

}
