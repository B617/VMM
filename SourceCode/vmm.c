#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "vmm.h"

/* ҳ�� */
PageTableItem pageTable[PAGE_SUM];
/* ʵ��ռ� */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* ���ļ�ģ�⸨��ռ� */
FILE *ptr_auxMem;
/* �����ʹ�ñ�ʶ */
BOOL blockStatus[BLOCK_SUM];
/* �ô����� */
Ptr_MemoryAccessRequest ptr_memAccReq;



/* ��ʼ������ */
void do_init()
{
	int i, j;
	srandom(time(NULL));
	for (i = 0; i < PAGE_SUM; i++)
	{
		pageTable[i].pageNum = i;
		pageTable[i].filled = FALSE;
		pageTable[i].edited = FALSE;
		pageTable[i].count = 0;
		/* ʹ����������ø�ҳ�ı������� */
		switch (random() % 7)
		{
			case 0:
			{
				pageTable[i].proType = READABLE;
				break;
			}
			case 1:
			{
				pageTable[i].proType = WRITABLE;
				break;
			}
			case 2:
			{
				pageTable[i].proType = EXECUTABLE;
				break;
			}
			case 3:
			{
				pageTable[i].proType = READABLE | WRITABLE;
				break;
			}
			case 4:
			{
				pageTable[i].proType = READABLE | EXECUTABLE;
				break;
			}
			case 5:
			{
				pageTable[i].proType = WRITABLE | EXECUTABLE;
				break;
			}
			case 6:
			{
				pageTable[i].proType = READABLE | WRITABLE | EXECUTABLE;
				break;
			}
			default:
				break;
		}
		/* ���ø�ҳ��Ӧ�ĸ����ַ */
		pageTable[i].auxAddr = i * PAGE_SIZE * 2;
	}
	for (j = 0; j < BLOCK_SUM; j++)
	{
		/* ���ѡ��һЩ��������ҳ��װ�� */
		if (random() % 2 == 0)
		{
			do_page_in(&pageTable[j], j);
			pageTable[j].blockNum = j;
			pageTable[j].filled = TRUE;
			blockStatus[j] = TRUE;
		}
		else
			blockStatus[j] = FALSE;
	}
}


/* ��Ӧ���� */
void do_response()
{
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int pageNum, offAddr;
	unsigned int actAddr;
	
	/* ����ַ�Ƿ�Խ�� */
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}
	
	/* ����ҳ�ź�ҳ��ƫ��ֵ */
	pageNum = ptr_memAccReq->virAddr / PAGE_SIZE;
	offAddr = ptr_memAccReq->virAddr % PAGE_SIZE;
	printf("ҳ��Ϊ��%u\tҳ��ƫ��Ϊ��%u\n", pageNum, offAddr);

	/* ��ȡ��Ӧҳ���� */
	ptr_pageTabIt = &pageTable[pageNum];
	
	/* ��������λ�����Ƿ����ȱҳ�ж� */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}
	
	actAddr = ptr_pageTabIt->blockNum * PAGE_SIZE + offAddr;
	printf("ʵ��ַΪ��%u\n", actAddr);
	
	/* ���ҳ�����Ȩ�޲�����ô����� */
	switch (ptr_memAccReq->reqType)
	{
		case REQUEST_READ: //������
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & READABLE)) //ҳ�治�ɶ�
			{
				do_error(ERROR_READ_DENY);
				return;
			}
			/* ��ȡʵ���е����� */
			printf("�������ɹ���ֵΪ%02X\n", actMem[actAddr]);
			break;
		}
		case REQUEST_WRITE: //д����
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & WRITABLE)) //ҳ�治��д
			{
				do_error(ERROR_WRITE_DENY);	
				return;
			}
			/* ��ʵ����д����������� */
			actMem[actAddr] = ptr_memAccReq->value;
			ptr_pageTabIt->edited = TRUE;			
			printf("д�����ɹ�\n");
			break;
		}
		case REQUEST_EXECUTE: //ִ������
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & EXECUTABLE)) //ҳ�治��ִ��
			{
				do_error(ERROR_EXECUTE_DENY);
				return;
			}			
			printf("ִ�гɹ�\n");
			break;
		}
		default: //�Ƿ���������
		{	
			do_error(ERROR_INVALID_REQUEST);
			return;
		}
	}
}

/* ����ȱҳ�ж� */
void do_page_fault(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i;
	printf("����ȱҳ�жϣ���ʼ���е�ҳ...\n");
	for (i = 0; i < BLOCK_SUM; i++)
	{
		if (!blockStatus[i])
		{
			/* ���������ݣ�д�뵽ʵ�� */
			do_page_in(ptr_pageTabIt, i);
			
			/* ����ҳ������ */
			ptr_pageTabIt->blockNum = i;
			ptr_pageTabIt->filled = TRUE;
			ptr_pageTabIt->edited = FALSE;
			ptr_pageTabIt->count = 0;
			
			blockStatus[i] = TRUE;
			return;
		}
	}
	/* û�п�������飬����ҳ���滻 */
	do_LFU(ptr_pageTabIt);
}

