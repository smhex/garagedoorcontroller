#include <assert.h>
#include <stdio.h>
#include "debug_window.h"
#include "firmware_image.h"

int main() {
    DebugWindow window;
    assert(window.enabled(0));
    assert(window.enabled(59999));
    assert(!window.shouldExpire(59999));
    assert(window.shouldExpire(60000));
    window.expire();
    assert(!window.enabled(60000));
    assert(!window.enabled(0xffffffff));
    assert(!window.enabled(1)); // timer wrap must not open another boot window
    window.enable();
    assert(window.enabled(60001));
    assert(window.enabled(0));
    DebugWindow reboot;
    assert(reboot.shouldExpire(60000));
    reboot.expire();
    assert(!reboot.enabled(60000));

    const uint8_t text[] = "123456789";
    assert((FirmwareImage::crc32(0xffffffff, text, 9) ^ 0xffffffff) == 0xcbf43926);
    assert(!FirmwareImage::validSize(0x4007));
    assert(FirmwareImage::validSize(0x3e000));
    assert(!FirmwareImage::validSize(0x3e001));
    uint8_t vectors[] = {0, 0x80, 0, 0x20, 1, 0x61, 0, 0};
    assert(FirmwareImage::validVectors(vectors, 0x8000));
    vectors[4] = 0; assert(!FirmwareImage::validVectors(vectors, 0x8000));
    vectors[4] = 1; vectors[3] = 0; assert(!FirmwareImage::validVectors(vectors, 0x8000));
    puts("PASS: console timeout/reboot/wrap, image bounds/vectors and CRC32");
}
