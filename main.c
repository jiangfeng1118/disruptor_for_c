#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "disruptor.h"

static void*
reader_func(void* arg)
{
	unsigned long data;
	unsigned long id;
	struct disruptor* dis = (struct disruptor*)arg;

	id = disruptor_add_reader(dis);

	while (1)
	{
		disruptor_read(dis, id, &data);

		fprintf(stdout, "reader-%d : %ld\n", id, data);
	}

	return NULL;
}

static unsigned long data = 0;

static void*
writer_func(void* arg)
{
	unsigned long num;
	unsigned long pos;
	struct disruptor* dis = (struct disruptor*)arg;
	
	while (1)
	{
		num = __atomic_fetch_add(&data, 1, __ATOMIC_RELEASE);
		disruptor_write(dis, &num);
	}

	return NULL;
}

int main(int argc, char* argv[])
{
	struct disruptor* dis;
	struct disruptor_config cfg;

	pthread_t pt1, pt2, pt3, pt4;
	int pd1, pd2, pd3, pd4;

	cfg.ele_cnt = 1024;
	cfg.ele_size = 8;
	cfg.reader_count = 2;

	dis = disruptor_create(&cfg);
	if (NULL == dis)
	{
		return -1;
	}

	pthread_create(&pt1, NULL, reader_func, dis);
	pthread_create(&pt2, NULL, reader_func, dis);
	pthread_create(&pt3, NULL, writer_func, dis);
	pthread_create(&pt4, NULL, writer_func, dis);

	sleep(10000);

	return 0;
}