#include <mbed.h>
#include "SerialStream.h"
#include "Radio.h"
#include <cinttypes>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>


#include <USBSerial.h>



#define ADDR_PIN PC_3
#define SERIAL_PIN PC_14

#define S_LED_1 PA_2

#define ADDR 0x34
#define BADDR 0x35

#define VID 0x3A3A
#define PID 0x0001

#define SDA PB_7
#define SCL PB_6

void I2C_MODE() {
    DigitalOut led1(S_LED_1);
    led1.write(1);

    USBSerial pc(false, VID, PID);  
    //SerialStream<serial> pc(serial);

    DigitalIn addrPin(ADDR_PIN);
    addrPin.mode(PullDown);

    

    Radio radio(&pc);
    radio.init();
    radio.setup_443();

    radio.set_debug(true);

    I2CSlave slave(SDA, SCL);
    
    if (!addrPin.read())
        slave.address(ADDR << 1); //keep actual addresss
    else
        slave.address(BADDR << 1);


    pc.printf("I2C confiigured !!\n");
    if (addrPin.read() == 1) {
        pc.printf("using backup address: 0x%X\n", BADDR);
    }
    else {
        pc.printf("using regular address: 0x%X\n", ADDR);
    }

    const char msg[] = "ArmLabCC1200";

    enum State {
        Idle,
        Transmit,
        Recieve_len,
        Recieve,
    };

    State state = Idle;
    int arg = 0;

    size_t len;
    char* packet;

    while (1) {
        if (state == Idle) {
            led1.write(0);
        }
        else {
            led1.write(1);
        }
        int i = slave.receive();
        switch (i) {
            case I2CSlave::ReadAddressed: {
                if (state == Idle) {
                    int res = slave.write(msg, strlen(msg) + 1); // Includes null char
                    pc.printf("wrote msg | res: %d\n", res);
                }

                else if (state == Recieve_len) {
                    slave.write(len);


                    if (len == 0) {
                        state = Idle;
                        pc.printf("no packet, skipping Recieve phase\n");
                        pc.printf("mode: Idle\n");
                    }
                    else {
                        state = Recieve;
                        pc.printf("mode: Recieve\n");
                    }
                }

                else if (state == Recieve) {
                    slave.write(packet, len);
                    delete[] packet;
                    state = Idle;
                    pc.printf("mode: Idle\n");
                }

                break;
            }
            case I2CSlave::WriteAddressed: {
                if (state == Idle) {
                    char buf[5];
                    slave.read(buf, 5);

                    pc.printf("cmd: %d, args: %d %d %d %d\n", buf[0], buf[1], buf[2], buf[3], buf[4]);

                    switch (buf[0]) {
                        case 1:
                            state = Transmit;
                            arg = buf[1];
                            pc.printf("mode: Transmit, expecting %d bytes\n", arg);
                            break;
                        case 2:
                            state = Recieve_len;
                            pc.printf("collecting packet\n");
                            //packet = "test test msg";
                            packet = radio.recieve(&len);

                            pc.printf("mode: Recieve_len, waiting for read\n");
                            break;

                        /*radio.setModulationFormat(modFormat);
                        radio.setFSKDeviation(fskDeviation);
                        radio.setSymbolRate(symbolRate);
                        radio.setRXFilterBandwidth(rxFilterBW);*/
                        case 3:
                        case 4:
                        case 5:
                        case 6:
                        case 7: {
                            float val;
                            memcpy(&val, &buf[1], 4);

                            string log = "";

                            switch (buf[0]) {
                                case 3:
                                    log = "frequency";
                                    radio.set_frequency(val);
                                    break;
                                case 4:
                                    log = "power";
                                    radio.set_power(val);
                                    break;
                                case 5:
                                    log = "deviation";
                                    radio.set_deviation(val);
                                    break;
                                case 6:
                                    log = "symbol rate";
                                    radio.set_rx_filter(val);
                                    break;
                                case 7:
                                    log = "rx fsk deviation";
                                    radio.set_rx_filter(val);
                                    break;
                            }

                            pc.printf("setting %s to %f\n", log.c_str(), val);
                            break;
                        }
                        case 8:
                            radio.set_modulation(buf[1]);
                            pc.printf("setting modulation to %d", buf[1]);
                            break;
                        case 9: {
                            bool res = radio.init();
                            pc.printf("reset CC1200, detected = %s\n", res ? "true" : "false");
                            break;
                        }
                        case 10: {
                            pc.printf("attempting soft STM32 Reset");
                            NVIC_SystemReset();
                        }
                        case 0:
                        default:
                            state = Idle;
                            break;
                    }
                }
                else if (state == Transmit) {
                    char data[arg + 1];
                    slave.read(data, arg);
    
                    data[arg] = 0;
                    pc.printf("sending: %s\n", data);
                    radio.transmit(data, arg+1);

                    state = Idle;
                    pc.printf("Idle\n");
                }
                
                
                break;
            }
        }
    }
}

void SERIAL_MODE() {
    DigitalOut led1(S_LED_1);
    led1.write(1);

    USBSerial pc(false, VID, PID);

    BufferedSerial dummy(PA_9, PA_10, 9600);
    SerialStream<BufferedSerial> dummyPC(dummy);

    Radio radio(&dummyPC);
    radio.init();
    radio.setup_443();

    radio.set_debug(false);

    while (1) {
        pc.sync();
        led1.write(0);
        //pc.printf("idle\n");

        // get command
        char buf[5] = {0,0,0,0,0};
        //pc.scanf("%5s", buf);
        for (size_t i = 0; i<5; i++) {
            buf[i] = pc.getc();
        }
        pc.getc(); //flush trailing newline / return char
        
        led1.write(1);
        //pc.printf("got cmd: 0x%X 0x%X 0x%X 0x%X 0x%X\n", buf[0], buf[1], buf[2], buf[3], buf[4]);

        switch (buf[0]) {
            case 1: {
                int size = buf[1];

                char msg[size];            

                for (int i = 0; i<size; i++) {
                    msg[i] = pc.getc();
                }
                pc.getc(); //flush trailing newline / return char

                radio.transmit(msg, size);
                //pc.printf("%d, %s\n", buf[1], msg);
                break;
            }
            case 2: {
                size_t len = 0;
                char* packet = radio.recieve(&len);

                //pc.printf("%c",(char)len);
                pc.putc((char) len);

                if (len == 0) {
                    continue;
                }

                for (int i = 0; i<len; i++) {
                    pc.putc(packet[i]);
                }

                break;
            }
            case 3:
            case 4:
            case 5:
            case 6:
            case 7: {
                float val;
                memcpy(&val, &buf[1], 4);

                string log = "";

                switch (buf[0]) {
                    case 3:
                        radio.set_frequency(val);
                        break;
                    case 4:
                        radio.set_power(val);
                        break;
                    case 5:
                        radio.set_deviation(val);
                        break;
                    case 6:
                        radio.set_rx_filter(val);
                        break;
                    case 7:
                        radio.set_rx_filter(val);
                        break;
                }

            }
            case 8:
                radio.set_modulation(buf[1]);
                break;
            case 9: {
                radio.init();
                break;
            }
            case 10: {
                NVIC_SystemReset();
            }
            case 0:
            default: {
                char msg[13] = "ArmLabCC1200";
                pc.write(msg, 12);
            }
        }
    }
}


int main()
{
    DigitalIn serPin(SERIAL_PIN);

    serPin.mode(PullDown);

    if (serPin.read() == 1) {
        SERIAL_MODE();
    }

    I2C_MODE();
}
