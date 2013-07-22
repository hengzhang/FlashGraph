#ifndef __REMOTE_ACCESS_H__
#define __REMOTE_ACCESS_H__

#include "disk_read_thread.h"
#include "messaging.h"
#include "container.h"
#include "file_mapper.h"

class request_sender;

/**
 * This class is to help the local thread send IO requests to remote threads
 * dedicated to accessing SSDs. Each SSD has such a thread.
 * However, the helper class isn't thread safe, so each local thread has to
 * reference its own helper object.
 */
class remote_disk_access: public io_interface
{
	// They work as buffers for requests and are only used to
	// send high-priority requests.
	request_sender **senders;
	// They are used to send low-priority requests.
	request_sender **low_prio_senders;
	int num_senders;
	callback *cb;
	file_mapper *block_mapper;
	aio_complete_queue *complete_queue;

	int num_completed_reqs;

	remote_disk_access(int node_id): io_interface(node_id) {
		senders = NULL;
		low_prio_senders = NULL;
		num_senders = 0;
		cb = NULL;
		block_mapper = NULL;
		num_completed_reqs = 0;
	}
public:
	remote_disk_access(disk_read_thread **remotes,
			aio_complete_thread *complete_thread, int num_remotes,
			file_mapper *mapper, int node_id);

	~remote_disk_access();

	virtual int thread_init() {
		return 0;
	}

	virtual bool support_aio() {
		return true;
	}

	virtual void cleanup();

	virtual bool set_callback(callback *cb) {
		this->cb = cb;
		return true;
	}

	virtual callback *get_callback() {
		return cb;
	}

	virtual void access(io_request *requests, int num,
			io_status *status = NULL);
	virtual io_interface *clone() const;
	void flush_requests(int max_cached);
	virtual void flush_requests();
	virtual void print_stat() {
		printf("num completed reqs: %d\n", num_completed_reqs);
	}
};

#endif