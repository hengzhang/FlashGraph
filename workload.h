#ifndef __WORKLOAD_H__
#define __WORKLOAD_H__

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>

#include <string>
#include <deque>

#include "cache.h"

#define CHUNK_SLOTS 1024

typedef struct workload_type
{
	off_t off;
	int size: 31;
	int read: 1;
} workload_t;

class workload_gen
{
	workload_t access;
	static int default_entry_size;
	static int default_access_method;
public:
	static void set_default_entry_size(int entry_size) {
		default_entry_size = entry_size;
	}
	static void set_default_access_method(int access_method) {
		default_access_method = access_method;
	}
	static int get_default_access_method() {
		return default_access_method;
	}

	workload_gen() {
		default_entry_size = PAGE_SIZE;
		default_access_method = READ;
		memset(&access, 0, sizeof(access));
	}

	/**
	 * This is a wrapper to the original interface `next_offset'
	 * so more info of an access is provided.
	 */
	virtual const workload_t &next() {
		assert(default_access_method >= 0);
		access.off = next_offset();
		access.size = default_entry_size;
		access.read = default_access_method == READ;
		return access;
	}

	/**
	 * The enxt offset in bytes.
	 */
	virtual off_t next_offset() = 0;
	virtual bool has_next() = 0;
	virtual ~workload_gen() { }
};

class seq_workload: public workload_gen
{
	long start;
	long end;
	long cur;
	int entry_size;

public:
	seq_workload(long start, long end, int entry_size) {
		this->start = start;
		this->end = end;
		this->cur = start;
		this->entry_size = entry_size;
	}

	off_t next_offset() {
		off_t next = cur;
		cur++;
		return next * entry_size;
	}

	bool has_next() {
		return cur < end;
	}
};

class rand_permute
{
	off_t *offset;
	long num;

public:
	/**
	 * @start: the index of the first entry.
	 */
	rand_permute(long num, int stride, long start) {
		offset = (off_t *) valloc(num * sizeof(off_t));
		for (int i = 0; i < num; i++) {
			offset[i] = ((off_t) i) * stride + start * stride;
		}

		for (int i = num - 1; i >= 1; i--) {
			int j = random() % i;
			off_t tmp = offset[j];
			offset[j] = offset[i];
			offset[i] = tmp;
		}
	}

	~rand_permute() {
		free(offset);
	}

	off_t get_offset(long idx) const {
		return offset[idx];
	}
};

class global_rand_permute_workload: public workload_gen
{
	long start;
	long end;
	const rand_permute *permute;
public:
	/**
	 * @start: the index of the first entry.
	 * @end: the index of the last entry.
	 */
	global_rand_permute_workload(int stride, long start, long end) {
		permute = new rand_permute(end - start, stride, start);
		this->start = 0;
		this->end = end - start;
	}

	virtual ~global_rand_permute_workload() {
		if (permute) {
			delete permute;
			permute = NULL;
		}
	}

	off_t next_offset() {
		if (start >= end)
			return -1;
		return permute->get_offset(start++);
	}

	bool has_next() {
		return start < end;
	}
};

/**
 * In this workload generator, the expected cache hit ratio
 * can be defined by the user.
 */
class cache_hit_defined_workload: public global_rand_permute_workload
{
	long num_pages;
	double cache_hit_ratio;
	std::deque<off_t> cached_pages;
	long seq;				// the sequence number of accesses
	long cache_hit_seq;		// the sequence number of cache hits
public:
	cache_hit_defined_workload(int stride, long start, long end,
			long cache_size, double ratio): global_rand_permute_workload(stride,
				start, end) {
		// only to access the most recent pages.
		this->num_pages = cache_size / PAGE_SIZE / 100;
		cache_hit_ratio = ratio;
		seq = 0;
		cache_hit_seq = 0;
	}

	off_t next_offset();
};

off_t *load_java_dump(const std::string &file, long &num_offsets);
workload_t *load_file_workload(const std::string &file, long &num);

