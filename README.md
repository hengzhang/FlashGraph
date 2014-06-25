FlashGraph
===========

FlashGraph is an [SSD](http://en.wikipedia.org/wiki/Solid-state_drive)-based
semi-external memory graph processing engine. FlashGraph stores vertex state
in memory and the edge lists of vertices on SSDs. It is optimized for a high-speed
SSD array or other high-speed flash storage. Its current implementation is tightly
integrated with `SAFS` (Set Associative File System) to take advantage of
the high I/O throughput of an SSD array. Due to the natural of semi-external memory
graph engine, FlashGraph has very short load time and small memory consumption
when processing very large graphs, which enables us to process a billion-node
graph in a single machine. Thanks to the high-speed SSDs, it also has performance
comparable to or exceed in-memory graph engines such as [PowerGraph](http://graphlab.org/).

Flashgraph is implemented with C++ and provides a vertex-centric programming
interface for users to express graph algorithms. Users encapsulate their graph
algorithms in vertex programs and `FlashGraph` executes the vertex programs
in parallel and fetches data from SSDs for the vertex programs.

[FlashGraph Quickstart](https://github.com/icoming/FlashGraph/wiki/FlashGraph-Quick-Start-Guide)

[FlashGraph User manual](https://github.com/icoming/FlashGraph/wiki/User-manual-of-FlashGraph).

SAFSlib
========

`SAFSlib` is an open-source library that implements the design described
in the paper "[Toward Millions of File System IOPS on Low-Cost, Commodity Hardware](http://dl.acm.org/citation.cfm?id=2503225&dl=ACM&coll=DL&CFID=350399128&CFTOKEN=49883861)".
SAFS is designed as a standalone library. It provides a filesystem-like interface
in the userspace to users to access an SSD array. It is designed to eliminate overhead
in the block stack, without modifying the kernel. It can achieve the optimal performance
of a large SSD array in a [NUMA](http://en.wikipedia.org/wiki/Non-uniform_memory_access) machine.
FlashGraph is an application to demonstrate the power of SAFS.

[SAFS user manual](https://docs.google.com/document/d/1OpsuLZw60MGCZAg4xO-j-1_AEWm3Yc2nqKKu8kXotkA/edit?usp=sharing).