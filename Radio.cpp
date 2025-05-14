#include <cinttypes>
#include <string>
#include "Radio.h"

Radio::Radio(PinName Mosi, PinName Miso, PinName CLK, PinName CS, PinName Reset, PinName INT, PinName MarcINT, Serial* ser) : radio(Mosi, Miso, CLK, CS, Reset, ser) {
    this->ser = ser;
    this->max_msg_size = 100;
    this->debug = false;
    this->frequency = -1;
    this->power = 0;

    radio.setOnReceiveState(CC1200::State::RX, CC1200::State::RX);
    radio.setOnTransmitState(CC1200::State::RX);
}

Radio::~Radio(){
}

bool Radio::checkExistance() {
    RADIO_DEBUG("\n\nChecking for radio...\n");
    return radio.begin();
}

bool Radio::init() {
    return this->radio.begin();
}

void Radio::setup_434() {
    this->frequency = 144e6;
    this->power = 14;
    this->deviation = 20000;
    this->symbol_rate = 38400;
    this->filter_bandwith = 104166;
    this->modulation = CC1200::ModFormat::GFSK_2;
}

void Radio::update() {
    const CC1200::PacketMode mode = CC1200::PacketMode::VARIABLE_LENGTH;

    CC1200::Band band;
    if (frequency >= 136e6 && frequency <= 160e6)
        band = CC1200::Band::BAND_136_160MHz;
    else if (frequency >= 164e6 && frequency <= 192e6)
        band = CC1200::Band::BAND_164_192MHz;
    else if (frequency >= 205e6 && frequency <= 240e6)
        band = CC1200::Band::BAND_205_240MHz;
    else if (frequency >= 273e6 && frequency <= 320e6)
        band = CC1200::Band::BAND_273_320MHz;
    else if (frequency >= 410e6 && frequency <= 480e6)
        band = CC1200::Band::BAND_410_480MHz;
    else if (frequency >= 820e6 && frequency <= 960e6)
        band = CC1200::Band::BAND_820_960MHz;
    else if (debug) {
        RADIO_DEBUG("ERROR | Out of Band Frequency\n");
    }


	const CC1200::IFCfg ifCfg = CC1200::IFCfg::POSITIVE_DIV_8;
	const bool imageCompEnabled = true;
	const bool dcOffsetCorrEnabled = true;
	const uint8_t agcRefLevel = 0x2A;

	const uint8_t dcFiltSettingCfg = 1; // default chip setting
	const uint8_t dcFiltCutoffCfg = 4; // default chip setting
    const uint8_t agcSettleWaitCfg = 2;

	const CC1200::SyncMode syncMode = CC1200::SyncMode::SYNC_32_BITS;
	const uint32_t syncWord = 0x930B51DE; // default sync word
	const uint8_t syncThreshold = 8; // TI seems to recommend this threshold for most configs above 100ksps

	const uint8_t preableLengthCfg = 5; // default chip setting
	const uint8_t preambleFormatCfg = 0; // default chip setting


    radio.configureFIFOMode();
    radio.setPacketMode(mode, true);
    
    radio.setModulationFormat(this->modulation);
    radio.setFSKDeviation(this->deviation);
    radio.setSymbolRate(this->symbol_rate);
    radio.setRXFilterBandwidth(this->filter_bandwith);
    radio.setOutputPower(this->power);
    radio.setRadioFrequency(band, frequency);
    
    radio.setIFCfg(ifCfg, imageCompEnabled);
    radio.configureDCFilter(dcOffsetCorrEnabled, dcFiltSettingCfg, dcFiltCutoffCfg);
    radio.configureSyncWord(syncWord, syncMode, syncThreshold);
    radio.configurePreamble(preableLengthCfg, preambleFormatCfg);

    // AGC configuration (straight from SmartRF)
    radio.setAGCReferenceLevel(agcRefLevel);
    radio.setAGCSyncBehavior(CC1200::SyncBehavior::FREEZE_NONE);
    radio.setAGCSettleWait(agcSettleWaitCfg);


    if(ifCfg == CC1200::IFCfg::ZERO)
    {
        radio.setAGCGainTable(CC1200::GainTable::ZERO_IF, 11, 0);
    }
    else
    {
        // enable all AGC steps for NORMAL mode
        radio.setAGCGainTable(CC1200::GainTable::NORMAL, 17, 0);
    }
    radio.setAGCHysteresis(0b10);
    radio.setAGCSlewRate(0);
}

int Radio::get_rssi() {
    return radio.getLastRSSI();
}

float Radio::get_frequency() {
    return this->frequency;
}


