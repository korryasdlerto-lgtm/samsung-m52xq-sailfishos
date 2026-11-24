#!/usr/bin/python3
# m52xq: jolla-camera does not take the volume keys (resource ScaleButton): the system screenshot (both keys) works in the camera too.
# The shutter by a volume key is gone. Usage: captureview-volume-keys.py <CaptureView.qml>   (jolla-camera 1.3.11.1, md5 cd60f9b6…)
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
o = '''    Permissions {
        enabled: captureView.activeFocus
                    && camera.captureMode == Camera.CaptureStillImage
'''
n = '''    Permissions {
        // m52xq: never asks for the volume keys (system screenshot in the camera)
        enabled: false && captureView.activeFocus
                    && camera.captureMode == Camera.CaptureStillImage
'''
if n in s:
    print('already patched')
elif s.count(o) == 1:
    open(p, 'w', encoding='utf-8').write(s.replace(o, n))
    print('patched')
else:
    sys.exit('CaptureView.qml: block not found')
