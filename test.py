import time
import serial
import sys
import os
from pynput.keyboard import Key, Listener
from pynput import keyboard
import threading
import socket
import argparse

verbosity = 0
s = ['']
tmps = ['']
ser = [None]
serin = ['']
sock_conn = [None]
sockin = [[]]
socklock = [threading.Lock()]

py_print = print
def my_print(s):
    py_print(s)
    sys.stdout.flush()
print = my_print

def errprint(test, s):
    print(test+": Failed: "+s)
    return False

def vprint(s, lvl=0):
    if verbosity > lvl:
        print(s)
def keypressed(key):
    if hasattr(key, 'char'):
        tmps[0] += key.char
        py_print(key.char, end='')
        sys.stdout.flush()
    elif key == keyboard.Key.enter:
        s[0] = tmps[0]
        tmps[0] = ''
        print("\n\r")

def keyreleased(key):
    pass

def terminal():
    listener = Listener(on_press=keypressed, on_release=keyreleased)
    listener.start()
    print('Enter your commands below.\r\nInsert "exit" to leave the application.')

    py_print(">>", end='')
    while True :
        # get keyboard input
        cmd = s[0]
        if len(cmd)==0:
            time.sleep(0.01)
            pass
        else:
            s[0] = ''
            if cmd == 'exit':
                ser[0].close()
                return True
            else:
                ser[0].write(bytes(cmd + '\r\n','utf-8'))
                out = ''
                time.sleep(0.1)
                while ser[0].inWaiting() > 0:
                    while ser[0].inWaiting() > 0:
                        out += ser[0].read(1).decode('utf-8')
                    time.sleep(0.1)
                if out != '':
                    print(out)
                    py_print("\n\r>>", end='')

def sock_write(b):
    if isinstance(b, int):
        vprint("sockout: "+str(b),0)
        sock_conn[0].sendall(bytes([b]))
    elif isinstance(b, str):
        vprint("sockout: "+b,0)
        sock_conn[0].sendall(bytes(b, 'utf-8'))
    else:
        vprint("sockout: "+str(len(b))+" bytes",0)
        if verbosity > 1:
            print("sockout buffer:")
            print_buf(b)
        sock_conn[0].sendall(b)

def serial_write(b):
    if isinstance(b, int):
        vprint("serout: "+str(b),0)
        ser[0].write(bytes([b]))
    elif isinstance(b, str):
        vprint("serout: "+b,0)
        ser[0].write(bytes(b, 'utf-8'))
    else:
        global verbosity
        vprint("serout: "+str(len(b))+" bytes",0)
        if verbosity > 1:
            print("serout buffer:")
            print_buf(b)
        ser[0].write(b)

def serial_writeln(s):
    vprint("serout: "+s,0)
    global verbosity
    v = verbosity
    verbosity = 0
    ser[0].write(bytes(s + "\r\n", 'utf-8'))
    verbosity = v

def serial_transact(cmd, sec=0.3):
    flush_serial()
    global verbosity
    v = verbosity
    verbosity = 0
    serial_writeln(cmd)
    serial_wait(sec / 0.08)
    res = serial_inln()
    verbosity = v
    vprint("sercmd: "+cmd+": "+str(res),0)
    return res

def serial_wait(max_ct=10):
    ct = 0
    while ser[0].inWaiting() == 0 and ct < max_ct:
        time.sleep(0.1)
        ct = ct + 1

# waits for a line of text.  if none arrives in time, preserve what it has gotten so far
def serial_inln():
    serial_wait(10)
    while ser[0].inWaiting() > 0:
        while ser[0].inWaiting() > 0:
            c = ser[0].read(1).decode('utf-8')
            if c == '\n' or c == '\r':
                s = serin[0]
                serin[0] = ''
                vprint("serin: "+s,0)
                return s
            else:
                serin[0] += c
        serial_wait(3)
    return None

# returns all bytes it can reasonably wait for from the serial port
def serial_in(expect=0):
    global verbosity
    serial_wait(10)
    bin = []
    while ser[0].inWaiting() > 0:
        while ser[0].inWaiting() > 0:
            c = ser[0].read(1)
            bin.extend(c)
        if expect > 0 and len(bin) >= expect:
            break
        serial_wait(10)
    vprint("serin: "+str(len(bin))+" bytes",0)
    ba = bytearray(bin)
    if verbosity > 1:
        print("serin buffer:")
        print_buf(ba)
    return ba
    
