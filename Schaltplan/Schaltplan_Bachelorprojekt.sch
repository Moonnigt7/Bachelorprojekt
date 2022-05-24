EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Switch:SW_SPST Schiebeschalter
U 1 1 628DDCFF
P 2500 2050
F 0 "Schiebeschalter" H 2500 2285 50  0000 C CNN
F 1 " " H 2500 2194 50  0000 C CNN
F 2 "" H 2500 2050 50  0001 C CNN
F 3 "~" H 2500 2050 50  0001 C CNN
	1    2500 2050
	1    0    0    -1  
$EndComp
$Comp
L Switch:SW_Push Button3
U 1 1 628E0510
P 4500 4300
F 0 "Button3" H 4500 4585 50  0000 C CNN
F 1 "SW_Push" H 4500 4494 50  0000 C CNN
F 2 "" H 4500 4500 50  0001 C CNN
F 3 "~" H 4500 4500 50  0001 C CNN
	1    4500 4300
	1    0    0    -1  
$EndComp
$Comp
L Switch:SW_Push Button2
U 1 1 628E031B
P 4500 3800
F 0 "Button2" H 4500 4085 50  0000 C CNN
F 1 "SW_Push" H 4500 3994 50  0000 C CNN
F 2 "" H 4500 4000 50  0001 C CNN
F 3 "~" H 4500 4000 50  0001 C CNN
	1    4500 3800
	1    0    0    -1  
$EndComp
$Comp
L Switch:SW_Push Button1
U 1 1 628DF0A2
P 4500 3300
F 0 "Button1" H 4500 3585 50  0000 C CNN
F 1 "SW_Push" H 4500 3494 50  0000 C CNN
F 2 "" H 4500 3500 50  0001 C CNN
F 3 "~" H 4500 3500 50  0001 C CNN
	1    4500 3300
	1    0    0    -1  
$EndComp
$Comp
L Device:Buzzer PiezoBuzzer
U 1 1 628FC92F
P 4400 2800
F 0 "PiezoBuzzer" H 4552 2829 50  0000 L CNN
F 1 "Buzzer" H 4552 2738 50  0000 L CNN
F 2 "" V 4375 2900 50  0001 C CNN
F 3 "~" V 4375 2900 50  0001 C CNN
	1    4400 2800
	1    0    0    -1  
$EndComp
Wire Wire Line
	3950 2700 4300 2700
Wire Wire Line
	4300 2900 4300 2950
Wire Wire Line
	4300 2950 4700 2950
Wire Wire Line
	4700 2950 4700 3300
Wire Wire Line
	4700 3300 4700 3800
Connection ~ 4700 3300
Wire Wire Line
	4700 4300 4700 3800
Connection ~ 4700 3800
Wire Wire Line
	4700 4300 4700 4900
Connection ~ 4700 4300
Wire Wire Line
	2700 2050 3350 2050
Wire Wire Line
	3350 2050 3350 2100
Wire Wire Line
	4300 3300 4150 3300
Wire Wire Line
	4150 3300 4150 3500
Wire Wire Line
	4150 3500 3950 3500
Wire Wire Line
	4300 3800 3950 3800
Wire Wire Line
	4300 4300 4100 4300
Wire Wire Line
	4100 4300 4100 3900
Wire Wire Line
	4100 3900 3950 3900
Connection ~ 3350 4900
Wire Wire Line
	4700 4900 3350 4900
Wire Wire Line
	2150 2950 2150 4900
Wire Wire Line
	2150 2050 2150 2650
$Comp
L Device:Battery_Cell 3.7V
U 1 1 628DB01D
P 2150 2850
F 0 "3.7V" H 2268 2946 50  0000 L CNN
F 1 "Li-Ion battery" H 2268 2855 50  0000 L CNN
F 2 "" V 2150 2910 50  0001 C CNN
F 3 "~" V 2150 2910 50  0001 C CNN
	1    2150 2850
	1    0    0    -1  
$EndComp
$Comp
L RF_Module:ESP32-WROOM-32U VCC
U 1 1 628DAD34
P 3350 3500
F 0 "VCC" H 3350 5081 50  0000 C CNN
F 1 "Sparkfun ESP32 Thing" H 3350 4990 50  0000 C CNN
F 2 "RF_Module:ESP32-WROOM-32U" H 3350 2000 50  0001 C CNN
F 3 "https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32d_esp32-wroom-32u_datasheet_en.pdf" H 3050 3550 50  0001 C CNN
	1    3350 3500
	1    0    0    -1  
$EndComp
Wire Wire Line
	2150 2050 2300 2050
Wire Wire Line
	2150 4900 3350 4900
$EndSCHEMATC
