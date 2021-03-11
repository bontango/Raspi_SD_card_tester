//from: http://www.netzmafia.de/skripten/hardware/RasPi/RasPi_SPI.html
// and https://raspberry-projects.com/pi/programming-in-c/spi/using-the-spi-interface
// and http://www.rjhcoding.com/avrc-sd-interface-1.php


/*
Micro SD shield
	  Card
	1	16 3Volt
GND	2	15 CS
	3	14 MOSI
	4	13 MISO
	5	12 CLK
	6	11	
	7	10
	8	 9 
	
Raspberry PI
SPI MOSI – PIN 19, GPIO 10 - 14
SPI MISO – PIN 21, GPIO 9 - 13
SPI SCLK – PIN 23, GPIO 11 - 12
SPI CS0  – PIN 24, GPIO 8 - 15
3Volt	 - PIN 1 (upper left)
GND	 - PIN 6

we use our own CS signal via  GPIO.6 (pin 22)

*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>                // Needed for SPI port
#include <sys/ioctl.h>            // Needed for SPI port
#include <linux/spi/spidev.h>     // Needed for SPI port
#include <wiringPi.h>

#define MY_CS 6 //we use our own CS signal via  GPIO.6 (pin 22)

#define CMD0        0
#define CMD0_ARG    0x00000000
#define CMD0_CRC    0x94

#define CMD8        8
#define CMD8_ARG    0x0000001AA
#define CMD8_CRC    0x86 //(1000011 << 1)

#define CMD58       58
#define CMD58_ARG   0x00000000
#define CMD58_CRC   0x00

#define CMD55       55
#define CMD55_ARG   0x00000000
#define CMD55_CRC   0x00

#define ACMD41      41
#define ACMD41_ARG  0x40000000
#define ACMD41_CRC  0x00

#define PARAM_ERROR(X)      X & 0b01000000
#define ADDR_ERROR(X)       X & 0b00100000
#define ERASE_SEQ_ERROR(X)  X & 0b00010000
#define CRC_ERROR(X)        X & 0b00001000
#define ILLEGAL_CMD(X)      X & 0b00000100
#define ERASE_RESET(X)      X & 0b00000010
#define IN_IDLE(X)          X & 0b00000001

#define CMD_VER(X)          ((X >> 4) & 0xF0)
#define VOL_ACC(X)          (X & 0x1F)

#define VOLTAGE_ACC_27_33   0b00000001
#define VOLTAGE_ACC_LOW     0b00000010
#define VOLTAGE_ACC_RES1    0b00000100
#define VOLTAGE_ACC_RES2    0b00001000

#define POWER_UP_STATUS(X)  X & 0x40
#define CCS_VAL(X)          X & 0x40
#define VDD_2728(X)         X & 0b10000000
#define VDD_2829(X)         X & 0b00000001
#define VDD_2930(X)         X & 0b00000010
#define VDD_3031(X)         X & 0b00000100
#define VDD_3132(X)         X & 0b00001000
#define VDD_3233(X)         X & 0b00010000
#define VDD_3334(X)         X & 0b00100000
#define VDD_3435(X)         X & 0b01000000
#define VDD_3536(X)         X & 0b10000000

#define SD_SUCCESS  0
#define SD_ERROR    1

#define SD_READY    0


//global stuff
int fd;
unsigned char spi_mode;
unsigned char spi_bitsPerWord;
unsigned int spi_speed;


void SD_printR1(uint8_t res)
{
    if(res & 0b10000000)
        { printf("\tError: MSB = 1\r\n"); return; }
    if(res == 0)
        { printf("\tCard Ready\r\n"); return; }
    if(PARAM_ERROR(res))
        printf("\tParameter Error\r\n");
    if(ADDR_ERROR(res))
        printf("\tAddress Error\r\n");
    if(ERASE_SEQ_ERROR(res))
        printf("\tErase Sequence Error\r\n");
    if(CRC_ERROR(res))
        printf("\tCRC Error\r\n");
    if(ILLEGAL_CMD(res))
        printf("\tIllegal Command\r\n");
    if(ERASE_RESET(res))
        printf("\tErase Reset Error\r\n");
    if(IN_IDLE(res))
        printf("\tIn Idle State\r\n");
}

void SD_printR7(uint8_t *res)
{
    SD_printR1(res[0]);

    if(res[0] > 1) return;

    printf("\tCommand Version: %x \n ",CMD_VER(res[1]));

    printf("\tVoltage Accepted: ");
    if(VOL_ACC(res[3]) == VOLTAGE_ACC_27_33)
        printf("2.7-3.6V\r\n");
    else if(VOL_ACC(res[3]) == VOLTAGE_ACC_LOW)
        printf("LOW VOLTAGE\r\n");
    else if(VOL_ACC(res[3]) == VOLTAGE_ACC_RES1)
        printf("RESERVED\r\n");
    else if(VOL_ACC(res[3]) == VOLTAGE_ACC_RES2)
        printf("RESERVED\r\n");
    else
        printf("NOT DEFINED\r\n");

    printf("\tEcho: %x \n ",res[4]);
}

void CS_ENABLE()
{
   digitalWrite (MY_CS,0);
}
void CS_DISABLE()
{
   digitalWrite (MY_CS,1);
}

uint8_t SPI_transfer (uint8_t data)
/* Schreiben und Lesen auf SPI. Parameter:
*/

  {
	struct spi_ioc_transfer spi; /* Bibliotheksstruktur fuer Schreiben/Lesen */
	int ret;                          /* Zaehler, Returnwert */

  		//init struct
  		memset(&spi, 0, sizeof (spi));
		spi.tx_buf        = (unsigned long)(&data ); // transmit from "data"
		spi.rx_buf        = (unsigned long)(&data); // receive into "data"
		spi.len           = sizeof(data);
		spi.delay_usecs   = 0;
		spi.speed_hz      = spi_speed;
		spi.bits_per_word = spi_bitsPerWord;
		spi.cs_change     = 0;

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &spi) ;
	if(ret < 0)
    {
		perror("Fehler beim Senden/Empfangen - ioctl");
		exit(1);
    }
	return data;
  }

