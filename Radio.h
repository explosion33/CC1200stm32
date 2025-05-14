#include <mbed.h>
#include <string>
#include "CC1200.h"
#include "Serial.h"

#define RADIO_DEBUG(...) {if (ser != nullptr) {ser->debug("RADIO| ", __VA_ARGS__);}}



class Radio{
    public:

        Radio(PinName Mosi, PinName Miso, PinName CLK, PinName CS, PinName Reset, PinName INT, PinName MarcINT, Serial* ser = nullptr);
        ~Radio();
        bool checkExistance();
        bool init();
        void setup_434();
        void update();

        
       
        
        void checkSignalTransmit();
        bool transmit(const char* message, size_t len);
        char* recieve(size_t* len);

        bool set_frequency(float frequency);
        bool set_power(float power);
        bool set_deviation(float deviation);
        bool set_symbol_rate(float symbol_rate);
        bool set_rx_filter(float filter_bandwith);
        bool set_modulation(CC1200::ModFormat);
        
        int get_rssi();
        float get_frequency();
        float get_power();
        float get_deviation();
        float get_symbol_rate();
        float get_rx_filter();
        CC1200::ModFormat get_modulation();
        
        /*radio.setModulationFormat(modFormat);
                        radio.setFSKDeviation(fskDeviation);
                        radio.setSymbolRate(symbolRate);
                        radio.setRXFilterBandwidth(rxFilterBW);*/

        void set_debug(bool debug);
        bool get_debug();
        //State checkState(return state; );

        enum mode {
            MASTER,
            SLAVE,
        };

    private:

        Serial* ser;
        Serial* hold;
        CC1200 radio;
        size_t max_msg_size;
        bool debug = false;

        float frequency;
        float power;
        float deviation;
        float symbol_rate;
        float filter_bandwith;
        CC1200::ModFormat modulation;

};