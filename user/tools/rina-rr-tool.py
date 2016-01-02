#!/usr/bin/env python

from rlite import *
import argparse
import select
import sys
import os


def python2():
    return sys.version_info[0] <= 2


def str2buf(s):
    if python2():
        return s
    return bytes(s, 'ascii')


def buf2str(b):
    if python2():
        return b
    return b.decode('ascii')


class RinaRRTool:

    def __init__(self):
        self.rc = rlite_ctrl()
        rl_ctrl_init(self.rc, None)
        self.client_name = rina_name()
        self.server_name = rina_name()
        self.dif = rina_name()
        self.ipcp_name = rina_name()
        self.flow_spec = rlite_flow_spec()
        rl_flow_spec_default(self.flow_spec)


    def __del__(self):
        rl_ctrl_fini(self.rc)


    def client(self, args):
        fd = rl_ctrl_flow_alloc(self.rc, self.dif, self.ipcp_name,
                                self.client_name, self.server_name,
                                self.flow_spec)
        if fd < 0:
            return fd

        msg = 'Hello guys, this is a test message!'
        n = os.write(fd, str2buf(msg))
        if n != len(msg):
            print("Partial write %s/%s", n, len(msg))

        r, w, e = select.select([fd], [], [], 3000)
        if len(r) == 0:
            print("Timeout")
            return -1

        buf = os.read(fd, 65535)
        print("Response: '%s'" % buf2str(buf))

        os.close(fd)

        return 0


    def server(self, args):
        ret = rl_ctrl_register(self.rc, True, self.dif, self.ipcp_name,
                               self.server_name)
        if ret:
            return ret

        while 1:
            fd = rl_ctrl_flow_accept(self.rc)
            if fd < 0:
                continue

            r, w, e = select.select([fd], [], [], 3000)
            if len(r) == 0:
                print("Timeout")
                return -1

            buf = os.read(fd, 65535)
            print("Request: '%s'" % buf2str(buf))

            n = os.write(fd, buf)
            if n != len(buf):
                print("Partial write %s/%s", n, len(buf))

            os.close(fd)

        return 0


description = "RINA echo client/server"
epilog = "2015 Vincenzo Maffione <v.maffione@gmail.com>"

argparser = argparse.ArgumentParser(description = description,
                                    epilog = epilog)
argparser.add_argument('-l', '--listen', dest = 'server_mode',
                       action='store_true',
                       help = "Run in server mode")
argparser.add_argument('-d', '--dif',
                       help = "DIF to register to or ask for flow allocation",
                       type = str)
argparser.add_argument('-f', '--flow-spec', help = "Flow specification",
                       type = str, default='unrel')
argparser.add_argument('-p', '--ipcp-apn', help = "IPCP APN (overwrites -d)",
                       type = str)
argparser.add_argument('-P', '--ipcp-api', help = "IPCP API (overwrites -d)",
                       type = str)
argparser.add_argument('-a', '--client-apn', help = "client APN",
                       type = str, default='rlite_rr-data')
argparser.add_argument('-A', '--client-api', help = "client API",
                       type = str, default='client')
argparser.add_argument('-z', '--server-apn', help = "server APN",
                       type = str, default='rlite_rr-data')
argparser.add_argument('-Z', '--server-api', help = "server API",
                       type = str, default='server')

args = argparser.parse_args()

if args.ipcp_apn == None:
    args.ipcp_api = None

try:
    rr = RinaRRTool()

    rina_name_fill(rr.client_name, args.client_apn, args.client_api, None, None)
    rina_name_fill(rr.server_name, args.server_apn, args.server_api, None, None)
    rina_name_fill(rr.ipcp_name, args.ipcp_apn, args.ipcp_api, None, None)
    rr.dif = args.dif
    rr.flow_spec.cubename = args.flow_spec

    if args.server_mode:
        rr.server(args)
    else:
        rr.client(args)
except KeyboardInterrupt:
    pass
