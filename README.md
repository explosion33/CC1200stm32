# CC1200stm32
Code for the STM32 operating in tandum with ArmLabCC1200 radios

## Credit
Huge thanks to Jamie Smith, who wrote the [CC1200 interface](https://os.mbed.com/users/MultipleMonomials/code/CC1200/) used in this project

## Purpose
This project aims to expand on the interface Jamie wrote by providing I2c, SPI, and Serial interfaces, using the STM32 as a middle-man between the CC1200 and any other device

This repository abstracts the CC1200 radio operations to external devices using the following [libraries](https://github.com/explosion33/ArmLabCC1200)

## Platform
This code is designed to work with STM32 devices running with [Mbed os](https://os.mbed.com/mbed-os/)

It has currently been tested with
* the Nucleo L476RG, STM32 breakout board, and running on [mbed os 6.0.0](https://github.com/ARMmbed/mbed-os/releases/tag/mbed-os-6.0.0)
* Custom STM32 L476RG based board, an example of which can be found [here](https://github.com/explosion33/ArmLabRadio_PCB)



## Features
### Implemented
* I2C abstraction for the majority of functions
* Serial abstraction for the same functions as I2C

### Coming
* SPI abstraction
* more features / commands
