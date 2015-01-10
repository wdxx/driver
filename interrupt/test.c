#include <stdio.h>

struct test_desc{
	int val;
	int *pd;
};

static int val[4]={0,0,0,0};

static struct test_desc my_test[4]={
	{1, &val[0]},
	{2, &val[1]},
	{3, &val[2]},
	{4, &val[3]},
		
}; 

int main(void)
{
	int i;
	(*(my_test[0].pd))++;
	(*(my_test[0].pd))++;
	(*(my_test[0].pd))++;

	(*(my_test[1].pd))++;
	(*(my_test[2].pd))++;
	(*(my_test[2].pd))++;

	for (i = 0; i < 4; i++)
		printf("val[%d] = %d\n", i, val[i]);
}

