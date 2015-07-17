# Protocol format

Latest information seems to be [https://github.com/b/statsd_spec](here).

This is a line-based protocol.

    key:value|type

Types are:

* g - gauge, instantaneous value
* c - counter, inc/dec, also supports rate (|@rate)
* ms - timer
* h - histogram, alias for timer
* m - meter, rate

A key with no other information should be treated as a meter with value of 1.


