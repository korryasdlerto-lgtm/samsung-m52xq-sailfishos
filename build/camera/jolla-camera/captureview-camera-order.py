#!/usr/bin/python3
# m52xq: the labels of the back cameras are shown as 0.5 / 1.0 / M (the list itself keeps the main camera first: it is the default).
# Usage: captureview-camera-order.py <CaptureView.qml>   (after captureview-cameras.py)
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
o = '''                backFacingCameras = backCameras
'''
n = '''                // m52xq: order on the screen: ultra wide, main, macro
                var rank = function(d) { return d.deviceId.indexOf("imx258") >= 0 ? 0 : d.deviceId.indexOf("s5kgw3") >= 0 ? 1 : 2 }
                backFacingCameras = backCameras.slice().sort(function(a, b) { return rank(a) - rank(b) })
'''
if n in s:
    print('already patched')
elif s.count(o) == 1:
    open(p, 'w', encoding='utf-8').write(s.replace(o, n))
    print('patched')
else:
    sys.exit('CaptureView.qml: block not found')
