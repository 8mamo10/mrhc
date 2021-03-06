# information for debug

## build apr
```
$ tar xzvf apr-1.7.0.tar.gz
$ cd apr-1.7.0
$ CFLAGS="-g" ./configure
$ make
$ sudo make install
```

## build apr-util
```
$ tar xzvf apr-util-1.6.1.tar.gz
$ cd apr-util-1.6.1
$ CFLAGS="-g" ./configure --with-apr=/usr/local/apr
$ make
$ sudo make install
$ sudo ln -s /usr/local/apr/bin/apu-1-config /usr/bin/apu-1-config
```

## build apache
```
$ tar xzvf httpd-2.4.41.tar.gz
$ cd httpd-2.4.41
$ CFLAGS="-g" ./configure # add -g option for gdb debug
$ make
$ sudo make install
```

## run apache
```
$ /usr/local/apache2/bin/httpd -v
Server version: Apache/2.4.41 (Unix)
Server built:   Jan 19 2020 20:20:22
$ sudo /usr/local/apache2/bin/httpd -k start
```

## init apache module
```
$ /usr/local/apache2/bin/apxs -g -n mrhc
$ cd mrhc
$ /usr/local/apache2/bin/apxs -c mod_mrhc.c
$ sudo /usr/local/apache2/bin/apxs -i -a -c mod_mrhc.c
```

## build opencv (opencv4)
```
$ sudo apt install cmake git libgtk2.0-dev pkg-config libavcodec-dev libavformat-dev libswscale-dev
$ cd opencv
$ mkdir build
$ cd build
$ cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=/usr/local
$ make
$ sudo make install
```

```
$ sudo mkdir /usr/local/lib/pkgconfig
$ sudo cp lib/pkgconfig/opencv4.pc /usr/local/lib/pkgconfig/
$ echo "export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig" >> ~/.zshrc
```

## gdb

```
$ sudo gdb /usr/local/apache2/bin/httpd
(gdb) b mrhc_handler
Function "mrhc_handler" not defined.
Make breakpoint pending on future shared library load? (y or [n]) y
Breakpoint 1 (mrhc_handler) pending.
(gdb) r -X # run by single process
Starting program: /usr/local/apache2/bin/httpd -X
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
warning: Temporarily disabling breakpoints for unloaded shared library "/usr/local/apache2/modules/mod_mrhc.so"
[New Thread 0x7ffff340e700 (LWP 24571)]
[New Thread 0x7ffff2c0d700 (LWP 24572)]
[New Thread 0x7ffff240c700 (LWP 24573)]
[New Thread 0x7ffff1c0b700 (LWP 24574)]
[New Thread 0x7ffff140a700 (LWP 24575)]
[New Thread 0x7ffff0c09700 (LWP 24576)]
[New Thread 0x7fffebfff700 (LWP 24577)]
[New Thread 0x7fffeb7fe700 (LWP 24578)]
[New Thread 0x7fffeaffd700 (LWP 24579)]
[New Thread 0x7fffea7fc700 (LWP 24580)]
[New Thread 0x7fffe9ffb700 (LWP 24581)]
[New Thread 0x7fffe97fa700 (LWP 24582)]
[New Thread 0x7fffe8ff9700 (LWP 24583)]
[New Thread 0x7fffe87f8700 (LWP 24584)]
[New Thread 0x7fffe7ff7700 (LWP 24585)]
[New Thread 0x7fffe77f6700 (LWP 24586)]
[New Thread 0x7fffe6ff5700 (LWP 24587)]
[New Thread 0x7fffe67f4700 (LWP 24588)]
[New Thread 0x7fffe5ff3700 (LWP 24589)]
[New Thread 0x7fffe57f2700 (LWP 24590)]
[New Thread 0x7fffe4ff1700 (LWP 24591)]
[New Thread 0x7fffe47f0700 (LWP 24592)]
[New Thread 0x7fffe3fef700 (LWP 24593)]
[New Thread 0x7fffe37ee700 (LWP 24594)]
[New Thread 0x7fffe2fed700 (LWP 24595)]
[New Thread 0x7fffe27ec700 (LWP 24596)]
[New Thread 0x7fffe1feb700 (LWP 24597)]
[Thread 0x7ffff340e700 (LWP 24571) exited]
```

```
$ wget http://[ip]/mrhc
```

```
[Switching to Thread 0x7ffff2c0d700 (LWP 24572)]

Thread 3 "httpd" hit Breakpoint 1, mrhc_handler (r=0x7ffff7ef70a0) at mod_mrhc.c:48
48          if (strcmp(r->handler, "mrhc")) {
(gdb)
(gdb) set print pretty on
```

Deal with coredump.  
```
$ sudo apport-unpack /var/crash/_usr_sbin_apache2.33.crash ./tmp/
$ gdb /usr/sbin/apache2 ./tmp/CoreDump
```

## google test
```
$ tar xzvf release-1.10.0.tar.gz
$ cd googletest-release-1.10.0
$ mkdir build
$ cd build
$ cmake ..
$ make
$ sudo make install
```