def flush_serial():
    serin[0] = ''
    serial_wait(1)
    while ser[0].inWaiting() > 0:
        while ser[0].inWaiting() > 0:
            ser[0].read(1)
        serial_wait(1)

def sock_wait(max_ct=10):
    ct = 0
    while len(sockin[0]) == 0 and ct < max_ct:
        time.sleep(0.1)
        ct = ct + 1

def flush_sock():
    sock_wait(1)
    while len(sockin[0]) >0:
        sockin[0] = [] # reset sockin buff
        sock_wait(1)

def sock_in(expect=0):
    global verbosity
    sock_wait(8)
    sin = []
    # print("sock_in: "+str(len(sockin[0])))
    while len(sockin[0]) >0:
        socklock[0].acquire()
        sin.extend(sockin[0])
        # print("sin: "+str(len(sin))+"/"+str(ord(sockin[0][0])))
        sockin[0] = [] # reset sockin buff
        socklock[0].release()
        if expect > 0 and len(sin) >= expect:
            break
        sock_wait(10)
    vprint("sockin: "+str(len(sin))+" bytes",0)
    ba = bytearray(sin)
    if verbosity > 1:
        print("sockin buffer:")
        print_buf(ba)
    return ba

def thread_socket_listener():
    try:
        hostname = socket.gethostname()
        ipaddr = socket.gethostbyname(hostname)
        serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        serversocket.bind((str(ipaddr), 8089))
        serversocket.listen(5)
        print("Listener ("+str(ipaddr)+") started")
        while True:
            sock_conn[0] = None
            connection, address = serversocket.accept()
            sock_conn[0] = connection
            try:
                while True:
                    time.sleep(0.1)
                    buf = connection.recv(64)
                    if len(buf) > 0:
                        socklock[0].acquire()
                        sockin[0].extend(buf)
                        socklock[0].release()
            except BaseException as e:
                pass
    finally:
        print("Listener closed")

def compare_bytearrays(a, b):
    la = list(a)
    lb = list(b)
    if len(la) != len(lb):
        return False
    for i in range(0,len(la)):
        if la[i] != lb[i]:
            return False
    return True

def print_buf(b):
    for i in range(0,len(b)):
        py_print(str(b[i])+" ",end="")
        if (i+1) % 8 == 0:
            print("")
    if (len(b) + 1) & 8 != 0:
        print("")

def test_atd(baud=1200):
    global verbosity
    if sock_conn[0] != None:
        return errprint("ATD","Previous socked connected")
    ipaddr = socket.gethostbyname(socket.gethostname())
    s = serial_transact('atd"'+str(ipaddr)+':8089"', sec=10)
    if s is None or s[0:8] != 'CONNECT ':
        return errprint("ATD","ATD ("+s+")")
    connum = s[8:]
    if sock_conn[0] is None:
        return errprint("ATD","No Connection")
    b = bytearray(os.urandom(1024))
    # send from socket->modem, this will be slow to receive all.
    sock_write(b)
    sb = serial_in()
    if not compare_bytearrays(b, sb):
        return errprint("ATD","sock->ser "+str(len(b)))
    # send from modem->socket, this will be slow to send.
    flush_serial()
    flush_sock()
    serial_write(b)
    rb = sock_in(len(b))
    if not compare_bytearrays(b, rb):
        return errprint("ATD","ser->sock "+str(len(b)))
    flush_serial()
    flush_sock()
    
    ud_tests = [[128, 50], [256, 50], [1024, 15 * (baud / 1200)], [baud * 20, 1]]
    for ud_test in ud_tests:
        packet_size = round(ud_test[0])
        rounds = round(ud_test[1])
        # download test 
        for rnd in range(0,rounds):
            b = bytearray(os.urandom(packet_size))
            sock_write(b)
            rb = serial_in(len(b))
            if not compare_bytearrays(b, rb):
                return errprint("ATD","sock->ser "+str(len(b))+" round "+str(rnd))
            serial_write(13)
            rb = sock_in(1)
            if not compare_bytearrays(bytes([13]), rb):
                return errprint("ATD","sock->ser  ack round "+str(rnd))
        # upload test 
        for rnd in range(0,rounds):
            b = bytearray(os.urandom(packet_size))
            serial_write(b)
            rb = sock_in(len(b))
            if not compare_bytearrays(b, rb):
                return errprint("ATD","ser->sock "+str(len(b))+" round "+str(rnd))
            sock_write(13)
            rb = serial_in(1)
            if not compare_bytearrays(bytes([13]), rb):
                return errprint("ATD","sock->ser  ack round "+str(rnd))
    
    print("ATD   : Passed")
    sock_conn[0].close()
    sock_conn[0] = None
    return True

