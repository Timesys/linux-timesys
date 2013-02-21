#ifndef __MVF_SEMA4__
#define __MVF_SEMA4__

typedef struct mvf_sema4_handle_struct {
	int gate_num;
	int use_interrupts;
	wait_queue_head_t wait_queue;
} MVF_SEMA4;

int mvf_sema4_assign(int gate_num, bool use_interrupts, MVF_SEMA4** sema4_p);
int mvf_sema4_deassign(MVF_SEMA4 *sema4);
int mvf_sema4_lock(MVF_SEMA4 *sema4, unsigned int timeout_us);
int mvf_sema4_unlock(MVF_SEMA4 *sema4);

#endif
