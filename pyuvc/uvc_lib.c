#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include "uvc_lib.h"

v4l2_buffer_t *buffers = NULL;

int xioctl(int fd, int request, void *arg) {

	int r;
	int tries = 3;
	do {
		r = ioctl(fd, request, arg);
	} while (--tries > 0 && -1 == r && EINTR == errno);
	return r;
}

void _v4l2_display_pix_format(int fd) {

	struct v4l2_fmtdesc fmt;
	int index;
	printf("V4L2 pixel formats:\n");
	index = 0;
	CLEAR(fmt);
	fmt.index = index;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while (ioctl(fd, VIDIOC_ENUM_FMT, &fmt) != -1) {
		printf("%i: [0x%08X] '%c%c%c%c' (%s)\n",
				index,
				fmt.pixelformat,
				fmt.pixelformat >> 0,
				fmt.pixelformat >> 8,
				fmt.pixelformat >> 16,
				fmt.pixelformat >> 24,
				fmt.description
				);
		memset(&fmt, 0, sizeof(fmt));
		fmt.index = ++index;
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	}
}

void _v4l2_display_info(struct v4l2_capability *caps) {

	printf("Driver: \"%s\"\n", caps->driver);
	printf("Card: \"%s\"\n", caps->card);
	printf("Bus: \"%s\"\n", caps->bus_info);
	printf("Version: %d.%d\n", (caps->version >> 16) && 0xff, (caps->version >> 24) && 0xff);
	printf("Capabilities: %08x\n", caps->capabilities);
}

int _v4l2_set_mmap(int fd) {

	int i;
	int nbf;
	enum v4l2_buf_type type;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;

	CLEAR(req);
	req.count = MMAP_BUFFERS;
	req.memory = V4L2_MEMORY_MMAP;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
		CAP_ERROR_RET("failed requesting buffers.");
	}
	nbf = req.count;
	if (MMAP_BUFFERS != nbf) {
		CAP_ERROR_RET("insufficient buffer memory.");
	}

	buffers = (v4l2_buffer_t *) calloc(nbf, sizeof(v4l2_buffer_t));
	if (!buffers) {
		CAP_ERROR_RET("failed to allocated buffers memory.");
	}

	for (i = 0; i < nbf; i++) {
		CLEAR(buf);
		buf.index = i;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
			CAP_ERROR_RET("failed to query buffer.");
		}
		buffers[i].length = buf.length;
		buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, buf.m.offset);
		if (MAP_FAILED == buffers[i].start) {
			CAP_ERROR_RET("failed to mmap buffer.");
		}
	}

	for (i = 0; i < nbf; i++) {
		CLEAR(buf);
		buf.index = i;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
			CAP_ERROR_RET("failed to queue buffer.");
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
		CAP_ERROR_RET("failed to stream on.");
	}

	return 0;
}

int v4l2_init_camera(int *_fd, init_params params, int *bufferSize) {

	int fd;
	struct v4l2_streamparm parms;
	struct v4l2_format fmt;
	struct v4l2_input input;
	struct v4l2_capability caps;

	CLEAR(fmt);
	CLEAR(input);
	CLEAR(caps);

	fd = open(params.dev, O_RDWR | O_NONBLOCK);
	if (fd == -1) {
		CAP_ERROR_RET("failed to open the camera.");
	}

	if (xioctl(fd, VIDIOC_QUERYCAP, &caps) == -1) {
		CAP_ERROR_RET("unable to query capabilities.");
	}

	if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		CAP_ERROR_RET("doesn't support video capturing.");
	}

	if (!(caps.capabilities & V4L2_CAP_STREAMING)) {
		printf("warning, doesn't support V4L2_CAP_STREAMING");
	}

	_v4l2_display_info(&caps);

	input.index = 0;
	if (xioctl(fd, VIDIOC_ENUMINPUT, &input) == -1) {
		CAP_ERROR_RET("unable to enumerate input.");
	}

	printf("Input: %d\n", input.index);
	if (xioctl(fd, VIDIOC_S_INPUT, &input.index) == -1) {
		CAP_ERROR_RET("unable to set input.");
	}

	parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	//parms.parm.capture.capturemode = V4L2_MODE_HIGHQUALITY;
	parms.parm.capture.timeperframe.numerator = 1;
	parms.parm.capture.timeperframe.denominator = params.fps;
	if (-1 == xioctl(fd, VIDIOC_S_PARM, &parms)) {
		CAP_ERROR_RET("unable to set stream parm.");
	}

	_v4l2_display_pix_format(fd);

	fmt.fmt.pix.width = params.width;
	fmt.fmt.pix.height = params.height;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;        // V4L2_FIELD_ANY;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = params.fmt_type;

	if (xioctl(fd, VIDIOC_TRY_FMT, &fmt) == -1) {
		CAP_ERROR_RET("failed trying to set pixel format.");
	}

	if (fmt.fmt.pix.width != params.width
			|| fmt.fmt.pix.height != params.height) {
		CAP_ERROR_RET("failed to set pixel size.");
	}

	if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
		CAP_ERROR_RET("failed to set pixel format.");
	}

	if (_v4l2_set_mmap(fd) == -1) {
		CAP_ERROR_RET("failed to mmap.");
	}

	*bufferSize = fmt.fmt.pix.sizeimage;
	*_fd = fd;

	return 0;
}

