stackcollapse-perf.cpp
======================

Another [stackcollapse-perf.pl](https://github.com/brendangregg/FlameGraph/blob/master/stackcollapse-perf.pl) implemented in C++ for performance

 - Faster
 - Less features
   - No command-line options
   - No signatures clean-ups (so the results may differ)


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
