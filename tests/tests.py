import json
import os
import threading
from pyuvc import uvc
from time import sleep as test_sleep


try:
    import cv2
    USE_OPENCV = True
except:
    USE_OPENCV = False


def test_init():
    dev = '/dev/video0'
    params = {
        'width': 1280,
        'height': 720,
        'fps': 30,
        'fmt_type': uvc.MJPEG
        }
    print('Start capture: %s' % dev)
    r = uvc.open(
        fd=dev,
        **params
        )
    if r != 0:
        print('Open capture failed, test exit(%d)' % r)
        os._exit(0)
    print('buffer_size: ', uvc.get_buffer_size())


def test_close():
    uvc.close()


def test_video_t_daemon():
    if USE_OPENCV:
        def video():
            while True:
                try:
                    tmp_img = '/dev/shm/t_img.jpg'
                    with open(tmp_img, 'wb') as f:
                        f.write(uvc.get_frame())
                    _img = cv2.imread(tmp_img)
                    # img = cv2.resize(_img, (1280, 720))
                    cv2.namedWindow("test")
                    cv2.imshow("test", _img)
                    cv2.waitKey(1)
                except Exception as e:
                    print(e)
        t = threading.Thread(target=video)
        t.setDaemon(True)
        t.start()
    else:
        print('Opencv not found, video test exit')


def test_ctrl():
    test_id = 9963776
    test_min = 0
    test_max = 0
    test_default = 0
    print('Test get ctrl enum')
    for i in json.loads(uvc.query_ctrl()):
        print(i)
        if i['id'] == test_id:
            test_min = i['min']
            test_max = i['max']
            test_default = i['default']
    for i in range(test_min, test_max):
        uvc.set_ctrl(test_id, i)
        test_sleep(0.05)
        test_value = uvc.get_ctrl(test_id)
        print('Test set ctrl successful, (%d, %d)' % (test_id, test_value))
        if test_value != i:
            print('Test catupre option error, set(%d), get(%s)' % (
                i, str(test_value)))
    uvc.set_ctrl(test_id, test_default)


def test_pipe():
    write_path = "/share/data.pipe"
    try:
        os.mkfifo(write_path)
    except:
        pass
    f = os.open(write_path, os.O_SYNC | os.O_CREAT | os.O_RDWR)
    while True:
        os.write(f, uvc.get_frame())
        test_sleep(0.05)
    os.close(f)


if __name__ == '__main__':
    test_init()
    test_video_t_daemon()
    test_ctrl()
    # test_pipe()
    # test_sleep(10)
    test_close()
