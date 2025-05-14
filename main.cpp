#include <cstdint>
#include <cstdio>
#include <mbed.h>
#include <Serial.h>
#include "Radio.h"

#define TX PA_9
#define RX PA_10

#define LED1 PB_7
#define LED2 PB_6

#define GPIO1 PB_8
#define GPIO2 PB_9

#define MOSI           PB_15
#define MISO           PB_14
#define SCLK           PB_13
#define CS             PB_12
#define RADIO_RST      PA_8
#define RADIO_INT      PA_7
#define RADIO_MARC_INT PA_6

#define SERIAL_START_BYTE 234

volatile char cmdbuf[200] = {0};
volatile bool found = false;
void do_cobs() {
    size_t len = cmdbuf[1];
    size_t index = 2;
    for (int i = 0; i < len; i++) {
        size_t next = index + cmdbuf[index];
        cmdbuf[index] = cmdbuf[0];
        
        if (next == index)
            break;
        index = next;
    }
}

void read_commands(Serial* ser) {
    int index = 0;
    bool start_byte = false;
    while (true) {
        if (ser->readable()) {
            char c;
            ser->read(&c, 1);

            //ser->printf("\t\t> %d %c\t%d %d == %d\n", c, c, start_byte, index, cmdbuf[1]);

            if (c == SERIAL_START_BYTE) {
                cmdbuf[0] = c;
                cmdbuf[1] = 0;
                index = 1;
                start_byte = true;
            }
            else if (start_byte) {
                cmdbuf[index] = c;
                index ++;
            }
        }

        if (index == cmdbuf[1] && start_byte) {
            start_byte = false;
            cmdbuf[index] = 0;
            do_cobs();
            
            found = true;
            while (found) {
                ThisThread::sleep_for(200ms);
            }
        }
    }
}

typedef enum {
    RADIO_POWER,
    RADIO_RSSI,
    RADIO_DEBUG,
    RADIO_FREQUENCY,
    RADIO_RATE,
    RADIO_FILTER,
    RADIO_DEVIATION,
    RADIO_MODULATION,
    RADIO_APPLY,
    RADIO_HELP,
} radio_property;

struct command {
    bool valid;
    bool get;
    radio_property property;
    float value;
};

bool get_bit(uint32_t bits, uint8_t bit) {
    return (bits >> bit) & 1;
}
void toggle_bit(uint32_t* bits, uint8_t bit) {
    *bits ^= (1 << bit);
}

command parse_command(const char* buf) {
    command out;
    out.valid = false;

    if (buf[0] != SERIAL_START_BYTE)
        return out;
    
    size_t size = buf[1];
    if (size < 4)
        return out;
    
    if (buf[3] != '+')
        return out;
    
    out.get = true;
    if (buf[4] == 's' || buf[4] == 'S')
        out.get = false;

    const char power[15]      = "power";
    const char rssi[15]       = "rssi";
    const char debug[15]      = "debug";
    const char frequency[15]  = "frequency";
    const char rate[15]       = "rate";
    const char filter[15]     = "filter";
    const char deviation[15]  = "deviation";
    const char modulation[15] = "modulation";
    const char apply[15]      = "apply";
    const char help[15]       = "help";

    uint32_t valid = 0b1111111111;
    int offset = (!out.get || buf[4] == 'g' || buf[4] == 'G') ? 5 : 4;
    int i = offset;
    for (; i<size; i++) {
        if (get_bit(valid, 0) && buf[i] != power[i-offset])
            toggle_bit(&valid, 0);
        if (get_bit(valid, 1) && buf[i] != rssi[i-offset])
            toggle_bit(&valid, 1);
        if (get_bit(valid, 2) && buf[i] != debug[i-offset])
            toggle_bit(&valid, 2);
        if (get_bit(valid, 3) && buf[i] != frequency[i-offset])
            toggle_bit(&valid, 3);
        if (get_bit(valid, 4) && buf[i] != rate[i-offset])
            toggle_bit(&valid, 4);
        if (get_bit(valid, 5) && buf[i] != filter[i-offset])
            toggle_bit(&valid, 5);
        if (get_bit(valid, 6) && buf[i] != deviation[i-offset])
            toggle_bit(&valid, 6);
        if (get_bit(valid, 7) && buf[i] != modulation[i-offset])
            toggle_bit(&valid, 7);
        if (get_bit(valid, 8) && buf[i] != apply[i-offset])
            toggle_bit(&valid, 8);
        if (get_bit(valid, 9) && buf[i] != help[i-offset])
            toggle_bit(&valid, 9);


        float res = log2f((float)valid);
        //std::cout << buf[i] << " " << valid << " " << res << "\n";
        if ((int)res == res) {
            if (valid != 0) {
                out.valid = true;
                out.property = static_cast<radio_property>(res);
            }
            break;
        }
    }
    
    // if setting, exaust paramater name
    if (!out.get) {
        for (; i<size; i++) {
            if (get_bit(valid, 0) && buf[i] != power[i-offset])
                toggle_bit(&valid, 0);
            if (get_bit(valid, 1) && buf[i] != rssi[i-offset])
                toggle_bit(&valid, 1);
            if (get_bit(valid, 2) && buf[i] != debug[i-offset])
                toggle_bit(&valid, 2);
            if (get_bit(valid, 3) && buf[i] != frequency[i-offset])
                toggle_bit(&valid, 3);
            if (get_bit(valid, 4) && buf[i] != rate[i-offset])
                toggle_bit(&valid, 4);
            if (get_bit(valid, 5) && buf[i] != filter[i-offset])
                toggle_bit(&valid, 5);
            if (get_bit(valid, 6) && buf[i] != deviation[i-offset])
                toggle_bit(&valid, 6);
            if (get_bit(valid, 7) && buf[i] != modulation[i-offset])
                toggle_bit(&valid, 7);
            if (get_bit(valid, 8) && buf[i] != apply[i-offset])
                toggle_bit(&valid, 8);
            if (get_bit(valid, 9) && buf[i] != help[i-offset])
                toggle_bit(&valid, 9);
                
            if (valid == 0)
                break;
        }
    }
    
    sscanf(&buf[i], "%f", &out.value);
    
    return out;
}