float Radio::get_power() {
    return this->power;
}

float Radio::get_deviation() {
    return this->deviation;
}
float Radio::get_symbol_rate() {
    return this->symbol_rate;
}
float Radio::get_rx_filter() {
    return this->filter_bandwith;
}
CC1200::ModFormat Radio::get_modulation() {
    return this->modulation;
}

bool Radio::transmit(const char* message, size_t len) {
    if (debug) {
        RADIO_DEBUG("\n---------------------------------\n");

        RADIO_DEBUG("SENDING: \"%s\"\n", message);
    }

    bool success = radio.enqueuePacket(message, len);
    radio.startTX();

    // if packet successfully enqueued wait for TXFIFO to empty (packet has transmitted) before continuing
    if (success) {
        success = false;
        for (int i = 0; i<2000; i++) {
            if (radio.getTXFIFOLen() == 0) {
                success = true;
                break;
            }

            ThisThread::sleep_for(1ms);

            if (i % 15 == 0) {
                if (static_cast<uint8_t>(radio.getState()) != 0x2)
                    radio.startTX();
            }
        }
    }
    radio.startRX();

    if (debug) {
        RADIO_DEBUG("sent: %s\n", (success ? "true" : "false")); 

        // update TX radio
        RADIO_DEBUG("TX radio: state = 0x%" PRIx8 ", TX FIFO len = %zu, FS lock = 0x%u\n",
                    static_cast<uint8_t>(radio.getState()), radio.getTXFIFOLen(), radio.readRegister(CC1200::ExtRegister::FSCAL_CTRL) & 1);
    }

    return success;

}

char* Radio::recieve(size_t* len) {
    size_t message_size;
    char receiveBuffer[this->max_msg_size];

    switch (radio.getState()) {
        case CC1200::State::RX_FIFO_ERROR: {
            radio.receivePacket(receiveBuffer, this->max_msg_size);
            radio.idle();
            
            *len = 0;
            return new char[0];

            break;
        }
        case CC1200::State::RX: {
            break;
        }
        default: {
            radio.startRX();
        }
    }

    bool hp = radio.hasReceivedPacket();


    if (debug && radio.getRXFIFOLen() > 0) {
        RADIO_DEBUG("\n---------------------------------\n");

        RADIO_DEBUG("RX radio: state = 0x%" PRIx8 ", RX FIFO len = %zu | hasPacket: %d\n",
                    static_cast<uint8_t>(radio.getState()), radio.getRXFIFOLen(), hp);
    }


    if (hp) {
        size_t bytes = radio.receivePacket(receiveBuffer, this->max_msg_size);
        //radio.idle();

        if (bytes > max_msg_size) {
            if (debug) {
                RADIO_DEBUG("recieved %d bytes, max size is %d ... discarding", bytes, this->max_msg_size);
            }

            *len = 0;
            return new char[0];
        }

        char* out = new char[bytes];

        for (int i = 0; i<bytes; i++) {
            out[i] = receiveBuffer[i];
        }
        
        
        *len = bytes;
        return out;
    }

    *len = 0;
    return new char[0];
}


void Radio::set_debug(bool debug) {
    this->debug = debug;

    if (debug) {
        if (ser == nullptr)
            ser = hold;
    }
    else {
        if (ser != nullptr) {
            hold = ser;
            ser = nullptr;
        }
    }
}
bool Radio::get_debug() {
    return this->debug;
}


bool Radio::set_frequency(float frequency) {
    if (frequency >= 820e6 && frequency <= 960e6) {
    }
    else if (frequency >= 410e6 && frequency <= 480e6) {
    }
    else if (frequency >= 273.3e6 && frequency <= 320e6) {
    }
    else if (frequency >= 205e6 && frequency <= 240e6) {
    }
    else if (frequency >= 164e6 && frequency <= 192e6) {
    }
    else if (frequency >= 136.7e6 && frequency <= 160e6) {
    }
    else {
        return false;
    }

    this->frequency = frequency;
    return true;
}

bool Radio::set_power(float power) {
    if (power < -16 || power > 14)
        return false;

    this->power = power;

    return true;
}


bool Radio::set_deviation(float deviation) {
    this->deviation = deviation;
    return true;
}
bool Radio::set_symbol_rate(float symbol_rate) {
    this->symbol_rate = symbol_rate;
    return true;
}
bool Radio::set_rx_filter(float filter_bandwith) {
    this->filter_bandwith = filter_bandwith;
    return true;
}

bool Radio::set_modulation(CC1200::ModFormat mod) {
    this->modulation = mod;
    return true;
}