int buffer_avaible(int fd){
	fd_set fds;
	struct timeval tv;
	int rc;
	CLEAR(tv);

	rc = 1;
	while (rc > 0) {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		rc = select(fd + 1, &fds, NULL, NULL, &tv);
		if (-1 == rc) {
			if (EINTR == errno) {
				rc = 1;         // try again
				continue;
			}
			CAP_ERROR_RET("failed to select frame.");
		}
		if (0 == rc) {
			CAP_ERROR_RET("select timeout.");
		}
		/* we got something */
		break;
	}
	if (rc <= 0) {
		CAP_ERROR_RET("failed to select frame.");
	}
	return 0;
}

int v4l2_get_frame_base(int fd, int *size, unsigned char *tmp_frame) {

	struct v4l2_buffer buf;
	CLEAR(buf);

	buf.memory = V4L2_MEMORY_MMAP;
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
		CAP_ERROR_RET("failed to retrieve frame.");
	}
	//printf("Length: %d \tBytesused: %d \tAddress: %p\n", buf.length, buf.bytesused, &buffers[buf.index]);
	memcpy(tmp_frame, buffers[buf.index].start, buf.bytesused);
	*size = buf.bytesused;

	if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
		CAP_ERROR_RET("failed to queue buffer.");
	}

	return 0;
}

int v4l2_close_camera(int fd) {

	int i;
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
		CAP_ERROR_RET("failed to stream off.");
	}
	for (i = 0; i < MMAP_BUFFERS; i++)
		munmap(buffers[i].start, buffers[i].length);
	close(fd);

	return 0;
}

int v4l2_get_ctrl(int fd, int ctrl_id, int *ctrl_value) {

	struct v4l2_control ctrl;
	ctrl.id = ctrl_id;
	if (xioctl(fd, VIDIOC_G_CTRL, &ctrl) == -1) {
		return -1;
	}
	*ctrl_value = ctrl.value;
	return 0;
}

void v4l2_query_ctrl(int fd, char *info) {

	char tmp[1024] = { 0 };
	int val;
	struct v4l2_queryctrl qctrl;
	qctrl.id = V4L2_CTRL_CLASS_USER | V4L2_CTRL_FLAG_NEXT_CTRL;
	strcat(info, "[");
	while (xioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {

		val = qctrl.id;
		v4l2_get_ctrl(fd, qctrl.id, &val);

		sprintf(tmp,
				"{\"id\": %d, \"name\": \"%s\", \"min\": %d, \"max\": %d, \"default\": %d, \"value\": %d, \"step\": %d, \"disabled\": %d},",
				qctrl.id, qctrl.name, qctrl.minimum, qctrl.maximum,
				qctrl.default_value, val, qctrl.step,
				qctrl.flags & V4L2_CTRL_FLAG_DISABLED);
		strcat(info, tmp);
		CLEAR(tmp);
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
	if (info[0] == 0) {
		strcat(info, "[]");
	} else {
		info[strlen(info) - 1] = ']';
	}
}

int v4l2_set_ctrl(int fd, int ctrl_id, int ctrl_value) {

	struct v4l2_control ctrl;
	ctrl.id = ctrl_id;
	ctrl.value = ctrl_value;
	if (xioctl(fd, VIDIOC_S_CTRL, &ctrl) == -1) {
		return -1;
	}
	return 0;
}
