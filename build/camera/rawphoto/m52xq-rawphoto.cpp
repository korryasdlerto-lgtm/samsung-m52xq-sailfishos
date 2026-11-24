// SPDX-License-Identifier: GPL-2.0-or-later
// m52xq-rawphoto: full-resolution photo of the Samsung Galaxy M52 5G (main camera s5kgw3 9248x6936 = 64 MP, front camera
// imx616 6528x4896 = 32 MP). Port of Worker::captureRaw() and PlasmaCameraManager raw development of the m52xq
// plasma-camera build (r36, plasma-camera-0015-m52xq-full-resolution-raw-photo.patch) without Qt, for applications that
// get the camera through PipeWire (Snapshot): the application stops its viewfinder, runs this helper, starts it again.
//
//   m52xq-rawphoto --camera main|front --out FILE.jpg [--orientation 1..8]
//
// The exposure of the sensor (set by the viewfinder auto exposure) is kept: converted to the line time of the full mode,
// restored afterwards. The 2nd frame is taken (the 1st may still have the exposure of the stream start). The white
// balance and the brightness are measured on the frame itself (m52xq_raw_develop), the development runs on the GPU.

#include <libcamera/libcamera.h>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <dirent.h>
#include <fcntl.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

#include "rawphoto.h"

using namespace libcamera;

static double now_ms()
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// the V4L2 sub-device of the sensor (main s5kgw3 = camera@2d, front imx616 = camera@1a), as in plasma-camera
static int open_sensor_subdev(bool main_cam)
{
	const char *name = main_cam ? "s5kgw3" : "imx616";
	DIR *d = opendir("/sys/class/video4linux");
	if (!d)
		return -1;
	int fd = -1;
	while (struct dirent *e = readdir(d)) {
		if (strncmp(e->d_name, "v4l-subdev", 10) != 0)
			continue;
		char path[256], buf[64] = "";
		snprintf(path, sizeof(path), "/sys/class/video4linux/%s/name", e->d_name);
		FILE *f = fopen(path, "r");
		if (!f)
			continue;
		if (!fgets(buf, sizeof(buf), f))
			buf[0] = 0;
		fclose(f);
		if (strncmp(buf, name, strlen(name)) == 0) {
			snprintf(path, sizeof(path), "/dev/%s", e->d_name);
			fd = open(path, O_RDWR);
			break;
		}
	}
	closedir(d);
	return fd;
}

static bool subdev_ctrl(int fd, unsigned id, long long *value, bool set)
{
	struct v4l2_ext_control c = {};
	c.id = id;
	if (set) {
		if (id == V4L2_CID_PIXEL_RATE)
			c.value64 = *value;
		else
			c.value = static_cast<int>(*value);
	}
	struct v4l2_ext_controls cs = {};
	cs.which = V4L2_CTRL_WHICH_CUR_VAL;
	cs.count = 1;
	cs.controls = &c;
	if (ioctl(fd, set ? VIDIOC_S_EXT_CTRLS : VIDIOC_G_EXT_CTRLS, &cs) < 0)
		return false;
	if (!set)
		*value = id == V4L2_CID_PIXEL_RATE ? c.value64 : c.value;
	return true;
}

static long long ctrl_max(int fd, unsigned id)
{
	struct v4l2_query_ext_ctrl q = {};
	q.id = id;
	if (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &q) < 0)
		return 0;
	return q.maximum;
}

// line time of the present sensor mode in microseconds: (width + HBLANK) / PIXEL_RATE
static double sensor_line_us(int fd)
{
	if (fd < 0)
		return 0.0;
	struct v4l2_subdev_format fmt = {};
	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	fmt.pad = 0;
	long long hblank = 0, rate = 0;
	if (ioctl(fd, VIDIOC_SUBDEV_G_FMT, &fmt) < 0 || !subdev_ctrl(fd, V4L2_CID_HBLANK, &hblank, false) ||
	    !subdev_ctrl(fd, V4L2_CID_PIXEL_RATE, &rate, false) || rate <= 0)
		return 0.0;
	return (fmt.format.width + hblank) * 1e6 / static_cast<double>(rate);
}

struct Capture {
	Stream *stream = nullptr;
	std::map<FrameBuffer *, std::pair<void *, size_t>> maps;
	std::mutex mutex;
	std::condition_variable cv;
	int frames = 0;
	std::vector<uint8_t> data;

