/*
 * �ṩlist_head�ṹ��ṹ�����������
 * ����ṹ�彫���������б������ڽṹ�����������
 */

#ifndef __COMMON_ST_H__
#define __COMMON_ST_H__

struct list_head{
	struct list_head *next;
	struct list_head *prev;
};

/* �����������һ��list_headͷ�� */
#define DECLARE_HEAD(head) \
	static struct list_head head = { \
		.next = &head, \
		.prev = &head, \
	}

#define LIST_ENTRY(list_head, type, member_name) \
	(type *)((unsigned int)list_head - (unsigned int)(&(((type*)(0))->member_name)))

/* from head to tail */
#define LIST_FOR_EACH_ENTRY_H(pos, head) \
	for(pos = (head)->next; pos != head; pos = pos->next)

/* from tail to head */
#define LIST_FOR_EACH_ENTRY_T(pos, head) \
	for(pos = (head)->prev; pos != head; pos = pos->prev)

void ListAddTail(struct list_head *new, struct list_head *head);
void ListDelTail(struct list_head *todel);

#endif
