#!/usr/bin/env python

import colorsys
import socket
import sys
import time
import random

def hsv2rgb(h,s,v):
    (r, g, b) = colorsys.hsv_to_rgb(h,s,v)
    return (int(r*255), int(g*100), int(b*50))

def rainbow_pixel(pixel, hue, s=1.0, v=1.0):
    scaling = 360/pixelcount
    if pixel >= 0:
        hue2 = (hue+(pixel*scaling))%360
        rgb = hsv2rgb(hue2/360.0, s, v)
        return rgb

def rainbow():
    scaling = 360/pixelcount
    while True:
        for hue in range(0, 360):
            message = [0x03]
            for pixel in range(0, pixelcount):
                rgb = rainbow_pixel(pixel, hue)
                message.append(rgb[1])
                message.append(rgb[0])
                message.append(rgb[2])
                #if pixel >= 0:
                #    hue2 = (hue+(pixel*scaling))%360
                #    rgb = hsv2rgb(hue2/360.0, 1.0, 1.0)
                #    message.append(rgb[1])
                #    message.append(rgb[0])
                #    message.append(rgb[2])
                #else:
                #    message.append(0)
                #    message.append(0)
                #    message.append(0)
            sock.sendto("".join(map(chr, message)), (hostname, port))
            time.sleep(0.05)

def fade():
    while True:
        for hue in range(0, 360):
            message = [0x01]
            rgb = hsv2rgb(hue/360.0, 1.0, 1.0)
            message.append(int(rgb[0]))
            message.append(int(rgb[1]))
            message.append(int(rgb[2]))
            sock.sendto("".join(map(chr, message)), (hostname, port))
            time.sleep(0.05)

def sunrise(t=10):
    while True:
        for level in range(7, 256):
            message = [0x01]
            message.append(level)
            message.append(0)
            message.append(0)
            sock.sendto("".join(map(chr, message)), (hostname, port))
            time.sleep(t)
        for level in range(0, 256):
            message = [0x01]
            message.append(255)
            message.append(level)
            message.append(0)
            sock.sendto("".join(map(chr, message)), (hostname, port))
            time.sleep(t)
        for level in range(0, 256):
            message = [0x01]
            message.append(255)
            message.append(255)
            message.append(level)
            sock.sendto("".join(map(chr, message)), (hostname, port))
            time.sleep(t)

def zap():
    while True:
        for pixel in range(0, pixelcount) + range(pixelcount+1, 0, -1):
            message = [0x03]
            for p in range(0, pixelcount):
                if pixel == p:
                    message.append(255)
                    message.append(255)
                    message.append(255)
                else:
                    message.append(0)
                    message.append(0)
                    message.append(0)
            sock.sendto("".join(map(chr, message)), (hostname, port))
            time.sleep(0.01)
        #time.sleep(0.5)

def strobe(t1=0.001, t2=0.5):
    while True:
        message = [0x01, 255, 255, 255]
        sock.sendto("".join(map(chr, message)), (hostname, port))
        time.sleep(t1)
        message = [0x01, 0, 0, 0]
        sock.sendto("".join(map(chr, message)), (hostname, port))
        time.sleep(t2)

def colour(hexcolor):
    red = int(hexcolor[0:2], base=16)
    green = int(hexcolor[2:4], base=16)
    blue = int(hexcolor[4:6], base=16)
    message = [0x01, red, green, blue]
    sock.sendto("".join(map(chr, message)), (hostname, port))
    sock.sendto("".join(map(chr, message)), (hostname, port))

def chase(n=5, t=0.05):
    while True:
        for i in range(0, n):
            message = [0x03]
            for p in range(0, pixelcount):
                if p % n == i:
                    message.append(0)
                    message.append(255)
                    message.append(0)
                else:
                    message.append(0)
                    message.append(0)
                    message.append(0)
            sock.sendto("".join(map(chr, message)), (hostname, port))
            time.sleep(t)

def twinkle():
    low = 50
    high = 255
    levels = [low]*pixelcount
    current = {}
    lastchange = 0
    interval = 0.2
    while True:
        if time.time()-lastchange > interval:
            target = random.randint(0, pixelcount-1)
            if current.has_key(target):
                pass
            else:
                print target
                current[target] = True
                lastchange = time.time()
        message = [0x03]
        for p in range(0, pixelcount):
            if current.has_key(p):
                levels[p] = min(levels[p]+10, high)
                if levels[p] == high:
                    del current[p]
            else:
                if levels[p] > low:
                    levels[p] = max(levels[p] - 10, low)
            #print levels[p],
            message.append(levels[p])
            message.append(levels[p])
            message.append(levels[p])
        #print
        #print current
        #print
        sock.sendto("".join(map(chr, message)), (hostname, port))
        time.sleep(0.01)

def twinkle2():
    low = 127
    high = 255
    levels = [low]*pixelcount
    current = {}
    lastchange = 0
    interval = 0.1
    hue = 0
    while True:
        hue = (hue+1) % 360
        if time.time()-lastchange > interval:
            target = random.randint(0, pixelcount-1)
            if current.has_key(target):
                pass
            else:
                print target
                current[target] = True
                lastchange = time.time()
        message = [0x03]
        for p in range(0, pixelcount):
            if current.has_key(p):
                levels[p] = min(levels[p]+50, high)
                if levels[p] == high:
                    del current[p]
            else:
                if levels[p] > low:
                    levels[p] = max(levels[p] - 50, low)
            #print levels[p],
            rgb = rainbow_pixel(p, hue, v=levels[p]/255.0)
            message.append(rgb[1])
            message.append(rgb[0])
            message.append(rgb[2])
            #message.append(levels[p])
            #message.append(levels[p])
            #message.append(levels[p])
        #print
        #print current
        #print
        sock.sendto("".join(map(chr, message)), (hostname, port))
        time.sleep(0.05)

def xmas():
    states = [False]*pixelcount
    lastchange = 0
    interval = 0.2
    for p in range(0, pixelcount):
        states[p] = random.choice([False, True])
    while True:
        if time.time()-lastchange > interval:
            target = random.randint(0, pixelcount-1)
            states[target] = not(states[target])
            lastchange = time.time()
        message = [0x03]
        for p in range(0, pixelcount):
            if states[p]:
                message.append(255)
                message.append(0)
                message.append(0)
            else:
                message.append(0)
                message.append(255)
                message.append(0)
        sock.sendto("".join(map(chr, message)), (hostname, port))
        time.sleep(0.2)

hostname = sys.argv[1]
port = int(sys.argv[2])
mode = sys.argv[3]
pixelcount = 50

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

if mode == "rainbow":
    rainbow()
elif mode == "fade":
    fade()
elif mode == "zap":
    zap()
elif mode == "strobe":
    strobe()
elif mode == "sunrise":
    sunrise()
elif mode == "chase":
    chase()
elif mode == "twinkle":
    twinkle()
elif mode == "twinkle2":
    twinkle2()
elif mode == "xmas":
    xmas()
else:
    colour(mode)
