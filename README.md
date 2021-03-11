# Raspi_SD_card_tester
a low level SD card tester with Raspberry and SPI protocol

wiring
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
	
Raspberry
SPI MOSI – PIN 19, GPIO 10 - 14
SPI MISO – PIN 21, GPIO 9 - 13
SPI SCLK – PIN 23, GPIO 11 - 12
SPI CS0  – PIN 22, GPIO 6 - 15
3Volt	 - PIN 1 (upper left)
GND	 - PIN 6


![grafik](https://user-images.githubusercontent.com/50822018/110833322-96b6bf00-829c-11eb-99c4-ec1b4422e8ef.png)
