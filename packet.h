/*
 * packet.h
 *
 *  Created on: 18-Jun-2014
 *      Author: root
 */

#ifndef PACKET_H_
#define PACKET_H_

#include <semaphore.h>

#define RESET_CALIBRATION_REPORT_LEN 		8
#define READ_PARAMETER_REPORT_LEN			5
#define GET_SERIAL_NUMBER_REPORT_LEN		7
#define CONTROLLER_STATUS_REPORT_LEN 		20
#define CONTROLLER_24ID_REPORT_LEN 			24
#define CONTROLLER_16ID_REPORT_LEN			16
#define CALIBRAION_3POINT_REPORT_LEN		33
#define COM_LEN_BUFFER						8
#define REPORT_LEN							50

#define COMMAND_REQUEST	_IOWR('U', 1, struct setup_packet)

void send_to_game(int, unsigned int, unsigned int);
void rgs_controller_status_command();
void rgs_read_parameter_orientation_command();
void rgs_read_parameter_serial_command();
void rgs_soft_reset_command();
void rgs_hard_reset_command();
void rgs_calibration_command();
void rgs_restore_default_command();
void rgs_16_controller_id_command();
void rgs_24_controller_id_command();
void rgs_get_serial_number_command();
void rgs_3point_calibration_command();

void command_response_to_game(char*, unsigned int);

#pragma pack (push , 1)
struct setup_packet {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
	uint8_t *response;
};

struct Mouse_Data_Report{
    uint8_t Report_ID;
    uint8_t Reserved;
    uint8_t Status;
    uint8_t X_Compensated_lsb;
    uint8_t X_Compensated_msb;
    uint8_t Y_Compensated_lsb;
    uint8_t Y_Compensated_msb;
    uint8_t X_raw_lsb;
    uint8_t X_raw_msb;
    uint8_t Y_raw_lsb;
    uint8_t Y_raw_msb;
};
#pragma pack (pop)

int fd_maingame_command, fd_usbcore_command, fd_maingame_event;
struct timespec timeout;
sem_t lock;

#endif /* PACKET_H_ */
