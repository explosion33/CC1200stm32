#include <mbed.h>
#include "SerialStream.h"
#include "Radio.h"
#include <cinttypes>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>



#define ADDR_PIN PC_3
#define SPI_PIN PC_2
#define SERIAL_PIN PC_1

#define LED1 PC_10

#define ADDR 0x34
#define BADDR 0x35

void I2C_MODE() {
    DigitalOut led1(LED1);
    led1.write(1);

    BufferedSerial serial(USBTX, USBRX, 115200);    
    SerialStream<BufferedSerial> pc(serial);

    DigitalIn addrPin(ADDR_PIN);
    addrPin.mode(PullDown);


    if (addrPin.read() == 1) {
        pc.printf("using backup address: 0x%X\n", BADDR);
    }
    else {
        pc.printf("using regular address: 0x%X\n", ADDR);
    }
    

    Radio radio(&pc);
    radio.init();
    radio.setup_443();

    radio.set_debug(true);

    I2CSlave slave(I2C_SDA, I2C_SCL);
    
    if (!addrPin.read())
        slave.address(ADDR << 1); //keep actual addresss
    else
        slave.address(BADDR << 1);


    pc.printf("I2C confiigured\n");

    const char msg[] = "ArmLabCC1200";

    enum State {
        Idle,
        Transmit,
        Recieve_len,
        Recieve,
    };

    State state = Idle;
    int arg = 0;
    string packet = "";

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
                    slave.write(packet.length());


                    if (packet.length() == 0) {
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
                    slave.write(packet.c_str(), packet.length());
                    packet = "";
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
                            packet = radio.recieve();

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
                        }
                        case 8:
                            radio.set_modulation(buf[1]);
                            pc.printf("setting modulation to %d", buf[1]);
                            break;
                        
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

void SPI_MODE() {
    BufferedSerial serial(USBTX, USBRX, 115200);    
    SerialStream<BufferedSerial> pc(serial);

    pc.printf("running in SPI MODE\n");

    while (1) {}
}

void SERIAL_MODE() {
    DigitalOut led1(LED1);
    led1.write(1);

    BufferedSerial serial(USBTX, USBRX, 115200);    
    SerialStream<BufferedSerial> pc(serial);
    serial.set_blocking(true);


    BufferedSerial dummy(PA_0, PA_1, 9600);
    SerialStream<BufferedSerial> dummyPC(dummy);

    Radio radio(&dummyPC);
    radio.init();
    radio.setup_443();

    radio.set_debug(false);

    while (1) {
        serial.sync();
        led1.write(0);
        //pc.printf("idle\n");

        // get command
        char buf[5] = {0,0,0,0,0};
        //pc.scanf("%5s", buf);
        for (size_t i = 0; i<5; i++) {
            buf[i] = pc.getc();
        }
        pc.getc(); //flush trailing newline / return char
        
        //pc.printf("got cmd: 0x%X 0x%X 0x%X 0x%X 0x%X\n", buf[0], buf[1], buf[2], buf[3], buf[4]);

        switch (buf[0]) {
            case 1: {
                led1.write(1);
                int size = buf[1];

                char msg[size+1];            

                for (int i = 0; i<size; i++) {
                    msg[i] = pc.getc();
                }
                pc.getc(); //flush trailing newline / return char
                msg[size] = 0;

                radio.transmit(msg, size+1);
                pc.printf("%d, %s\n", buf[1], msg);
                break;
            }
            case 2: {
                led1.write(1);
                string packet = radio.recieve();

                pc.printf("%c",(char)packet.length());

                if (packet.length() == 0) {
                    continue;
                }

                pc.printf("%s", packet.c_str());

                led1.write(0);
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
            default: {
                char msg[13] = "ArmLabCC1200";
                serial.write(msg, 13);
            }
        }
    }
}

int main()
{
    DigitalIn spiPin(SPI_PIN);
    DigitalIn serPin(SERIAL_PIN);

    spiPin.mode(PullDown);
    serPin.mode(PullDown);

    if (serPin.read() == 1) {
        SERIAL_MODE();
    }
    else if (spiPin.read() == 1) {
        SPI_MODE();
    }

    I2C_MODE();

    
}



/*
#include <mbed.h>
#include <SerialStream.h>

#include <CC1200.h>
#include <cinttypes>

// Pin definitions
// change these to match your board setup
// Current setup for Nucleo-L476RG running on SPI1;  Comments are for running on SPI2
#define PIN_SPI_MOSI PA_7  //PC_3
#define PIN_SPI_MISO PA_6  //PC_2
#define PIN_SPI_SCLK PA_5  //PB_10

#define PIN_TX_CS PA_4  //PA_4
#define PIN_TX_RST PC_5 //PC_5

#define PIN_RX_CS PB_0  //PB_0
#define PIN_RX_RST PB_1 //PB_1

BufferedSerial serial(USBTX, USBRX, 115200);
SerialStream<BufferedSerial> pc(serial);

CC1200 txRadio(PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCLK, PIN_TX_CS, PIN_TX_RST, &pc);
CC1200 rxRadio(PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_SCLK, PIN_RX_CS, PIN_RX_RST, &pc);

void checkExistance()
{
	pc.printf("Checking TX radio.....\n");
	bool txSuccess = txRadio.begin();
	pc.printf("TX radio initialized: %s\n", txSuccess ? "true" : "false");

	pc.printf("Checking RX radio.....\n");
	bool rxSuccess = rxRadio.begin();
	pc.printf("RX radio initialized: %s\n", rxSuccess ? "true" : "false");
}

void checkSignalTransmit()
{
	pc.printf("Initializing CC1200s.....\n");
	txRadio.begin();
	rxRadio.begin();

	pc.printf("Configuring RF settings.....\n");

	int config=-1;
	//MENU. ADD AN OPTION FOR EACH TEST.
	pc.printf("Select a config: \n");
	// This is similar to the SmartRF "100ksps ARIB Standard" configuration, except it doesn't use zero-if
	pc.printf("1.  915MHz 100ksps 2-GFSK DEV=50kHz CHF=208kHz\n");
	// This should be identical to the SmartRF "500ksps 4-GFSK Max Throughput ARIB Standard" configuration
	pc.printf("2.  915MHz 500ksps 4-GFSK DEV=400kHz CHF=1666kHz Zero-IF\n");
	pc.printf("3.  915MHz 38.4ksps 2-GFSK DEV=20kHz CHF=833kHz\n");
	pc.printf("4.  915MHz 100ksps 2-GFSK DEV=50kHz CHF=208kHz Fixed Length\n");
    pc.printf("5.  440MHz 100kbps 2-GFSK DEV=50kHz ETSI Standard -- DoNotUse\n");


	pc.scanf("%d", &config);
	printf("Running test with config %d:\n\n", config);

	CC1200::PacketMode mode = CC1200::PacketMode::VARIABLE_LENGTH;
	CC1200::Band band = CC1200::Band::BAND_410_480MHz; //Band::BAND_820_960MHz;  //need to get to 440MHz (Armstrong)
	float frequency = 435e6; //915e6;  //use 435MHz (Armstrong)
	const float txPower = 0;
	float fskDeviation;
	float symbolRate;
	float rxFilterBW;
	CC1200::ModFormat modFormat;
	CC1200::IFCfg ifCfg = CC1200::IFCfg::POSITIVE_DIV_8;
	bool imageCompEnabled = true;
	bool dcOffsetCorrEnabled = true;
	uint8_t agcRefLevel;

	uint8_t dcFiltSettingCfg = 1; // default chip setting
	uint8_t dcFiltCutoffCfg = 4; // default chip setting
    uint8_t agcSettleWaitCfg = 2;

	CC1200::SyncMode syncMode = CC1200::SyncMode::SYNC_32_BITS;
	uint32_t syncWord = 0x930B51DE; // default sync word
	uint8_t syncThreshold = 8; // TI seems to recommend this threshold for most configs above 100ksps

	uint8_t preableLengthCfg = 5; // default chip setting
	uint8_t preambleFormatCfg = 0; // default chip setting

	if(config == 1)  // configure for 435 in RF Studio (Armstrong)
	{
		fskDeviation = 49896;
		symbolRate = 100000;
		rxFilterBW = 208300;
		modFormat = CC1200::ModFormat::GFSK_2;
		agcRefLevel = 0x2A;
	}
	else if(config == 2)
	{
		// SmartRF "500ksps 4-GFSK ARIB standard" config
		fskDeviation = 399169;
		symbolRate = 500000;
		rxFilterBW = 1666700;
		ifCfg = CC1200::IFCfg::ZERO;
		imageCompEnabled = false;
		dcOffsetCorrEnabled = true;
		modFormat = CC1200::ModFormat::GFSK_4;
		agcRefLevel = 0x2F;
	}
	else if(config == 3)
	{
		fskDeviation = 19989;
		symbolRate = 38400;
		rxFilterBW = 104200;
		modFormat = CC1200::ModFormat::GFSK_2;
		agcRefLevel = 0x27;
        agcSettleWaitCfg = 1;
	}
	else if(config == 4)
	{
		fskDeviation = 49896;
		symbolRate = 100000;
		rxFilterBW = 208300;
		modFormat = CC1200::ModFormat::GFSK_2;
		agcRefLevel = 0x2A;
		mode = CC1200::PacketMode::FIXED_LENGTH;
	}
	else if(config == 5)
	{
        fskDeviation = 49896;
		symbolRate = 100000;
		rxFilterBW = 208300;
		modFormat = CC1200::ModFormat::GFSK_2;
		agcRefLevel = 0x2A;
		mode = CC1200::PacketMode::FIXED_LENGTH;
    }
	else
	{
		pc.printf("Invalid config number!");
		return;
	}

	for(CC1200 & radio : {std::ref(txRadio), std::ref(rxRadio)})
	{

		radio.configureFIFOMode();
		radio.setPacketMode(mode);
		radio.setModulationFormat(modFormat);
		radio.setFSKDeviation(fskDeviation);
		radio.setSymbolRate(symbolRate);
		radio.setOutputPower(txPower);
		radio.setRadioFrequency(band, frequency);
		radio.setIFCfg(ifCfg, imageCompEnabled);
		radio.configureDCFilter(dcOffsetCorrEnabled, dcFiltSettingCfg, dcFiltCutoffCfg);
		radio.setRXFilterBandwidth(rxFilterBW);
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

	// configure on-transmit actions
	txRadio.setOnTransmitState(CC1200::State::TX);
	rxRadio.setOnReceiveState(CC1200::State::RX, CC1200::State::RX);

	pc.printf("Starting transmission.....\n");

	txRadio.startTX();
	rxRadio.startRX();

	const char message[] = "Hello Ethan Armstrong!";
	char receiveBuffer[sizeof(message)];

	if(mode == CC1200::PacketMode::FIXED_LENGTH)
	{
		txRadio.setPacketLength(sizeof(message) - 1);
		rxRadio.setPacketLength(sizeof(message) - 1);
	}

	while(true)
	{
		ThisThread::sleep_for(1s);

		pc.printf("\n---------------------------------\n");

		pc.printf("<<SENDING: %s\n", message);
		txRadio.enqueuePacket(message, sizeof(message) - 1);


		// update TX radio
		pc.printf("TX radio: state = 0x%" PRIx8 ", TX FIFO len = %zu, FS lock = 0x%u\n",
				  static_cast<uint8_t>(txRadio.getState()), txRadio.getTXFIFOLen(), txRadio.readRegister(CC1200::ExtRegister::FSCAL_CTRL) & 1);

		// wait a while for the packet to come in.
		ThisThread::sleep_for(10ms);

		// wait the time we expect the message to take, with a safety factor of 2
		auto timeoutTime = std::chrono::microseconds(static_cast<int64_t>((1/symbolRate) * 1e-6f * sizeof(message) * 2));

		Timer packetReceiveTimeout;
		packetReceiveTimeout.start();
		bool hasPacket;
		do
		{
			hasPacket = rxRadio.hasReceivedPacket();
		}
		while(packetReceiveTimeout.elapsed_time() < timeoutTime && !hasPacket);

		pc.printf("RX radio: state = 0x%" PRIx8 ", RX FIFO len = %zu\n",
				  static_cast<uint8_t>(rxRadio.getState()), rxRadio.getRXFIFOLen());
		if(hasPacket)
		{
			rxRadio.receivePacket(receiveBuffer, sizeof(message) - 1);
			receiveBuffer[sizeof(message)-1] = '\0';
			pc.printf(">>RECEIVED: %s\n", receiveBuffer);
		}
		else
		{
			pc.printf(">>No packet received!\n");
		}
	}
}


int main()
{
	pc.printf("\nCC1200 Radio Test Suite:\n");

	while(1){
		int test=-1;
		//MENU. ADD AN OPTION FOR EACH TEST.
		pc.printf("Select a test: \n");
		pc.printf("1.  Exit Test Suite\n");
		pc.printf("2.  Check Existence\n");
		pc.printf("3.  Check Transmitting Signal\n");

		pc.scanf("%d", &test);
		printf("Running test %d:\n\n", test);
		//SWITCH. ADD A CASE FOR EACH TEST.
		switch(test) {
			case 1:         pc.printf("Exiting test suite.\n");    return 0;
			case 2:         checkExistance();              break;
			//case 3:         checkSignalTransmit();              break;
			default:        pc.printf("Invalid test number. Please run again.\n"); continue;
		}
		pc.printf("done.\r\n");
	}

	return 0;

}
*/