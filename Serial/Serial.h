#include <mbed.h>

#ifndef _SERIAL_H_
#define _SERIAL_H_

#define SERIAL_MAX_OUTPUT_SIZE 512

class Serial : public BufferedSerial {
public:
    Serial(PinName tx, PinName rx, int baud) : BufferedSerial(tx, rx, baud) {
    }

    inline bool printf(const char* format, ...) {
        va_list args;

        va_start(args, format);
        bool res = this->vprintf(format, args);
        va_end(args);

        return res;
    };

    inline bool debug(const char* location, const char* format, ...) {
        va_list args;

        va_start(args, format);
        this->printf("%s> ", location);
        bool res = this->vprintf(format, args);
        va_end(args);
        return res;
    }
private:
    inline bool vprintf(const char* format, va_list args) {
        char buffer[SERIAL_MAX_OUTPUT_SIZE];

        size_t n = vsnprintf(buffer, SERIAL_MAX_OUTPUT_SIZE, format, args);
        if (n > 0 && n < SERIAL_MAX_OUTPUT_SIZE) {
            return this->write(buffer, n) == n;
        }
        return false;
    }
};
#endif //_SERIAL_H_