/* ����LFU�㷨����ҳ���滻 */
void do_LFU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i, min, page;
	printf("û�п�������飬��ʼ����LFUҳ���滻...\n");
	for (i = 0, min = 0xFFFFFFFF, page = 0; i < PAGE_SUM; i++)
	{
		if (pageTable[i].count < min)
		{
			min = pageTable[i].count;
			page = i;
		}
	}
	printf("ѡ���%uҳ�����滻\n", page);
	if (pageTable[page].edited)
	{
		/* ҳ���������޸ģ���Ҫд�������� */
		printf("��ҳ�������޸ģ�д��������\n");
		do_page_out(&pageTable[page]);
	}
	pageTable[page].filled = FALSE;
	pageTable[page].count = 0;


	/* ���������ݣ�д�뵽ʵ�� */
	do_page_in(ptr_pageTabIt, pageTable[page].blockNum);
	
	/* ����ҳ������ */
	ptr_pageTabIt->blockNum = pageTable[page].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	printf("ҳ���滻�ɹ�\n");
}

/* ����������д��ʵ�� */
void do_page_in(Ptr_PageTableItem ptr_pageTabIt, unsigned int blockNum)
{
	unsigned int readNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
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
	printf("��ҳ�ɹ��������ַ%u-->>�����%u\n", ptr_pageTabIt->auxAddr, blockNum);
}

/* �����滻ҳ�������д�ظ��� */
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
	printf("д�سɹ��������%u-->>�����ַ%03X\n", ptr_pageTabIt->auxAddr, ptr_pageTabIt->blockNum);
}

/* ������ */
void do_error(ERROR_CODE code)
{
	switch (code)
	{
		case ERROR_READ_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ���ݲ��ɶ�\n");
			break;
		}
		case ERROR_WRITE_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ���ݲ���д\n");
			break;
		}
		case ERROR_EXECUTE_DENY:
		{
			printf("�ô�ʧ�ܣ��õ�ַ���ݲ���ִ��\n");
			break;
		}		
		case ERROR_INVALID_REQUEST:
		{
			printf("�ô�ʧ�ܣ��Ƿ��ô�����\n");
			break;
		}
		case ERROR_OVER_BOUNDARY:
		{
			printf("�ô�ʧ�ܣ���ַԽ��\n");
			break;
		}
		case ERROR_FILE_OPEN_FAILED:
		{
			printf("ϵͳ���󣺴��ļ�ʧ��\n");
			break;
		}
		case ERROR_FILE_CLOSE_FAILED:
		{
			printf("ϵͳ���󣺹ر��ļ�ʧ��\n");
			break;
		}
		case ERROR_FILE_SEEK_FAILED:
		{
			printf("ϵͳ�����ļ�ָ�붨λʧ��\n");
			break;
		}
		case ERROR_FILE_READ_FAILED:
		{
			printf("ϵͳ���󣺶�ȡ�ļ�ʧ��\n");
			break;
		}
		case ERROR_FILE_WRITE_FAILED:
		{
			printf("ϵͳ����д���ļ�ʧ��\n");
			break;
		}
		default:
		{
			printf("δ֪����û������������\n");
		}
	}
}

/* �����ô����� */
void do_request()
{
	/* ������������ַ */
	ptr_memAccReq->virAddr = random() % VIRTUAL_MEMORY_SIZE;
	/* ��������������� */
	switch (random() % 3)
	{
		case 0: //������
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("��������\n��ַ��%u\t���ͣ���ȡ\n", ptr_memAccReq->virAddr);
			break;
		}
		case 1: //д����
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* ���������д���ֵ */
			ptr_memAccReq->value = random() % 0xFFu;
			printf("��������\n��ַ��%u\t���ͣ�д��\tֵ��%02X\n", ptr_memAccReq->virAddr, ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("��������\n��ַ��%u\t���ͣ�ִ��\n", ptr_memAccReq->virAddr);
			break;
		}
		default:
			break;
	}	
}

/* ��ӡҳ�� */
void do_print_info()
{
	unsigned int i, j, k;
	char str[4];
	printf("ҳ��\t���\tװ��\t�޸�\t����\t����\t����\n");
	for (i = 0; i < PAGE_SUM; i++)
	{
		printf("%u\t%u\t%u\t%u\t%s\t%u\t%u\n", i, pageTable[i].blockNum, pageTable[i].filled, 
			pageTable[i].edited, get_proType_str(str, pageTable[i].proType), 
			pageTable[i].count, pageTable[i].auxAddr);
	}
}

/* ��ȡҳ�汣�������ַ��� */
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
	int i;
	if (!(ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+")))
	{
		do_error(ERROR_FILE_OPEN_FAILED);
		exit(1);
	}
	
	do_init();
	do_print_info();
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));
	/* ��ѭ����ģ��ô������봦����� */
	while (TRUE)
	{
		do_request();
		do_response();
		printf("��Y��ӡҳ��������������ӡ...\n");
		if ((c = getchar()) == 'y' || c == 'Y')
			do_print_info();
		while (c != '\n')
			c = getchar();
		printf("��X�˳����򣬰�����������...\n");
		if ((c = getchar()) == 'x' || c == 'X')
			break;
		while (c != '\n')
			c = getchar();
		//sleep(5000);
	}

	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	return (0);
}
