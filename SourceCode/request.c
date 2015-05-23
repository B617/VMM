#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include "vmm.h"

CMD cmd;
Ptr_MemoryAccessRequest ptr_memAccReq=&(cmd.request);

int main(){
	char c;
	int fd;
	srandom(time(NULL));
	/* 在循环中模拟访存请求与处理过程 */
	while (TRUE)
	{
		bzero(&cmd,DATALEN);
		printf("按Y打印页表，按V打印辅存,按A打印实存\n按C手动产生请求,按N随机产生新请求,按X退出程序...\n");
		c = getchar();
		if(c == 'n' || c == 'N'){
			cmd.c = 'n';
			do_request();
		}
		else if(c == 'c' || c == 'C'){
			cmd.c = 'c';
			create_request();
		}
		else
			cmd.c = c;

		if((fd = open("/tmp/server",O_WRONLY)) < 0)
		{
			printf("enq open fifo failed");
			exit(1);
		}

		if(write(fd,&cmd,DATALEN) < 0)
		{
			printf("enq write failed");
			exit(1);
		}
//		printf("ok\n");

		close(fd);
		if(c == 'x' || c == 'X')
			break;
		while((c = getchar()) != '\n');
	}
	return 0;
}
/* 产生访存请求 */
void do_request()
{
	/* 随机产生请求地址 */
	ptr_memAccReq->virAddr = random() % VIRTUAL_MEMORY_SIZE;
	/* 随机产生请求进程号 */
	ptr_memAccReq->processNum = random() % PROCESS_SUM;
	/* 随机产生请求类型 */
	switch (random() % 3)
	{
		case 0: //读请求
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("产生请求：\n进程号：%u\t地址：%u\t类型：读取\n", 
					ptr_memAccReq->processNum, ptr_memAccReq->virAddr);
			break;
		}
		case 1: //写请求
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* 随机产生待写入的值 */
			ptr_memAccReq->value = random() % 0xFFu;
//			printf("%c\n",ptr_memAccReq->value);
			printf("产生请求：\n进程号：%u\t地址：%u\t类型：写入\t值：%02X\n",
					ptr_memAccReq->processNum, ptr_memAccReq->virAddr, ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("产生请求：\n进程号：%u\t地址：%u\t类型：执行\n", 
					ptr_memAccReq->processNum, ptr_memAccReq->virAddr);
			break;
		}
		default:
			break;
	}	
}

/* 手动产生访存请求 */
void create_request()
{
	unsigned long addr;
	int type;
	BYTE value;

	/* 产生请求地址 */
	printf("请输入请求地址[0,%u)...\n",VIRTUAL_MEMORY_SIZE);
	scanf("%u",&ptr_memAccReq->virAddr);
	/* 产生请求进程号 */
	printf("请输入请求进程号[0,%u)...\n",PROCESS_SUM);
	scanf("%d",&ptr_memAccReq->processNum);
	/* 产生请求类型 */
	printf("请输入请求类型(0:read\t1:write\t2:execute)...\n");
	scanf("%d",&type);
	switch (type%3)
	{
		case 0: //读请求
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("产生请求：\n进程号：%u\t地址：%u\t类型：读取\n", 
					ptr_memAccReq->processNum, ptr_memAccReq->virAddr);
			break;
		}
		case 1: //写请求
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* 产生待写入的值 */
			printf("请输入待写入的值...\n");
			scanf("%02x",&ptr_memAccReq->value);
			printf("产生请求：\n进程号：%u\t地址：%u\t类型：写入\t值：%02X\n",
					ptr_memAccReq->processNum, ptr_memAccReq->virAddr, ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("产生请求：\n进程号：%u\t地址：%u\t类型：执行\n", 
					ptr_memAccReq->processNum, ptr_memAccReq->virAddr);
			break;
		}
		default:
			break;
	}	
}
