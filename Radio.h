#include <mbed.h>
#include <string>
#include "SerialStream.h"
#include "CC1200.h"


#include <Stream.h>

#define PIN_SPI_MOSI PB_15  //PC_7
#define PIN_SPI_MISO PB_14  //PA_6
#define PIN_SPI_SCLK PB_13  //PA_5
#define PIN_CS PB_12  //PA_4
#define PIN_RST PA_8 //PA_5

class Radio{
    
    public:

        Radio(Stream * pc);
        ~Radio();
        bool checkExistance();
        bool init();
        void setup_443();
        float get_frequency();
       
        float get_power();
        void checkSignalTransmit();
        void transmit(const char* message, size_t len);
        bool hasPacket();
        char* recieve(size_t* len);

        bool set_frequency(float frequency);
        bool set_power(float power);
        bool set_deviation(float deviation);
        bool set_symbol_rate(float symbol_rate);
        bool set_rx_filter(float filter_bandwith);

        bool set_modulation(char modulation);
        
        /*radio.setModulationFormat(modFormat);
                        radio.setFSKDeviation(fskDeviation);
                        radio.setSymbolRate(symbolRate);
                        radio.setRXFilterBandwidth(rxFilterBW);*/

        void set_debug(bool debug);
        //State checkState(return state; );

        enum mode {
            MASTER,
            SLAVE,
        };

    private:

        Stream * pc;
        CC1200 radio;
        size_t max_msg_size;
        bool debug = false;

        float frequency;
        float power;
        float deviation;
        float symbol_rate;
        float filter_bandwith;

};