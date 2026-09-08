#include <assert.h>
#include <stdio.h>
#include "debug_window.h"
#include "firmware_image.h"
#include "local_approval.h"

int main() {
    DebugWindow window;
    assert(window.enabled(0));
    assert(window.enabled(29999));
    assert(!window.enabled(30000));
    assert(!window.enabled(0xffffffff));
    assert(!window.enabled(1)); // timer wrap must not open another boot window
    window.enable();
    assert(window.enabled(30001));
    assert(window.enabled(0));
    DebugWindow reboot;
    assert(!reboot.enabled(30000));

    LocalApproval approval;
    approval.begin(8);
    assert(!approval.update(8, false)); // stale release sample
    assert(!approval.update(9, true)); // button already held
    assert(!approval.update(10, true));
    assert(!approval.update(11, false));
    assert(!approval.update(11, true)); // no fresh sample
    assert(approval.update(12, true));
    approval.begin(12);
    assert(!approval.update(13, true)); // next upload requires another release

    const uint8_t text[] = "123456789";
    assert((FirmwareImage::crc32(0xffffffff, text, 9) ^ 0xffffffff) == 0xcbf43926);
    assert(!FirmwareImage::validSize(0x4007));
    assert(FirmwareImage::validSize(0x3e000));
    assert(!FirmwareImage::validSize(0x3e001));
    uint8_t vectors[] = {0, 0x80, 0, 0x20, 1, 0x61, 0, 0};
    assert(FirmwareImage::validVectors(vectors, 0x8000));
    vectors[4] = 0; assert(!FirmwareImage::validVectors(vectors, 0x8000));
    vectors[4] = 1; vectors[3] = 0; assert(!FirmwareImage::validVectors(vectors, 0x8000));
    puts("PASS: console timeout/reboot/wrap, fresh local approval, image bounds/vectors and CRC32");
}