/* this class reads workload from a file dumped by a Java program. */
class java_dump_workload: public workload_gen
{
	off_t *offsets;
	long curr;
	long end;

public:
	java_dump_workload(off_t offsets[], long length, long start, long end) {
		assert(length >= end);
		this->offsets = (off_t *) valloc((end - start) * sizeof(off_t));
		memcpy(this->offsets, &offsets[start], sizeof(off_t) * (end - start));
		this->end = end - start;
		this->curr = 0;
		printf("start at %ld end at %ld\n", curr, end);
	}

	virtual ~java_dump_workload() {
		free(offsets);
	}

	off_t next_offset() {
		/*
		 * the data in the file is generated by a Java program,
		 * its byte order is different from Intel architectures.
		 */
		return swap_bytesl(offsets[curr++]);
	}

	bool has_next() {
		return curr < end;
	}

	long static swap_bytesl(long num) {
		long res;

		char *src = (char *) &num;
		char *dst = (char *) &res;
		for (unsigned int i = 0; i < sizeof(long); i++) {
			dst[sizeof(long) - 1 - i] = src[i];
		}
		return res;
	}
};

class file_workload: public workload_gen
{
	workload_t *workloads;
	long curr;
	long end;
public:
	file_workload(workload_t workloads[], long length, long start, long end) {
		assert(length >= end);
		this->workloads = (workload_t *) valloc((end - start)
				* sizeof(workload_t));
		memcpy(this->workloads, &workloads[start],
				sizeof(workload_t) * (end - start));
		this->end = end - start;
		this->curr = 0;
		printf("start at %ld end at %ld\n", curr, end);
	}

	virtual ~file_workload() {
		free(workloads);
	}

	const workload_t &next() {
		if (get_default_access_method() >= 0)
			workloads[curr].read = get_default_access_method() == READ;
		return workloads[curr++];
	}

	off_t next_offset() {
		return workloads[curr++].off;
	}

	bool has_next() {
		return curr < end;
	}

	/**
	 * The remaining number of accesses.
	 */
	int size() const {
		return end - curr;
	}
};

class rand_workload: public workload_gen
{
	long start;
	long range;
	long num;
	off_t *offsets;
public:
	rand_workload(long start, long end, int stride) {
		this->start = start;
		this->range = end - start;
		num = 0;
		offsets = (off_t *) valloc(sizeof(*offsets) * range);
		for (int i = 0; i < range; i++) {
			offsets[i] = (start + random() % range) * stride;
		}
	}

	virtual ~rand_workload() {
		free(offsets);
	}

	off_t next_offset() {
		return offsets[num++];
	}

	bool has_next() {
		return num < range;
	}
};

class workload_chunk
{
public:
	virtual bool get_workload(off_t *, int num) = 0;
	virtual ~workload_chunk() { }
};

class stride_workload_chunk: public workload_chunk
{
	long first;	// the first entry
	long last;	// the last entry but it's not included in the range
	long curr;	// the current location
	int stride;
	int entry_size;
	pthread_spinlock_t _lock;
public:
	stride_workload_chunk(long first, long last, int entry_size) {
		pthread_spin_init(&_lock, PTHREAD_PROCESS_PRIVATE);
		this->first = first;
		this->last = last;
		this->entry_size = entry_size;
		printf("first: %ld, last: %ld\n", first, last);
		curr = first;
		stride = PAGE_SIZE / entry_size;
	}

	virtual ~stride_workload_chunk() {
		pthread_spin_destroy(&_lock);
	}

	bool get_workload(off_t *offsets, int num);
};

class balanced_workload: public workload_gen
{
	off_t offsets[CHUNK_SLOTS];
	int curr;
	static workload_chunk *chunks;
public:
	balanced_workload(workload_chunk *chunks) {
		memset(offsets, 0, sizeof(offsets));
		curr = CHUNK_SLOTS;
		this->chunks = chunks;
	}

	virtual ~balanced_workload() {
		if (chunks) {
			delete chunks;
			chunks = NULL;
		}
	}

	off_t next_offset() {
		return offsets[curr++];
	}

	bool has_next() {
		if (curr < CHUNK_SLOTS)
			return true;
		else {
			bool ret = chunks->get_workload(offsets, CHUNK_SLOTS);
			curr = 0;
			return ret;
		}
	}
};

#endif
