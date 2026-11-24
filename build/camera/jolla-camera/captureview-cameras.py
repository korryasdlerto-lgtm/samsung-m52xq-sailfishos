#!/usr/bin/python3
# m52xq: jolla-camera reads the list of cameras once, in LoadedStatus; also take it in ActiveStatus, so that the switch
# of the front/back camera and the labels of the back cameras appear. Usage: captureview-cameras.py <CaptureView.qml>
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
o = '''            if (cameraStatus === Camera.LoadedStatus && !initialized) {
                initialized = true
'''
n = '''            // m52xq: also in ActiveStatus, and not before the device list is there
            if ((cameraStatus === Camera.LoadedStatus || cameraStatus === Camera.ActiveStatus)
                    && !initialized && QtMultimedia.availableCameras.length > 0) {
                initialized = true
                for (var k = 0; k < QtMultimedia.availableCameras.length; k++) {
                    console.log("m52xq camera " + k + ": position " + QtMultimedia.availableCameras[k].position
                                + " id " + QtMultimedia.availableCameras[k].deviceId.split(" ").pop())
                }
'''
if n in s:
    print('already patched')
elif s.count(o) == 1:
    open(p, 'w', encoding='utf-8').write(s.replace(o, n))
    print('patched')
else:
    sys.exit('CaptureView.qml: block not found')
