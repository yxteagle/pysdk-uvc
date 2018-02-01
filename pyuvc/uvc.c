#include <Python.h>
#include <linux/videodev2.h>

#include "uvc_lib.h"

#define PYUVC_INFO_OK 0
#define PYUVC_ERROR_DEV -1
#define PYUVC_ERROR_PARAMS -2


#define PYSDK_VERSION "PYUVC version: 0.3"


int fd;
int width = 1920;
int height = 1080;
int fps = 30;
int fmt_type = V4L2_PIX_FMT_MJPEG;
int capture_opend = 0;
int buffer_size = 0;
unsigned char *tmp_frame = NULL;

static PyObject* get_frame_without_select(PyObject* self) {

	if (!capture_opend) {
		Py_RETURN_NONE;
	}

	int ret, size;
	ret = v4l2_get_frame_base(fd, &size, tmp_frame);
	if (ret != 0) {
		Py_RETURN_NONE;
	}

	PyObject *frame = 0;
	frame = PyBytes_FromStringAndSize((const char *)tmp_frame, size);

	return frame;
}

static PyObject* get_frame(PyObject* self) {

	if (!capture_opend) {
		Py_RETURN_NONE;
	}

	int ret;
	ret = buffer_avaible(fd);
	if (ret != 0) {
		Py_RETURN_NONE;
	}

	int size;
	ret = v4l2_get_frame_base(fd, &size, tmp_frame);
	if (ret != 0) {
		Py_RETURN_NONE;
	}

	PyObject *frame = 0;
	//frame = PyMemoryView_FromMemory((void*) tmp_frame, size, PyBUF_READ);
	frame = PyBytes_FromStringAndSize((const char *)tmp_frame, size);

	return frame;
}

static PyObject* pyuvc_open(PyObject* self, PyObject* args, PyObject* kwargs) {

	if (capture_opend) {
		return Py_BuildValue("i", PYUVC_ERROR_DEV);
	}

	printf(PYSDK_VERSION);
	printf("\n");
	char *dev;
	static char *kwlist[] = { "fd", "width", "height", "fps", "fmt_type", NULL };
	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|iiii", kwlist, &dev, &width, &height, &fps, &fmt_type)) {
		return Py_BuildValue("i", PYUVC_ERROR_PARAMS);
	}

	printf("frame size: %d*%d\n", width, height);
	printf("frame rate: %d\n", fps);
	printf("fmt type: %d\n", fmt_type);

	init_params params;
	params.dev = dev;
	params.fmt_type = fmt_type;
	params.width = width;
	params.height = height;
	params.fps = fps;
	if (v4l2_init_camera(&fd, params, &buffer_size) == -1) {
		printf("failed to init camera.\n");
		return Py_BuildValue("i", PYUVC_ERROR_DEV);
	}

	tmp_frame = calloc(ALIGN_16B(width) * height * 3, sizeof(unsigned char));
	if (!tmp_frame) {
		printf("failed to malloc buffer.\n");
		return Py_BuildValue("i", PYUVC_ERROR_DEV);
	}

	capture_opend = 1;

	return Py_BuildValue("i", PYUVC_INFO_OK);
}

static PyObject* pyuvc_close(PyObject* self) {

	capture_opend = 0;
	v4l2_close_camera(fd);
	free(tmp_frame);
	printf("Capture closed.\n");

	return (PyObject*) Py_BuildValue("i", PYUVC_INFO_OK);
}

static PyObject* query_ctrl(PyObject* self) {

	char info[65536] = { 0 };
	v4l2_query_ctrl(fd, info);

	return (PyObject*) Py_BuildValue("s", info);
}

static PyObject* set_ctrl(PyObject* self, PyObject* args) {

	int ctrl_id;
	int ctrl_value;

	if (!PyArg_ParseTuple(args, "ii", &ctrl_id, &ctrl_value)) {
		return (PyObject*) Py_BuildValue("i", PYUVC_ERROR_PARAMS);
	}
	if (v4l2_set_ctrl(fd, ctrl_id, ctrl_value)) {
		return (PyObject*) Py_BuildValue("i", PYUVC_ERROR_DEV);
	} else {
		return (PyObject*) Py_BuildValue("i", PYUVC_INFO_OK);
	}
}

static PyObject* get_ctrl(PyObject* self, PyObject* args) {

	int ctrl_id;
	int ctrl_value;

	if (!PyArg_ParseTuple(args, "i", &ctrl_id)) {
		return (PyObject*) Py_BuildValue("i", PYUVC_ERROR_PARAMS);
	}
	if (v4l2_get_ctrl(fd, ctrl_id, &ctrl_value)) {
		Py_RETURN_NONE;
	} else {
		return (PyObject*) Py_BuildValue("i", ctrl_value);
	}
}

static PyObject* get_fd(PyObject* self, PyObject* args) {

	return (PyObject*) Py_BuildValue("i", fd);
}

static PyObject* get_buffer_size(PyObject* self, PyObject* args) {

	return (PyObject*) Py_BuildValue("i", buffer_size);
}

static PyMethodDef module_methods[] = {
		{ "open", (void*) pyuvc_open, METH_VARARGS | METH_KEYWORDS },
		{ "close", (void*) pyuvc_close, METH_NOARGS },
		{ "get_frame_without_select", (void*) get_frame_without_select, METH_NOARGS },
		{ "get_frame", (void*) get_frame, METH_NOARGS },
		{ "query_ctrl", (void*) query_ctrl, METH_NOARGS },
		{ "set_ctrl", (void*) set_ctrl, METH_VARARGS },
		{ "get_ctrl", (void*) get_ctrl,	METH_VARARGS },
		{ "get_fd", (void*) get_fd, METH_NOARGS },
		{ "get_buffer_size", (void*) get_buffer_size, METH_NOARGS },
		{ NULL, NULL, 0, NULL }
};

static struct PyModuleDef module_def = { PyModuleDef_HEAD_INIT, "uvc", NULL, -1, module_methods };

PyMODINIT_FUNC PyInit_uvc(void) {

	PyObject* module = NULL;
	module = PyModule_Create(&module_def);
	if (module == NULL)
		return NULL;

	PyModule_AddObject(module, "MJPEG", Py_BuildValue("i", V4L2_PIX_FMT_MJPEG));
	PyModule_AddObject(module, "H264", Py_BuildValue("i", V4L2_PIX_FMT_H264));
	PyModule_AddObject(module, "NV12", Py_BuildValue("i", V4L2_PIX_FMT_NV12));
	PyModule_AddObject(module, "INFO_OK", Py_BuildValue("i", PYUVC_INFO_OK));
	PyModule_AddObject(module, "ERR_DEV", Py_BuildValue("i", PYUVC_ERROR_DEV));
	PyModule_AddObject(module, "ERR_PARAMS", Py_BuildValue("i", PYUVC_ERROR_PARAMS));

	return module;
}
