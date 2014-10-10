Carbon daemon in C
==================

This is a pointless and desperate implementation of Graphite carbon daemon in C
instead of the reference implementation in Python.

Hey but why the hell? Few reasons:

- C is clearly the programming language of the future, no doubt.
- For the sake of better performances and deep tuning.
- Before anything else, it's just for fun. And profit, then.

Feel free to open tickets on GitHub:

 http://github.com/rezib/carbond/issues/new

And send pull requests!

Install and run
---------------

```
./configure && make && make install
```

Tune `--prefix` at your convenience.

Licence
-------

carbond is distributed under the terms of the GNU General Public License
version 3.

Roadmap
-------

### `0.1`

* X TCP receiver thread
* X handle SIGINT, still have to make all threads block SIGINT signal
* X make multiple archive creation works
* X make propagate works
* X replace sizeof(whisper structs) with *_SIZE macros
* X write TODO inline function to calculate timestamp of higher precision archive
* X write TODO inline function to write data point
* X load aggregation rules from storage-aggregation.conf
* X merge all wait thread functions
* X create struct for manipuling threads
* X add network port numbers in configuration file
* _ Benchmarks!

### `0.2`

* _ code architecture documentation (with schemas)
* X better log/output utilities
* _ cache query thread
* _ multiple write at once function for whisper
* _ implement cache priority queue with heap
* _ implement exclusive mode to cache files offset
* _ make Python linked query thread optional at configure time
* _ implement foreground/daemon (with logging to syslog)
* _ handle SIGUP to reload configuration on the fly
* _ internal counters threads with its own metrics

### `0.3`

* _ unit tests