def test_atc(baud=1200):
    if sock_conn[0] != None:
        return errprint("ATC","Previous socked connected")
    ipaddr = socket.gethostbyname(socket.gethostname())
    s = serial_transact('atc"'+str(ipaddr)+':8089"', sec=10)
    if s is None or s[0:8] != 'CONNECT ':
        return errprint("ATC","ATC")
    connum = s[8:]
    if sock_conn[0] is None:
        return errprint("ATC","No Connection")
    if serial_transact('att"testing!"') != 'OK':
        return errprint("ATC","ATT 1")
    if sock_in().decode() != 'testing!\r\n':
        return errprint("ATC","ATT2 "+str(s))
    # should return a conn message, and THEN ok
    if serial_transact('atc0') == 'OK':
        return errprint("ATC","ATC0:MSG")
    if serial_inln() != 'OK':
        return errprint("ATC","ATC0:OK")
    if serial_transact('ath'+connum) != 'OK':
        return errprint("ATC","ATHx")
    if serial_transact('atc0') != 'OK':
        return errprint("ATC","ATC0 #2" )
    print("ATC   : Passed")
    sock_conn[0].close()
    sock_conn[0] = None
    return True
    

def tester(baud):
    print("Starting Zimodem test")
    if not test_atc(baud):
        return False
    if not test_atd(baud):
        return False
    # TODO
    return True
    
def initialize(port, baud):
    global verbosity
    ser[0] = serial.Serial(
        port=port,
        baudrate=1200,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS
    )
    
    if not ser[0].isOpen():
        print("Unable to open serial port")
        return None
    serial_writeln('ath0z0r0e0&p0b'+str(baud))
    time.sleep(1)
    flush_serial()
    if baud != 1200:
        ser[0].close()
        ser[0] = serial.Serial(
            port=port,
            baudrate=baud,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS
        )
    cmd = "i4"
    if verbosity > 1:
        cmd = "&o1i4"
    s = serial_transact("at"+cmd)
    if s is None or (s[0:2] != '3.' and s[0:2] != '4.'):
        print("Unable to initialize")
        return None
    flush_serial()
    print("Modem Initialized "+s)
    server_thread = threading.Thread(target=thread_socket_listener, daemon=True)
    server_thread.start()
    time.sleep(3) # wait for listener to kick off
    return ser[0]

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
                        prog='Zimodem Tester',
                        description='Runs various tests on a Zimodem.  Must always start at 1200bps!')
    parser.add_argument('port')
    parser.add_argument('baud', default=1200)
    parser.add_argument('-t', '--term', default=False)
    parser.add_argument('-v', '--verbosity', default=0)
    args = parser.parse_args()
    verbosity = int(args.verbosity)
    baud = int(args.baud)
    ser[0] = initialize(args.port, baud)
    if ser[0] is None:
        sys.exit(-1)
    
    result = False
    try:
        if args.term:
            terminal()
            sys.exit(0)
        # configure the serial connections (the parameters differs on the device you are connecting to)
        result = tester(baud)
    finally:
        if ser[0].isOpen():
            time.sleep(1);
            serial_write('+++')
            time.sleep(1)
            if verbosity > 1:
                flush_serial()
                serial_writeln("at&o0")
                time.sleep(1)
                v=verbosity
                verbosity=0
                buf = serial_in()
                print("Log:")
                print(buf.decode('utf-8'))
                verbosity=v
            serial_writeln('atb1200')
            ser[0].close()
    if result:
        print("Test Completed Successfully")
    else:
        print("Test Failed.")
