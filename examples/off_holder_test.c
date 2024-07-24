#include <hatrack.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

struct ll {
    off_holder_t next;
    off_holder_t prev;
    off_holder_t data;  // native type: char *
};

#define LIST_ROOT 0  // ralloc root index - must not be MMM_ROOT (1)

int main(int argc, char *argv[])
{
    struct ll *l1, *l2, *l3, *lN, *p, *prev = NULL;
    char *d1, *d2, *d3, *dN, *c;
    int restart;
    unsigned int count = 0;

    restart = mmm_init("hatrack-off-holder-test", GB(2));

    if (!restart) {
	l1 = HR_malloc(sizeof(*l1));
	l2 = HR_malloc(sizeof(*l2));
	l3 = HR_malloc(sizeof(*l3));
	d1 = HR_malloc(80);
	snprintf(d1, 80, "This is the first list element.");
	d2 = HR_malloc(80);
	snprintf(d2, 80, "This is the second list element.");
	d3 = HR_malloc(80);
	snprintf(d3, 80, "This is the third list element.");
	printf("l1=%p, l2=%p, l3=%p, d1=%p, d2=%p, d3=%p\n",
	       l1, l2, l3, d1, d2, d3);

	l1->data = ptr2off(d1, &l1->data);
	l1->next = ptr2off(l2, &l1->next);
	l1->prev = ptr2off(NULL, &l1->prev);
	printf("l1->data=0x%lx, l1->next=0x%lx, l1->prev=0x%lx\n",
	       l1->data, l1->next, l1->prev);

	l2->data = ptr2off(d2, &l2->data);
	l2->next = ptr2off(l3, &l2->next);
	l2->prev = ptr2off(l1, &l2->prev);
	printf("l2->data=0x%lx, l2->next=0x%lx, l2->prev=0x%lx\n",
	       l2->data, l2->next, l2->prev);

	l3->data = ptr2off(d3, &l3->data);
	l3->next = ptr2off(NULL, &l3->next);
	l3->prev = ptr2off(l2, &l3->prev);
	printf("l3->data=0x%lx, l3->next=0x%lx, l3->prev=0x%lx\n",
	       l3->data, l3->next, l3->prev);
	RP_set_root(l1, LIST_ROOT);
    } else { // restart
	l1 = (struct ll *)RP_get_root_c(LIST_ROOT);
	printf("restart: l1=%p\n", l1);
	for (p = l1; p; prev = p, p = off2ptr(p->next, &p->next, p)) {
	    c = off2ptr(p->data, &p->data, c);
	    printf("found: p=%p, c=%p '%s'\n", p, c, c);
	    count++;
	}
	lN = HR_malloc(sizeof(*lN));
	dN = HR_malloc(80);
	snprintf(dN, 80, "This is the %uth list element.", count+1u);
	lN->data = ptr2off(dN, &lN->data);
	lN->next = ptr2off(NULL, &lN->next);
	lN->prev = ptr2off(prev, &lN->prev);
	prev->next = ptr2off(lN, &prev->next);
	printf("adding: lN=%p, dN=%p '%s', lN->next=0x%lx, lN->prev=0x%lx\n",
	       lN, dN, dN, lN->next, lN->prev);
    }

    for (p = l1; p; p = off2ptr(p->next, &p->next, p)) {
	c = off2ptr(p->data, &p->data, c);
	prev = off2ptr(p->prev, &p->prev, prev);
	printf("p=%p, c=%p '%s', prev=%p\n", p, c, c, prev);
    }

    return 0;
}
