/*
 * Simple page allocator for cmpctmalloc heap
 */
#include <lk/debug.h>
#include <lk/list.h>
#include <sys/types.h>
#include <kernel/vm.h>
#include <kernel/mutex.h>

void *page_alloc(size_t pages) {
    struct list_node page_list = LIST_INITIAL_VALUE(page_list);

    /* Allocate pages from PMM */
    size_t count = pmm_alloc_pages(pages, &page_list);
    if (count < pages) {
        /* Failed to allocate enough pages, free what we got */
        if (count > 0) {
            pmm_free(&page_list);
        }
        return NULL;
    }

    /* Get the first page from the list */
    vm_page_t *p = list_peek_head_type(&page_list, vm_page_t, node);
    if (!p) {
        return NULL;
    }

    /* Calculate physical address from page */
    paddr_t pa = vm_page_to_paddr(p);

    /* Convert physical address to virtual address using kernel mapping */
    vaddr_t va = (vaddr_t)paddr_to_kvaddr(pa);

    return (void *)va;
}

status_t page_free(void *ptr, size_t pages) {
    // For now, we don't actually free pages back to PMM
    // This is a simplified implementation
    return 0;
}
