#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "vmm.h"

//#define WRITE

/* 页表 */
//PageTableItem pageTable[PAGE_SUM];
/* 二级页表 */
PageTableItem pageTable[ROOT_PAGE_SUM][SUB_PAGE_SUM];
/* 实存空间 */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* 用文件模拟辅存空间 */
FILE *ptr_auxMem;
/* 物理块使用标识 */
BOOL blockStatus[BLOCK_SUM];
/* 访存请求 */
Ptr_MemoryAccessRequest ptr_memAccReq;
/* FIFO */
int fifo;

extern int errno;

/* 初始化环境 */
void do_init()
{
	int i, j ,k;
	unsigned long auxAddr=0;
	srandom(time(NULL));
	for (i=0; i < ROOT_PAGE_SUM; i++)
	{
		for (j = 0; j < SUB_PAGE_SUM; j++)
		{
			pageTable[i][j].pageNum = j;
			pageTable[i][j].filled = FALSE;
			pageTable[i][j].edited = FALSE;
			pageTable[i][j].count = 0;
			pageTable[i][j].processNum = random() % PROCESS_SUM;
			/* 使用随机数设置该页的保护类型 */
			switch (random() % 7)
			{
				case 0:
				{
					pageTable[i][j].proType = READABLE;
					break;
				}
				case 1:
				{
					pageTable[i][j].proType = WRITABLE;
					break;
				}
				case 2:
				{
					pageTable[i][j].proType = EXECUTABLE;
					break;
				}
				case 3:
				{
					pageTable[i][j].proType = READABLE | WRITABLE;
					break;
				}
				case 4:
				{
					pageTable[i][j].proType = READABLE | EXECUTABLE;
					break;
				}
				case 5:
				{
					pageTable[i][j].proType = WRITABLE | EXECUTABLE;
					break;
				}
				case 6:
				{
					pageTable[i][j].proType = READABLE | WRITABLE | EXECUTABLE;
					break;
				}
				default:
					break;
			}
			/* 设置该页对应的辅存地址 */
			pageTable[i][j].auxAddr = auxAddr;
			auxAddr += PAGE_SIZE;				//@remove *2
		}
	}
	for (k = 0; k < BLOCK_SUM; k++)
	{
		/* 随机选择一些物理块进行页面装入 */
		if (random() % 2 == 0)
		{
			i=random() % ROOT_PAGE_SUM;
			j=random() % SUB_PAGE_SUM;
			do_page_in(&pageTable[i][j], k);
			pageTable[i][j].blockNum = k;
			pageTable[i][j].filled = TRUE;
			blockStatus[k] = TRUE;
		}
		else
			blockStatus[k] = FALSE;
	}
}

/* 初始化文件 */
void initFile(){
	int i;
	char *key="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	char buffer[VIRTUAL_MEMORY_SIZE+1];
	
	ptr_auxMem=fopen(AUXILIARY_MEMORY,"w+");
	for(i=0;i<VIRTUAL_MEMORY_SIZE;i++){
		buffer[i]=key[rand()%62];
	}
	buffer[VIRTUAL_MEMORY_SIZE]=0;
	
	fwrite(buffer,sizeof(BYTE),VIRTUAL_MEMORY_SIZE,ptr_auxMem);
	printf("系统提示：初始化辅存模拟文件成功\n");
	fclose(ptr_auxMem);
}


