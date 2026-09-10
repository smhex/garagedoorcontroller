"""Exercise the uploader against a local fake controller, without hardware."""
import contextlib
import io
from pathlib import Path
import socket
import struct
import sys
import threading
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from scripts.upload_lan import upload, validate_image


class UploadTests(unittest.TestCase):
    def image(self):
        image = bytearray(0x8000)
        struct.pack_into('<II', image, 0x4000, 0x20008000, 0x6101)
        return bytes(image)

    def controller(self, reply, expected_body):
        listener = socket.socket()
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        port = listener.getsockname()[1]
        errors = []

        def serve():
            try:
                with listener, listener.accept()[0] as client:
                    client.settimeout(5)
                    stream = client.makefile('rb')
                    header = stream.readline().decode().split()
                    self.assertEqual(header[:2], ['GDC1', 'a' * 48])
                    if not expected_body:
                        client.sendall(reply)
                        return
                    client.sendall(b'READY\n')
                    if expected_body:
                        body = stream.read(int(header[2]))
                        self.assertEqual(body, self.image())
                        self.assertEqual(int(header[3], 16), zlib.crc32(body))
                        client.sendall(b'STAGED awaiting installation\n')
                    stream.close()
            except BaseException as error:
                errors.append(error)

        thread = threading.Thread(target=serve)
        thread.start()
        return port, thread, errors

    def test_upload_stages_before_installation(self):
        port, thread, errors = self.controller(b'', True)
        with contextlib.redirect_stdout(io.StringIO()):
            upload('127.0.0.1', self.image(), 'a' * 48, port)
        thread.join(6)
        self.assertFalse(thread.is_alive())
        self.assertEqual(errors, [])

    def test_controller_rejection_sends_no_firmware(self):
        port, thread, errors = self.controller(b'ERROR controller busy\n', False)
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaisesRegex(RuntimeError, 'controller busy'):
            upload('127.0.0.1', self.image(), 'a' * 48, port)
        thread.join(6)
        self.assertEqual(errors, [])

    def test_invalid_image(self):
        with self.assertRaises(ValueError):
            validate_image(b'not firmware')
        image = bytearray(self.image())
        image[0x4000:0x4008] = b'\0' * 8
        with self.assertRaises(ValueError):
            validate_image(image)


if __name__ == '__main__':
    unittest.main()