void SD_powerUpSeq()
{

  // make sure card is deselected
    CS_DISABLE();

    // give SD card time to power up
    usleep(10000);

    // send 80 clock cycles to synchronize
    for(uint8_t i = 0; i < 10; i++)
        SPI_transfer(0xFF);

    // deselect SD card
    CS_DISABLE();
    SPI_transfer(0xFF);

}

uint8_t SD_readRes1()
{
    uint8_t i = 0, res1;

    // keep polling until actual data received
    while((res1 = SPI_transfer(0xFF)) == 0xFF)
    {
        i++;

        // if no data received for 8 bytes, break
        if(i > 8) break;
    }

    return res1;
}

void SD_command(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    // transmit command to sd card
    SPI_transfer(cmd|0x40);

    // transmit argument
    SPI_transfer((uint8_t)(arg >> 24));
    SPI_transfer((uint8_t)(arg >> 16));
    SPI_transfer((uint8_t)(arg >> 8));
    SPI_transfer((uint8_t)(arg));

    // transmit crc
    SPI_transfer(crc|0x01);
}

void SD_readRes3_7(uint8_t *res)
{
    // read R1
    res[0] = SD_readRes1();

    // if error reading R1, return
    if(res[0] > 1) return;

    // read remaining bytes
    res[1] = SPI_transfer(0xFF);
    res[2] = SPI_transfer(0xFF);
    res[3] = SPI_transfer(0xFF);
    res[4] = SPI_transfer(0xFF);
}

void SD_readRes7(uint8_t *res)
{
    // read response 1 in R7
    res[0] = SD_readRes1();

    // if error reading R1, return
    if(res[0] > 1) return;

    // read remaining bytes
    res[1] = SPI_transfer(0xFF);
    res[2] = SPI_transfer(0xFF);
    res[3] = SPI_transfer(0xFF);
    res[4] = SPI_transfer(0xFF);
}

void SD_sendIfCond(uint8_t *res)
{
    // assert chip select
    SPI_transfer(0xFF);
    CS_ENABLE();
    SPI_transfer(0xFF);

    // send CMD8
    SD_command(CMD8, CMD8_ARG, CMD8_CRC);
    // read response
    SD_readRes7(res);

    // deassert chip select
    SPI_transfer(0xFF);
    CS_DISABLE();
    SPI_transfer(0xFF);
}

uint8_t SD_goIdleState()
{
    // assert chip select
    SPI_transfer(0xFF);
    CS_ENABLE();
    SPI_transfer(0xFF);

    // send CMD0
    SD_command(CMD0, CMD0_ARG, CMD0_CRC);

    // read response
    uint8_t res1 = SD_readRes1();

    // deassert chip select
    SPI_transfer(0xFF);
    CS_DISABLE();
    SPI_transfer(0xFF);

    return res1;
}

