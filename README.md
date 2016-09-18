stackcollapse-perf.cpp
======================

Another [stackcollapse-perf.pl](https://github.com/brendangregg/FlameGraph/blob/master/stackcollapse-perf.pl) implemented in C++ for performance.

Build & Install
---------------

```sh
make
sudo make install
```

Usage
-----

No command-line options. Just use in a pipe.

```sh
perf script | stackcollapse-perf | flamegraph.pl > foo.svg
```
