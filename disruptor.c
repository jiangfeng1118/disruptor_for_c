#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "disruptor.h"

#define MAX_READER_COUNT 128

#define INV_POS (unsigned long)-1

struct disruptor
{
	unsigned long write_pos;
	unsigned long slowest_read_pos;
	unsigned long max_readable_pos;
	unsigned long read_pos[MAX_READER_COUNT];
	unsigned int reader_cnt;

	char* data_buf;
	unsigned int data_item_size;

	/* data_item_count must be a power of 2 */
	unsigned long data_item_count;
	unsigned long mask;
};

struct disruptor*
	disruptor_create(struct disruptor_config* cfg)
{
	int i;
	long size;
	struct disruptor* dis;

	if (cfg->reader_count > MAX_READER_COUNT)
	{
		return NULL;
	}

	dis = (struct disruptor*)malloc(sizeof(*dis));
	if (NULL == dis)
	{
		return NULL;
	}
	memset(dis, 0, sizeof(*dis));

	size = cfg->ele_cnt * cfg->ele_size;

	dis->data_buf = (char*)malloc(size);
	if (NULL == dis->data_buf)
	{
		goto err_exit;
	}

	dis->data_item_count = cfg->ele_cnt;
	dis->mask = cfg->ele_cnt - 1;
	dis->data_item_size = cfg->ele_size;
	dis->reader_cnt = cfg->reader_count;

	for (i = 0; i < dis->reader_cnt; i++)
	{
		dis->read_pos[i] = INV_POS;
	}
	
	return dis;
err_exit:
	if (dis->data_buf != NULL)
	{
		free(dis->data_buf);
	}

	if (dis != NULL)
	{
		free(dis);
	}

	return NULL;
}

/* return reader id */
unsigned int
disruptor_add_reader(struct disruptor* dis)
{
	int i;
	unsigned long inv_pos = INV_POS;

	do 
	{
		for (i = 0; i < dis->reader_cnt; i++)
		{
			if (__atomic_compare_exchange_n(&dis->read_pos[i],				// type* ptr
				&inv_pos,													// type* expected
				__atomic_load_n(&dis->slowest_read_pos, __ATOMIC_CONSUME),	// type* desired
				1,															// weak
				__ATOMIC_RELEASE,											// success_memorder
				__ATOMIC_RELAXED))											// failure_memorder
			{
				goto out;
			}

			inv_pos = INV_POS;
		}
	} while (1);

out:
	/* set init read_pos 1 if it's 0. */
	if (0 == dis->read_pos[i])
	{
		__atomic_store_n(&dis->read_pos[i], 1, __ATOMIC_RELEASE);
	}

	return i;
}

void
disruptor_del_reader(struct disruptor* dis, unsigned int reader_id)
{
	__atomic_store_n(&dis->read_pos[reader_id], INV_POS, __ATOMIC_RELAXED);
}

void
disruptor_read(struct disruptor* dis, unsigned int reader_id, void* dst)
{
	unsigned long cur_pos;

	cur_pos = dis->read_pos[reader_id];

	while (cur_pos > __atomic_load_n(&dis->max_readable_pos, __ATOMIC_RELAXED))
	{
		usleep(1);
	}

	memcpy(dst, dis->data_buf + (cur_pos & dis->mask) * dis->data_item_size, dis->data_item_size);
	cur_pos++;

	__atomic_store_n(&dis->read_pos[reader_id], cur_pos, __ATOMIC_RELAXED);
}

unsigned long
disruptor_next_write_pos(struct disruptor* dis)
{
	unsigned int i;
	unsigned long slowest_read_pos;
	unsigned long pos;
	unsigned long next_write_pos;

	next_write_pos = 1 + __atomic_fetch_add(&dis->write_pos, 1, __ATOMIC_RELAXED);

	do 
	{
		/* get the slowest reader */
		slowest_read_pos = INV_POS;
		for (i = 0; i < dis->reader_cnt; i++)
		{
			pos = __atomic_load_n(&dis->read_pos[i], __ATOMIC_RELAXED);
			if (pos < slowest_read_pos)
			{
				slowest_read_pos = pos;
			}
		}

		/* all readers exit, or no reader join in */
		if (INV_POS == slowest_read_pos)
		{
			slowest_read_pos = next_write_pos - (dis->mask & next_write_pos);
		}

		if ((next_write_pos - slowest_read_pos) <= dis->mask)
		{
			return next_write_pos;
		}

		usleep(1);
	} while (1);
}

void
disruptor_write_done(struct disruptor* dis, unsigned long pos)
{
	unsigned long max_readable_pos = pos - 1;

	while (__atomic_load_n(&dis->max_readable_pos, __ATOMIC_RELAXED) != max_readable_pos)
	{
		usleep(1);
	}

	__atomic_fetch_add(&dis->max_readable_pos, 1, __ATOMIC_RELEASE);
}

void
disruptor_write(struct disruptor* dis, void* src)
{
	unsigned long pos;

	pos = disruptor_next_write_pos(dis);

	memcpy(dis->data_buf + (pos & dis->mask) * dis->data_item_size, src, dis->data_item_size);

	disruptor_write_done(dis, pos);
}

void
disruptor_destroy(struct disruptor* dis)
{
	free(dis->data_buf);
	free(dis);
}