	void complete(Request *request)
	{
		// libcamera thread
		if (request->status() == Request::RequestCancelled || !stream)
			return;
		std::lock_guard<std::mutex> lock(mutex);
		++frames;
		// the 1st frame may still have the exposure of the stream start: take the 2nd one
		if (frames == 2 && request->buffers().count(stream)) {
			FrameBuffer *buffer = request->buffers().at(stream);
			auto it = maps.find(buffer);
			const size_t used = buffer->metadata().planes()[0].bytesused;
			if (it != maps.end() && it->second.first && used <= it->second.second) {
				const uint8_t *p = static_cast<const uint8_t *>(it->second.first) + buffer->planes()[0].offset;
				data.assign(p, p + used);
			}
			cv.notify_all();
		}
	}
};

int main(int argc, char **argv)
{
	bool main_cam = true;
	const char *out = nullptr;
	int orientation = 1;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--camera") && i + 1 < argc) {
			main_cam = strcmp(argv[++i], "front") != 0;
		} else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
			out = argv[++i];
		} else if (!strcmp(argv[i], "--orientation") && i + 1 < argc) {
			orientation = atoi(argv[++i]);
			if (orientation < 1 || orientation > 8)
				orientation = 1;
		}
	}
	if (!out) {
		fprintf(stderr, "usage: m52xq-rawphoto --camera main|front --out FILE.jpg [--orientation 1..8]\n");
		return 2;
	}
	const double t0 = now_ms();
	// by the devicetree path, or (sandbox without /sys/firmware: Sailfish) by the sensor name
	const char *match = main_cam ? "camera@2d" : "camera@1a";
	const char *match2 = main_cam ? "s5kgw3" : "imx616";
	const Size full = main_cam ? Size(9248, 6936) : Size(6528, 4896);

	// the exposure time must stay the same in the full mode (other line time)
	const int sfd = open_sensor_subdev(main_cam);
	long long exp_prev = 0, gain = 0;
	const double lt_prev = sensor_line_us(sfd);
	long long gain_max = 0;
	if (sfd >= 0) {
		subdev_ctrl(sfd, V4L2_CID_EXPOSURE, &exp_prev, false);
		subdev_ctrl(sfd, V4L2_CID_ANALOGUE_GAIN, &gain, false);
		gain_max = ctrl_max(sfd, V4L2_CID_ANALOGUE_GAIN);
	}
	// how dark the scene is (sensor gain towards its maximum): stronger noise reduction, formula of plasma-camera
	double iso_frac = gain_max > 0 ? double(gain) / double(gain_max) : 0.0;
	float dark = float(std::min(1.0, std::max(0.0, (iso_frac - 0.25) / 0.65)));
	fprintf(stderr, "m52xq-rawphoto: %s camera, exposure %lld lines (line %.2f us), gain %lld/%lld, dark %.2f\n",
		main_cam ? "main" : "front", exp_prev, lt_prev, gain, gain_max, dark);

	// the application has just stopped its viewfinder: PipeWire releases the camera a moment later, try for 4 s
	std::unique_ptr<CameraManager> cm;
	std::shared_ptr<Camera> cam;
	for (int attempt = 0; attempt < 20; attempt++) {
		cm = std::make_unique<CameraManager>();
		if (cm->start() == 0) {
			for (const auto &c : cm->cameras())
				if (c->id().find(match) != std::string::npos || c->id().find(match2) != std::string::npos)
					cam = c;
			if (cam && cam->acquire() == 0)
				break;
			cam.reset();
			cm->stop();
		}
		cm.reset();
		usleep(200000);
	}
	if (!cam) {
		fprintf(stderr, "m52xq-rawphoto: camera %s not found or busy\n", match);
		return 1;
	}
	fprintf(stderr, "m52xq-rawphoto: camera acquired after %.0f ms\n", now_ms() - t0);

	Capture cap;
	int width = 0, height = 0, stride = 0;
	std::string cfa;
	std::unique_ptr<CameraConfiguration> cfg = cam->generateConfiguration({ StreamRole::Raw });
	bool ok = false;
	if (cfg && !cfg->empty()) {
		StreamConfiguration &sc = cfg->at(0);
		sc.size = full;
		sc.bufferCount = 3;
		if (cfg->validate() != CameraConfiguration::Invalid && cam->configure(cfg.get()) == 0) {
			width = static_cast<int>(sc.size.width);
			height = static_cast<int>(sc.size.height);
			stride = static_cast<int>(sc.stride);
			const std::string fmt = sc.pixelFormat.toString(); // e.g. SGRBG10_CSI2P
			cfa = fmt.size() > 5 && fmt[0] == 'S' ? fmt.substr(1, 4) : "";
			fprintf(stderr, "m52xq-rawphoto: raw capture %s stride %d\n", sc.toString().c_str(), stride);

			cap.stream = sc.stream();
			FrameBufferAllocator allocator(cam);
			std::vector<std::unique_ptr<Request>> requests;
			if (allocator.allocate(cap.stream) > 0) {
				for (const std::unique_ptr<FrameBuffer> &buffer : allocator.buffers(cap.stream)) {
					const FrameBuffer::Plane &plane = buffer->planes()[0];
					const size_t len = plane.offset + plane.length;
					void *m = mmap(nullptr, len, PROT_READ, MAP_SHARED, plane.fd.get(), 0);
					cap.maps[buffer.get()] = { m == MAP_FAILED ? nullptr : m, len };
					std::unique_ptr<Request> request = cam->createRequest();
					if (request && request->addBuffer(cap.stream, buffer.get()) == 0)
						requests.push_back(std::move(request));
				}
			}
			const double lt_full = sensor_line_us(sfd);
			if (sfd >= 0 && exp_prev > 0 && lt_prev > 0 && lt_full > 0) {
				long long e = std::llround(exp_prev * lt_prev / lt_full);
				subdev_ctrl(sfd, V4L2_CID_EXPOSURE, &e, true);
				fprintf(stderr, "m52xq-rawphoto: exposure %lld -> %lld lines (line %.2f -> %.2f us)\n", exp_prev, e,
					lt_prev, lt_full);
			}
			cam->requestCompleted.connect(&cap, &Capture::complete);
			if (!requests.empty() && cam->start() == 0) {
				for (auto &r : requests)
					cam->queueRequest(r.get());
				std::unique_lock<std::mutex> lock(cap.mutex);
				cap.cv.wait_for(lock, std::chrono::seconds(4), [&cap]() { return !cap.data.empty(); });
				lock.unlock();
				cam->stop();
			}
			cam->requestCompleted.disconnect(&cap);
			requests.clear();
			for (auto &m : cap.maps)
				if (m.second.first)
					munmap(m.second.first, m.second.second);
			cap.maps.clear();
			allocator.free(cap.stream);
			cap.stream = nullptr;
			ok = !cap.data.empty();
		} else {
			fprintf(stderr, "m52xq-rawphoto: raw configuration %s not accepted\n", sc.toString().c_str());
		}
	}
	cam->release();
	cam.reset();
	cm->stop();

	// back to the viewfinder exposure
	if (sfd >= 0) {
		if (exp_prev > 0)
			subdev_ctrl(sfd, V4L2_CID_EXPOSURE, &exp_prev, true);
		close(sfd);
	}
	const double t_cap = now_ms();
	fprintf(stderr, "m52xq-rawphoto: raw capture %s %zu bytes in %.0f ms\n", ok ? "done" : "FAILED", cap.data.size(),
		t_cap - t0);
	if (!ok)
		return 1;
	// the camera is free again: the application can start its viewfinder while the frame is developed
	printf("M52XQ_CAPTURED\n");
	fflush(stdout);
	// the application reads only that line and may close the pipe: everything else goes to stderr, and a closed pipe
	// must not kill the helper before the JPEG is written (r2 died of SIGPIPE on the printf of photo_gpu)
	signal(SIGPIPE, SIG_IGN);
	dup2(2, 1);

	// after the re-mosaic the main camera frame is a plain GRBG Bayer pattern
	const std::string cfa_dev = main_cam ? "GRBG" : cfa;
	ok = m52xq_raw_develop(cap.data.data(), width, height, stride, cfa_dev.c_str(), main_cam, orientation, dark, !main_cam,
			       out);
	fprintf(stderr, "m52xq-rawphoto: development %s in %.0f ms, total %.0f ms: %s\n", ok ? "done" : "FAILED",
		now_ms() - t_cap, now_ms() - t0, out);
	return ok ? 0 : 1;
}
