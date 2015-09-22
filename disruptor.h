#ifndef DISRUPTOR_H
#define DISRUPTOR_H

struct disruptor;

struct disruptor_config
{
	unsigned int reader_count;
	
	unsigned int ele_size;

	/* element count must be a power of 2 */
	unsigned long ele_cnt;
};

struct disruptor*
disruptor_create(struct disruptor_config* cfg);

/* return reader id */
unsigned int
disruptor_add_reader(struct disruptor* dis);

void
disruptor_del_reader(struct disruptor* dis, unsigned int reader_id);

void
disruptor_read(struct disruptor* dis, unsigned int reader_id, void* dst);

void
disruptor_write(struct disruptor* dis, void* src);

void
disruptor_destroy(struct disruptor* dis);

#endif