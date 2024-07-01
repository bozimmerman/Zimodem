import time
import serial
import sys
import os
from pynput.keyboard import Key, Listener
from pynput import keyboard
import threading
import socket

s = ['']
tmps = ['']

def keypressed(key):
    if hasattr(key, 'char'):
        tmps[0] += key.char
        print(key.char, end='')
        sys.stdout.flush()
    elif key == keyboard.Key.enter:
        s[0] = tmps[0]
        tmps[0] = ''
        print("\n\r")

def keyreleased(key):
    pass

# configure the serial connections (the parameters differs on the device you are connecting to)
ser = serial.Serial(
    port='COM4',
    baudrate=1200,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS
)

ser.isOpen()

def terminal():
    listener = Listener(on_press=keypressed, on_release=keyreleased)
    listener.start()
    print('Enter your commands below.\r\nInsert "exit" to leave the application.')

    print(">>", end='')
    while 1 :
        # get keyboard input
        cmd = s[0]
        if len(cmd)==0:
            time.sleep(0.01)
            pass
        else:
            s[0] = ''
            if cmd == 'exit':
                ser.close()
                exit()
            else:
                ser.write(bytes(cmd + '\r\n','utf-8'))
                out = ''
                time.sleep(0.1)
                while ser.inWaiting() > 0:
                    while ser.inWaiting() > 0:
                        out += ser.read(1).decode('utf-8')
                    time.sleep(0.1)
                if out != '':
                    print(out)
                    print("\n\r>>", end='')


serin = ['']
def serial_writeln(s):
    ser.write(bytes(s + "\r\n", 'utf-8'))

def serial_transact(cmd, sec=1):
    serial_writeln(cmd)
    time.sleep(sec)
    return serial_in()   

def serial_in():
    time.sleep(0.1)
    while ser.inWaiting() > 0:
        while ser.inWaiting() > 0:
            c = ser.read(1).decode('utf-8')
            if c == '\n' or c == '\r':
                s = serin[0]
                serin[0] = ''
                return s
            else:
                serin[0] += c
        time.sleep(0.1)
    return None

def flush_serial():
    time.sleep(0.1)
    serin[0] = ''
    while ser.inWaiting() > 0:
        while ser.inWaiting() > 0:
            ser.read(1)
        time.sleep(0.1)

sock_conn = [False]

def tester_socket_listener():
    sock_conn[0] = False
    serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    serversocket.bind(('localhost', 8089))
    serversocket.listen(1)
    connection, address = serversocket.accept()
    sock_conn[0] = True
    while True:
        buf = connection.recv(64)
        if len(buf) > 0:
            print(str(len(buf))+": "+str(buf))

def tester():
    serial_writeln('ath0z0r0e0&p0')
    time.sleep(1)
    flush_serial()
    print("Starting Zimodem test")
    s = serial_transact('ati4')
    if s is None or s[0:3] != '3.7':
        print("Unable to initialize ("+s+")")
        sys.exit(-1)
    print("Modem Initialized "+s)
    server_thread = threading.Thread(target=tester_socket_listener, daemon=True)
    server_thread.start()
    s = serial_transact('atc"localhost:8089"', sec=5)
    if s is None:
        print("Connection Failure: Transaction")
        sys.exit(-1)
    print(s)
    if not sock_conn[0]:
        print("Connection Failure: No Connection detected")
        sys.exit(-1)
    
    
        
tester()
     
