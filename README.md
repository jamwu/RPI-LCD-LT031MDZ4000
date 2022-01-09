# RPI-LCD-LT031MDZ4000
Raspberry Pi panel driver for JDI LT031MDZ4000 720x720 display

# LCD Adapter board 
![Adapter](https://raw.githubusercontent.com/jamwu/RPI-LCD-LT031MDZ4000/main/images/result.jpg)

The adapter board is design for an A64 development board.
You can refer to the sch below to design a specific adapter for your raspberry pi.

# Get kernel source
Refer to [rpi-source](https://github.com/RPi-Distro/rpi-source)


# Build & Install
```
git clone https://github.com/jamwu/RPI-LCD-LT031MDZ4000.git
cd RPI-LCD-LT031MDZ4000
make
make install
```
Then edit /boot/config.txt to add:
```
lcd_ignore=1
dtoverlay=vc4-kms-dsi-lt031mdz4000
```
and reboot

LCD pins defaults to:
- Reset - GPIO26
- BL_Enable - GPIO19
- PWM - GND

Default pins can be changed, for example:
```
#change Reset to GPIO5,BL_Enable to GPIO6
dtoverlay=vc4-kms-dsi-lt031mdz4000,bl_en=5,reset=6

```

# Hardware

 Datasheet - [LT031MDZ4000-Datasheet](https://wenku.baidu.com/view/c4c5a559680203d8ce2f24f7.html)

Proof-of-concept schematics - [SCH_LT031MDZ4000_Adapter_QiHua_X64](https://raw.githubusercontent.com/jamwu/RPI-LCD-LT031MDZ4000/main/SCH_LT031MDZ4000_Adapter_QiHua_X64.pdf)

Wiring
![Wiring](https://raw.githubusercontent.com/jamwu/RPI-LCD-LT031MDZ4000/main/images/wiring.jpg)


Important notes:
- This schematics comes with no warranties, use at your own risk

