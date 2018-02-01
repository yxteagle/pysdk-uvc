from distutils.core import setup
from distutils.core import Extension
from distutils.command.build_ext import build_ext as _build_ext


class build_ext(_build_ext):
    def run(self):
        print('WTF, Nothing but python is the best language for developers!')
        _build_ext.run(self)


modules = [

    Extension(
        'pyuvc.uvc',
        sources=['pyuvc/uvc.c', 'pyuvc/uvc_lib.c'],
        extra_compile_args=['-Wall', '-g'],
        ),

]

setup(

    name='pysdk-uvc',
    version='0.3',
    author='yxt',
    author_email='yxteagle@gmail.com',
    url='http://mail.google.com',
    license='BSD',
    packages=['pyuvc'],
    description='uvc driver for banana pi m2u',
    long_description=open('README.md').read() + open('CHANGES.txt').read(),
    classifiers=['Development Status :: 3 - Alpha',
                 'Environment :: Console',
                 'Intended Audience :: Developers',
                 'Intended Audience :: Education',
                 'License :: OSI Approved :: MIT License',
                 'Operating System :: POSIX :: Linux',
                 'Programming Language :: Python',
                 'Topic :: Home Automation',
                 'Topic :: Software Development :: Embedded Systems'
                 ],
    ext_modules=modules,
    cmdclass={'build_ext': build_ext}

)