int main() {
    Serial ser(TX, RX, 115200);
    Radio radio(MOSI, MISO, SCLK, CS, RADIO_RST, RADIO_INT, RADIO_MARC_INT, &ser);

    DigitalOut l1(LED1);
    DigitalOut l2(LED2);

    l1.write(1);
    l2.write(0);

    radio.set_debug(true);
    bool exists = radio.checkExistance();
    bool init = radio.init();
    radio.setup_434();
    radio.update();

    float f = radio.get_frequency();
    float p = radio.get_power();
    
    ser.printf("Device Found: %s\nDevice Init: %s\nFrequency: %f\nPower: %f\n",
        (exists ? "Yes" : "No"),
        (init   ? "Yes" : "No"),
        f, p
    );


    Timer t;
    t.start();
    Thread th;
    th.start(callback(read_commands, &ser));
    while (true) {
        if (found) {
            t.reset();
            
            //ser.printf("%s\n", &cmdbuf[3]);
            //for (int i = 3; i<cmdbuf[1]; i++) {
            //    ser.printf("\t%d| %d %c\n", i, cmdbuf[i], cmdbuf[i]);
            //}

            if (cmdbuf[3] == '+') {
                command cmd = parse_command((const char*)cmdbuf); // should be safe since we are essentially using 'found' as a mutex

                if (!cmd.valid)
                    ser.printf("Error | Invalid Command\n");
                else {
                    switch (cmd.property) {
                        case RADIO_POWER: {
                            if (cmd.get)
                                ser.printf("Power: %f\n", radio.get_power());
                            else {
                                if (radio.set_power(cmd.value))
                                    ser.printf("Set Power: %f\n", cmd.value);
                                else
                                    ser.printf("Error: %f\n", cmd.value);
                            }    
                            break;
                        }
                        case RADIO_RSSI: {
                            if (cmd.get)
                                ser.printf("Rssi: %f\n", radio.get_rssi());
                            else {
                                ser.printf("Error\n");
                            }
                            break;
                        }
                        case RADIO_DEBUG: {
                            if (cmd.get)
                                ser.printf("Debug: %s\n", (radio.get_debug() ? "True" : "False"));
                            else {
                                radio.set_debug(cmd.value > 0);
                                ser.printf("Set Debug: %s\n", (cmd.value > 0 ? "True" : "False"));
                            }
                            break;    
                        }
                        case RADIO_FREQUENCY: {
                            if (cmd.get)
                                ser.printf("Frequency: %f\n", radio.get_frequency());
                            else {
                                if (radio.set_frequency(cmd.value))
                                    ser.printf("Set Frequency: %f\n", cmd.value);
                                else
                                    ser.printf("Error: %f\n", cmd.value);
                            }    
                            break;
                        }
                        case RADIO_RATE: {
                            if (cmd.get)
                                ser.printf("Symbol Rate: %f\n", radio.get_symbol_rate());
                            else {
                                if (radio.set_symbol_rate(cmd.value))
                                    ser.printf("Set Symbol Rate: %f\n", cmd.value);
                                else
                                    ser.printf("Error: %f\n", cmd.value);
                            }    
                            break;
                        }
                        case RADIO_FILTER: {
                            if (cmd.get)
                                ser.printf("RX Filter: %f\n", radio.get_rx_filter());
                            else {
                                if (radio.set_rx_filter(cmd.value))
                                    ser.printf("Set RX Filter: %f\n", cmd.value);
                                else
                                    ser.printf("Error: %f\n", cmd.value);
                            }    
                            break;
                        }
                        case RADIO_DEVIATION: {
                            if (cmd.get)
                                ser.printf("Deviation: %f\n", radio.get_deviation());
                            else {
                                if (radio.set_deviation(cmd.value))
                                    ser.printf("Set Deviation: %f\n", cmd.value);
                                else
                                    ser.printf("Error: %f\n", cmd.value);
                            }    
                            break;
                        }
                        case RADIO_MODULATION: {
                            if (cmd.get)
                                switch (radio.get_modulation()) {
                                    case CC1200::ModFormat::FSK_2: {
                                        ser.printf("Modulation: FSK_2\n");
                                        break;
                                    }
                                    case CC1200::ModFormat::GFSK_2: {
                                        ser.printf("Modulation: GFSK_2\n");
                                        break;
                                    }
                                    case CC1200::ModFormat::ASK: {
                                        ser.printf("Modulation: ASK\n");
                                        break;
                                    }
                                    case CC1200::ModFormat::FSK_4: {
                                        ser.printf("Modulation: FSK_4\n");
                                        break;
                                    }
                                    case CC1200::ModFormat::GFSK_4: {
                                        ser.printf("Modulation: GFSK_4\n");
                                        break;
                                    }
                                }
                            else {
                                CC1200::ModFormat mod = static_cast<CC1200::ModFormat>((int)cmd.value);
                                if (radio.set_modulation(mod)) {
                                    ser.printf("Set ");
                                    switch (mod) {
                                        case CC1200::ModFormat::FSK_2: {
                                            ser.printf("Modulation: FSK_2\n");
                                            break;
                                        }
                                        case CC1200::ModFormat::GFSK_2: {
                                            ser.printf("Modulation: GFSK_2\n");
                                            break;
                                        }
                                        case CC1200::ModFormat::ASK: {
                                            ser.printf("Modulation: ASK\n");
                                            break;
                                        }
                                        case CC1200::ModFormat::FSK_4: {
                                            ser.printf("Modulation: FSK_4\n");
                                            break;
                                        }
                                        case CC1200::ModFormat::GFSK_4: {
                                            ser.printf("Modulation: GFSK_4\n");
                                            break;
                                        }
                                    }
                                }
                                else
                                    ser.printf("Error: %f\n", cmd.value);
                            }    
                            break;
                        }
                        case RADIO_APPLY: {
                            radio.init();
                            radio.update();
                            ser.printf("Settings Applied\n");
                            break;
                        }
                        case RADIO_HELP: {
                            ser.printf("power      | (g/s) float\n");
                            ser.printf("rssi       | (g)   float\n");
                            ser.printf("debug      | (g/s) bool\n");
                            ser.printf("frequency  | (g/s) float\n");
                            ser.printf("rate       | (g/s) float\n");
                            ser.printf("filter     | (g/s) float\n");
                            ser.printf("deviation  | (g/s) float\n");
                            ser.printf("modulation | (g/s) enum\n");
                            ser.printf("\t 0: 2FSK\n");
                            ser.printf("\t 1: 2GFSK\n");
                            ser.printf("\t 3: ASK\n");
                            ser.printf("\t 4: 4FSK\n");
                            ser.printf("\t 0: 4GFSK\n");
                            ser.printf("apply      | (g/s)\n");
                            ser.printf("help       | (g)   text\n");
                            break;
                        }
                    }
            
                }
            }
            else {
                char* _temp = new char[cmdbuf[1]-3];
                memcpy(_temp, (char*)&cmdbuf[3], cmdbuf[1]-3); // WARNING: unsafe, ensure cmdbuf is locked for operation

                bool res = radio.transmit(_temp, cmdbuf[1]-3);
                ser.printf("Transmit: %s\n", res ? "Success" : "Fail");

                delete[] _temp;
                
                ser.printf("Transmit Time: %d ms\n", t.read_ms());
            }

            found = false;
        }

        t.reset();
        size_t len = 0;
        char* msg = radio.recieve(&len);

        if (len > 0) {
            ser.printf("Recieve Time: %d ms\n", t.read_ms());
            ser.printf("\"%s\"\n", msg);
            for (int i = 0; i<len; i++) {
                ser.printf("\t%d| %d %c\n", i, msg[i], msg[i]);
            }
            ser.printf("RSSI: %d\n", radio.get_rssi());
        }

        delete[] msg;

        



    }
}