/* 响应请求 */
void do_response()
{
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int rootPageNum, subPageNum, offAddr, temp;
	unsigned int actAddr;
	
	/* 检查地址是否越界 */
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}
	
	/* 检查进程是否存在 */
	if (ptr_memAccReq->processNum < 0 || ptr_memAccReq->processNum >= PROCESS_SUM)
	{
		do_error(ERROR_PROCESS_NOT_FOUND);
		return;
	}
	
	/* 计算页号和页内偏移值 */
	temp = PAGE_SIZE * SUB_PAGE_SUM;
	rootPageNum = ptr_memAccReq->virAddr / temp;
	subPageNum = ptr_memAccReq->virAddr % temp / PAGE_SIZE;
	offAddr = ptr_memAccReq->virAddr % PAGE_SIZE;
	printf("一级页号为：%u\t二级页号为：%u\t页内偏移为：%u\n", rootPageNum, subPageNum, offAddr);

	/* 获取对应页表项 */
	ptr_pageTabIt = &pageTable[rootPageNum][subPageNum];

	/* 检查进程是否匹配 */
	if (ptr_memAccReq->processNum != ptr_pageTabIt->processNum)
	{
		do_error(ERROR_PROCESS_PROTECTED);
		return ;
	}
	
	/* 根据特征位决定是否产生缺页中断 */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}
	
	actAddr = ptr_pageTabIt->blockNum * PAGE_SIZE + offAddr;
	printf("实地址为：%u\n", actAddr);
	
	/* 检查页面访问权限并处理访存请求 */
	switch (ptr_memAccReq->reqType)
	{
		case REQUEST_READ: //读请求
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & READABLE)) //页面不可读
			{
				do_error(ERROR_READ_DENY);
				return;
			}
			/* 读取实存中的内容 */
			printf("读操作成功：值为%02X\n", actMem[actAddr]);
			break;
		}
		case REQUEST_WRITE: //写请求
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & WRITABLE)) //页面不可写
			{
				do_error(ERROR_WRITE_DENY);	
				return;
			}
			/* 向实存中写入请求的内容 */
			#ifdef WRITE
				do_print_actMem();
			#endif
			actMem[actAddr] = ptr_memAccReq->value;
			#ifdef WRITE
				do_print_actMem();
			#endif
			ptr_pageTabIt->edited = TRUE;			
			printf("写操作成功\n");
			break;
		}
		case REQUEST_EXECUTE: //执行请求
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & EXECUTABLE)) //页面不可执行
			{
				do_error(ERROR_EXECUTE_DENY);
				return;
			}			
			printf("执行成功\n");
			break;
		}
		default: //非法请求类型
		{	
			do_error(ERROR_INVALID_REQUEST);
			return;
		}
	}
}

/* 处理缺页中断 */
void do_page_fault(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i;
	printf("产生缺页中断，开始进行调页...\n");
	for (i = 0; i < BLOCK_SUM; i++)
	{
		if (!blockStatus[i])
		{
			/* 读辅存内容，写入到实存 */
			do_page_in(ptr_pageTabIt, i);
			
			/* 更新页表内容 */
			ptr_pageTabIt->blockNum = i;
			ptr_pageTabIt->filled = TRUE;
			ptr_pageTabIt->edited = FALSE;
			ptr_pageTabIt->count = 0;
			
			blockStatus[i] = TRUE;
			return;
		}
	}
	/* 没有空闲物理块，进行页面替换 */
	do_LFU(ptr_pageTabIt);
}

/* 根据LFU算法进行页面替换 */
void do_LFU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i, j, min, page_i, page_j;
	printf("没有空闲物理块，开始进行LFU页面替换...\n");
	for (i = 0, min = 0xFFFFFFFF, page_i = 0, page_j = 0; i < ROOT_PAGE_SUM; i++)
	{
		for(j = 0; j < SUB_PAGE_SUM; j++)
		{
			if (pageTable[i][j].filled=TRUE && pageTable[i][j].count < min)
			{
				min = pageTable[i][j].count;
				page_i = i;
				page_j = j;
			}
		}
	}
	printf("选择一级页表第%u项,二级页表第%u项进行替换\n", page_i, page_j);
	if (pageTable[page_i][page_j].edited)
	{
		/* 页面内容有修改，需要写回至辅存 */
		printf("该页内容有修改，写回至辅存\n");
		do_page_out(&pageTable[page_i][page_j]);
	}
	pageTable[page_i][page_j].filled = FALSE;
	pageTable[page_i][page_j].count = 0;


	/* 读辅存内容，写入到实存 */
	do_page_in(ptr_pageTabIt, pageTable[page_i][page_j].blockNum);
	
	/* 更新页表内容 */
	ptr_pageTabIt->blockNum = pageTable[page_i][page_j].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	printf("页面替换成功\n");
}

