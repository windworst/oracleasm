
/* Silly io_request_lock manageablility */
#define IO_REQUEST_LOCK(q)	spin_lock_irq((q)->queue_lock)
#define IO_REQUEST_UNLOCK(q)	spin_unlock_irq((q)->queue_lock)

/* Avoid linker conflicts */
#define get_request		o_get_request
#define __get_request_wait	o___get_request_wait
#define get_request_wait	o_get_request_wait


/*
 * Get a free request. io_request_lock must be held and interrupts
 * disabled on the way in.
 */
static inline struct request *get_request(request_queue_t *q, int rw)
{
	struct request *rq = NULL;

	if (!list_empty(&q->request_freelist[rw])) {
		rq = blkdev_free_rq(&q->request_freelist[rw]);
		list_del(&rq->table);
		rq->rq_status = RQ_ACTIVE;
		rq->special = NULL;
		rq->q = q;
	}

	return rq;
}

/*
 * No available requests for this queue, unplug the device.
 */
static struct request *__get_request_wait(request_queue_t *q, int rw)
{
	register struct request *rq;
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue_exclusive(&q->wait_for_request, &wait);
	for (;;) {
		__set_current_state(TASK_UNINTERRUPTIBLE);
		spin_lock_irq(q->queue_lock);
		rq = get_request(q, rw);
		spin_unlock_irq(q->queue_lock);
		if (rq)
			break;
		generic_unplug_device(q);
		schedule();
	}
	remove_wait_queue(&q->wait_for_request, &wait);
	current->state = TASK_RUNNING;
	return rq;
}

static inline struct request *get_request_wait(request_queue_t *q, int rw)
{
	register struct request *rq;

	spin_lock_irq(q->queue_lock);
	rq = get_request(q, rw);
	spin_unlock_irq(q->queue_lock);
	if (rq)
		return rq;
	return __get_request_wait(q, rw);
}

/*
 * add-request adds a request to the linked list.
 * io_request_lock is held and interrupts disabled, as we muck with the
 * request queue list.
 *
 * By this point, req->cmd is always either READ/WRITE, never READA,
 * which is important for drive_stat_acct() above.
 */
static inline void add_request(request_queue_t * q, struct request * req,
			       struct list_head *insert_here)
{
#if 0
	drive_stat_acct(req->rq_dev, req->cmd, req->nr_sectors, 1);
#endif

	if (!q->plugged && q->head_active && insert_here == &q->queue_head) {
		spin_unlock_irq(q->queue_lock);
		BUG();
	}

	/*
	 * elevator indicated where it wants this request to be
	 * inserted at elevator_merge time
	 */
	list_add(&req->queue, insert_here);
}