void SD_readOCR(uint8_t *res)
{
    // assert chip select
    SPI_transfer(0xFF);
    CS_ENABLE();
    SPI_transfer(0xFF);

    // send CMD58
    SD_command(CMD58, CMD58_ARG, CMD58_CRC);

    // read response
    SD_readRes3_7(res);

    // deassert chip select
    SPI_transfer(0xFF);
    CS_DISABLE();
    SPI_transfer(0xFF);
}

void SD_printR3(uint8_t *res)
{
    SD_printR1(res[0]);

    if(res[0] > 1) return;

    printf("\tCard Power Up Status: ");
    if(POWER_UP_STATUS(res[1]))
    {
        printf("READY\r\n");
        printf("\tCCS Status: ");
        if(CCS_VAL(res[1])){ printf("1\r\n"); }
        else printf("0\r\n");
    }
    else
    {
        printf("BUSY\r\n");
    }

    printf("\tVDD Window: ");
    if(VDD_2728(res[3])) printf("2.7-2.8, ");
    if(VDD_2829(res[2])) printf("2.8-2.9, ");
    if(VDD_2930(res[2])) printf("2.9-3.0, ");
    if(VDD_3031(res[2])) printf("3.0-3.1, ");
    if(VDD_3132(res[2])) printf("3.1-3.2, ");
    if(VDD_3233(res[2])) printf("3.2-3.3, ");
    if(VDD_3334(res[2])) printf("3.3-3.4, ");
    if(VDD_3435(res[2])) printf("3.4-3.5, ");
    if(VDD_3536(res[2])) printf("3.5-3.6");
    printf("\r\n");
}

uint8_t SD_sendApp()
{
    // assert chip select
    SPI_transfer(0xFF);
    CS_ENABLE();
    SPI_transfer(0xFF);

    // send CMD0
    SD_command(CMD55, CMD55_ARG, CMD55_CRC);

    // read response
    uint8_t res1 = SD_readRes1();

    // deassert chip select
    SPI_transfer(0xFF);
    CS_DISABLE();
    SPI_transfer(0xFF);

    return res1;
}

uint8_t SD_sendOpCond()
{
    // assert chip select
    SPI_transfer(0xFF);
    CS_ENABLE();
    SPI_transfer(0xFF);

    // send CMD0
    SD_command(ACMD41, ACMD41_ARG, ACMD41_CRC);

    // read response
    uint8_t res1 = SD_readRes1();

    // deassert chip select
    SPI_transfer(0xFF);
    CS_DISABLE();
    SPI_transfer(0xFF);

    return res1;
}

uint8_t SD_init()
{
    uint8_t res[5], cmdAttempts = 0;

    SD_powerUpSeq();

    // command card to idle
    while((res[0] = SD_goIdleState()) != 0x01)
    {
        cmdAttempts++;
        if(cmdAttempts > 10) return SD_ERROR;
    }

    printf("%d attempts for SD_goIdleState\n",cmdAttempts +1);

    // send interface conditions
    SD_sendIfCond(res);
    if(res[0] != 0x01)
    {
        return SD_ERROR;
    }

    // check echo pattern
    if(res[4] != 0xAA)
    {
        return SD_ERROR;
    }

    // attempt to initialize card
    cmdAttempts = 0;
    do
    {
        if(cmdAttempts > 100) return SD_ERROR;

        // send app cmd
        res[0] = SD_sendApp();

        // if no error in response
        if(res[0] < 2)
        {
            res[0] = SD_sendOpCond();
        }

        // wait
        usleep(10000);

        cmdAttempts++;
    }
    while(res[0] != SD_READY);

    printf("%d attempts for SD_sendOpCond\n",cmdAttempts +1);

    // read OCR
    SD_readOCR(res);

    // check card is ready
    if(!(res[1] & 0x80)) return SD_ERROR;

    return SD_SUCCESS;
}

