
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

