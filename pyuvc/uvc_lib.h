#ifndef _UVC_LIB_H_
#define _UVC_LIB_H_

typedef struct {
	void *start;
	size_t length;
} v4l2_buffer_t;

typedef struct {
	int width;
	int height;
	int fmt_type;
	int fps;
	const char *dev;
} init_params;

#define CAP_ERROR_RET(s) {printf("uvc: %s\n", s);return -1;}
#define CLEAR(x) memset (&(x), 0, sizeof (x))
#define ALIGN_4K(x) (((x) + (4095)) & ~(4095))
#define ALIGN_16B(x) (((x) + (15)) & ~(15))
#define MMAP_BUFFERS 4

int v4l2_init_camera(int *_fd, init_params params, int *bufferSize);
int v4l2_get_frame_base(int fd, int *size, unsigned char *tmp_frame);
int v4l2_close_camera(int fd);
void v4l2_query_ctrl(int fd, char *info);
int v4l2_set_ctrl(int fd, int ctrl_id, int ctrl_value);
int v4l2_get_ctrl(int fd, int ctrl_id, int *ctrl_value);
int buffer_avaible(int fd);

#endif
