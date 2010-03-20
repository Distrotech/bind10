from socketserver import *
import os
from bind10_message import *
import struct
import socket

class MyHandler(BaseRequestHandler):
    def handle(self):
        #TODO. recv fd
        fd = receive_fd(self.request.fileno())
        data_len = self.request.recv(2)
        msg_len = struct.unpack('H', data_len)[0]
        msgdata = self.request.recv(msg_len)
        print(msgdata)

        sock = socket.fromfd(fd, socket.AF_INET, socket.SOCK_STREAM)
        sock.send(msgdata)
        sock.send(b'===============================')
        sock.close()


if '__main__' == __name__:
    file = 'conn'
    if os.path.isfile(file):
        os.unlink(file)
    server = ThreadingUnixStreamServer(file, MyHandler);
    server.serve_forever()