int main(void)
{
static const char *device = "/dev/spidev0.0";
int ret;
char c = 0;

// array to hold responses
    uint8_t res[5];

 //----- SET SPI MODE -----
    //SPI_MODE_0 (0,0) 	CPOL = 0, CPHA = 0, Clock idle low, data is clocked in on rising edge, output data (change) on falling edge
    //SPI_MODE_1 (0,1) 	CPOL = 0, CPHA = 1, Clock idle low, data is clocked in on falling edge, output data (change) on rising edge
    //SPI_MODE_2 (1,0) 	CPOL = 1, CPHA = 0, Clock idle high, data is clocked in on falling edge, output data (change) on rising edge
    //SPI_MODE_3 (1,1) 	CPOL = 1, CPHA = 1, Clock idle high, data is clocked in on rising, edge output data (change) on falling edge
    spi_mode = SPI_MODE_0;
    
    //----- SET BITS PER WORD -----
    spi_bitsPerWord = 8;
    
    //----- SET SPI BUS SPEED -----
    spi_speed = 400000;

// wiringPi library
   wiringPiSetup();
//set our own CS, initially high/disable
   pinMode ( MY_CS, OUTPUT);
   digitalWrite (MY_CS,1);

/* Device oeffen */
if ((fd = open(device, O_RDWR)) < 0)
  {
  perror("Fehler Open Device");
  exit(1);
  }
/* Mode setzen */
ret = ioctl(fd, SPI_IOC_WR_MODE, &spi_mode);
if (ret < 0)
  {
  perror("Fehler Set SPI-Modus");
  exit(1);
  }
/* Mode abfragen */
ret = ioctl(fd, SPI_IOC_RD_MODE, &spi_mode);
if (ret < 0)
  {
  perror("Fehler Get SPI-Modus");
  exit(1);
  }

/* Wortlaenge setzen */
ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bitsPerWord);
if (ret < 0)
  {
  perror("Fehler Set Wortlaenge");
  exit(1);
  }

/* Wortlaenge abfragen */
ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &spi_bitsPerWord);
if (ret < 0)
  {
  perror("Fehler Get Wortlaenge");
  exit(1);
  }

/* Datenrate setzen */
ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed);
if (ret < 0)
  {
  perror("Fehler Set Speed");
  exit(1);
  }
   
/* Datenrate abfragen */
ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &spi_speed);
if (ret < 0)
  {
  perror("Fehler Get Speed");
  exit(1);
  }

/* Kontrollausgabe 
printf("SPI-Device.....: %s\n", device);
printf("SPI-Mode.......: %d\n", spi_mode);
printf("Wortlaenge.....: %d\n", spi_bitsPerWord);
printf("Geschwindigkeit: %d Hz (%d kHz)\n", spi_speed, spi_speed/1000);
*/

  // start power up sequence
    SD_powerUpSeq();
while(1)
    {
      if ( c != 0x0a) //not for nl
       {
        // print menu
        printf("\r\n\nMENU\r\n");
        printf("------------------\r\n");
        printf("0 - Send CMD0\r\n1 - Send CMD8\r\n2 - Send CMD58\r\n");
	printf("3 - Send CMD55\r\n4 - Send ACMD41\r\n");
	printf("9 - init SD card\r\n");
        printf("------------------\r\n");
       }

        // get character from user
        c = getchar();

     if(c == '0')
        {
            // command card to idle
            printf("Sending CMD0...\r\n");
            res[0] = SD_goIdleState();
            printf("Response:\r\n");
            SD_printR1(res[0]);
        }
        else if(c == '1')
        {
            // send if conditions
            printf("Sending CMD8...\r\n");
            SD_sendIfCond(res);
            printf("Response:\r\n");
            SD_printR7(res);
        }
        else if(c == '2')
        {
            // send if conditions
            printf("Sending CMD58...\r\n");
            SD_readOCR(res);
            printf("Response:\r\n");
            SD_printR3(res);
        }
        else if(c == '3')
        {
            // command card to idle
            printf("Sending CMD55...\r\n");
            res[0] = SD_sendApp();
            printf("Response:\r\n");
            SD_printR1(res[0]);
        }
        else if(c == '4')
        {
            // command card to idle
            printf("Sending ACMD41...\r\n");
            res[0] = SD_sendOpCond();
            printf("Response:\r\n");
            SD_printR1(res[0]);
        }
        else if(c == '9')
        {
            // do the complete init process
            printf("complete init process...\r\n");
            ret = SD_init();
            if (ret == SD_SUCCESS) printf("Init SUCCEDED:\r\n");
            else printf("Init FAILED:\r\n");
        }
        else
        {
            if ( c != 0x0a ) printf("Unrecognized command %x \r\n",c);
        }

    }

}