/* 将辅存内容写入实存 */
void do_page_in(Ptr_PageTableItem ptr_pageTabIt, unsigned int blockNum)
{
	unsigned int readNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)//从文件头偏移
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((readNum = fread(actMem + blockNum * PAGE_SIZE, 
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: blockNum=%u\treadNum=%u\n", blockNum, readNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_READ_FAILED);
		exit(1);
	}
	printf("调页成功：辅存地址%u-->>物理块%u\n", ptr_pageTabIt->auxAddr, blockNum);
}

/* 将被替换页面的内容写回辅存 */
void do_page_out(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int writeNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((writeNum = fwrite(actMem + ptr_pageTabIt->blockNum * PAGE_SIZE, 
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%u\tftell=%u\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: writeNum=%u\n", writeNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_WRITE_FAILED);
		exit(1);
	}
	printf("写回成功：物理块%u-->>辅存地址%03X\n", ptr_pageTabIt->auxAddr, ptr_pageTabIt->blockNum);
}

/* 错误处理 */
void do_error(ERROR_CODE code)
{
	switch (code)
	{
		case ERROR_READ_DENY:
		{
			printf("访存失败：该地址内容不可读\n");
			break;
		}
		case ERROR_WRITE_DENY:
		{
			printf("访存失败：该地址内容不可写\n");
			break;
		}
		case ERROR_EXECUTE_DENY:
		{
			printf("访存失败：该地址内容不可执行\n");
			break;
		}		
		case ERROR_INVALID_REQUEST:
		{
			printf("访存失败：非法访存请求\n");
			break;
		}
		case ERROR_OVER_BOUNDARY:
		{
			printf("访存失败：地址越界\n");
			break;
		}
		case ERROR_FILE_OPEN_FAILED:
		{
			printf("系统错误：打开文件失败\n");
			break;
		}
		case ERROR_FILE_CLOSE_FAILED:
		{
			printf("系统错误：关闭文件失败\n");
			break;
		}
		case ERROR_FILE_SEEK_FAILED:
		{
			printf("系统错误：文件指针定位失败\n");
			break;
		}
		case ERROR_FILE_READ_FAILED:
		{
			printf("系统错误：读取文件失败\n");
			break;
		}
		case ERROR_FILE_WRITE_FAILED:
		{
			printf("系统错误：写入文件失败\n");
			break;
		}
		case ERROR_PROCESS_NOT_FOUND:
		{
			printf("系统错误：进程不存在\n");
			break;
		}
		case ERROR_PROCESS_PROTECTED:
		{
			printf("系统错误：进程访问受限\n");
			break;
		}
		case ERROR_FIFO_REMOVE_FAILED:
		{
			printf("fifo文件删除失败\n");
			break;
		}
		case ERROR_FIFO_MAKE_FAILED:
		{
			printf("fifo文件创建失败\n");
			break;
		}
		case ERROR_FIFO_OPEN_FAILED:
		{
			printf("fifo文件打开失败\n");
			break;
		}
		case ERROR_FIFO_READ_FAILED:
		{
			printf("fifo文件读取失败\n");
			break;
		}
		default:
		{
			printf("未知错误：没有这个错误代码\n");
		}
	}
}

/* 打印页表 */
void do_print_info()
{
	unsigned int i, j;
	char str[4];
	printf("1级页号\t2级页号\t进程号\t块号\t装入\t修改\t保护\t计数\t辅存\n");
	for (i = 0; i < ROOT_PAGE_SUM; i++)
	{
		for(j = 0; j < SUB_PAGE_SUM; j++)
		{
			printf("%u\t%u\t%u\t%u\t%u\t%u\t%s\t%u\t%u\n", i, j, pageTable[i][j].processNum, pageTable[i][j].blockNum, 
				pageTable[i][j].filled, pageTable[i][j].edited, get_proType_str(str, pageTable[i][j].proType), 
				pageTable[i][j].count, pageTable[i][j].auxAddr);
		}
	}
}

/* 打印辅存相关信息 */
void do_print_auxMem()
{
	int i,j,k,readNum,p;
	BYTE temp[VIRTUAL_MEMORY_SIZE];
	if (fseek(ptr_auxMem, 0, SEEK_SET) < 0)//从文件头偏移
	{
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((readNum = fread(temp, 
		sizeof(BYTE), VIRTUAL_MEMORY_SIZE, ptr_auxMem)) < VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_FILE_READ_FAILED);
		exit(1);
	}
	printf("1级页号\t2级页号\t辅存\t内容\t\n");
	for(i=0,k=0;i<ROOT_PAGE_SUM;i++)
	{
		for(p=0;p<SUB_PAGE_SUM;p++)
		{
			printf("%d\t%d\t%d\t",i,p,k);
			for(j=0;j<PAGE_SIZE;j++){
				printf("%02x ",temp[k++]);
			}
			printf("\n");
		}
	}
}

/* 打印实存相关信息 */
void do_print_actMem()
{
	int i,j,k;
	printf("页号\t内容\t\n");
	for(i=0,k=0;i<BLOCK_SUM;i++){
		printf("%d\t",i);
		if(blockStatus[i]==TRUE){
			for(j=0;j<PAGE_SIZE;j++)		
				printf("%02x ",actMem[k++]);
		}
		printf("\n");
	}
}

/* 获取页面保护类型字符串 */
char *get_proType_str(char *str, BYTE type)
{
	if (type & READABLE)
		str[0] = 'r';
	else
		str[0] = '-';
	if (type & WRITABLE)
		str[1] = 'w';
	else
		str[1] = '-';
	if (type & EXECUTABLE)
		str[2] = 'x';
	else
		str[2] = '-';
	str[3] = '\0';
	return str;
}

int main(int argc, char* argv[])
{
	char c;
	int i,count;
	CMD cmd;
	struct stat statbuf;
	
	initFile();
	if (!(ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+")))
	{
		do_error(ERROR_FILE_OPEN_FAILED);
		exit(1);
	}
	do_init();
	do_print_info();
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));

	if(stat("/tmp/server",&statbuf)==0)//通过文件名获取文件信息，并保存在statbuf结构体中
	{
		/* 如果FIFO文件存在,删掉 */
		if(remove("/tmp/server")<0)
		{
			do_error(ERROR_FIFO_REMOVE_FAILED);
			exit(1);
		}
	}

	if(mkfifo("/tmp/server",0666)<0)//mkfifo ()会依参数pathname建立特殊的FIFO文件，该文件必须不存在，而参数mode为该文件的权限
	{
		do_error(ERROR_FIFO_MAKE_FAILED);
		exit(1);
	}
	/* 在阻塞模式下打开FIFO */
//|O_NONBLOCK
	if((fifo=open("/tmp/server",O_RDONLY))<0)
	{
		do_error(ERROR_FIFO_OPEN_FAILED );
		exit(1);
	}

	while(TRUE)
	{
		bzero(&cmd,DATALEN);
//		sleep(5);
		if((count=read(fifo,&cmd,DATALEN))<0)
		{
			do_error(ERROR_FIFO_READ_FAILED);
			printf("errno=%d\n",errno);
			exit(1);
		}
		if(count==0)
		{
			continue;
		}
		c=cmd.c;
		if (c == 'y' || c == 'Y')
			do_print_info();
		else if(c == 'v' || c == 'V')
			do_print_auxMem();
		else if(c == 'a' || c == 'A')
			do_print_actMem();
		else if(c == 'n' || c == 'N'){
			ptr_memAccReq=&(cmd.request);
			do_response();
		}
		else if(c == 'c' || c == 'C'){
			ptr_memAccReq=&(cmd.request);
			do_response();
		}
		else if(c == 'x' || c == 'X')
			break;
//		sleep(1);
//		printf("sleep over\n");
	}

	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	close(fifo);
	return (0);
}
