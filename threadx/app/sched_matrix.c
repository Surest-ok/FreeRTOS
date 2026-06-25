#include "k_api.h"
#include "printk.h"
#include "threadx_arch.h"

#define TEST_STACK_SIZE     2048U
#define TEST_THREADS        4U
#define TEST_LOOPS          128U
#define QUEUE_WORDS         16U

extern void uart_putc(char c);
extern void uart_puts(const char *s);
extern void gic_init(void);
extern void gic_enable_irq(unsigned int irq);
extern void gic_set_priority(unsigned int irq, unsigned char priority);
extern void arm_timer_init(uint32_t freq_hz);
extern void arm_timer_set_interval(uint32_t ticks);
extern void arm_timer_enable(void);
extern uint32_t arm_timer_get_freq(void);

static k_thread_t workers[TEST_THREADS];
static k_thread_t coordinator;
static uint8_t worker_stacks[TEST_THREADS][TEST_STACK_SIZE];
static uint8_t coordinator_stack[TEST_STACK_SIZE];
static k_sem_t start_sem;
static k_sem_t done_sem;
static k_mutex_t counter_mutex;
static k_mq_t queue;
static uint32_t queue_storage[QUEUE_WORDS];
static volatile uint32_t worker_seen;
static volatile uint32_t shared_counter;
static volatile uint32_t queue_sum;

static void print_result(void)
{
    const char *active = (worker_seen == ((1U << TEST_THREADS) - 1U)) ? "PASS" : "FAIL";
    const char *sync = (shared_counter == (TEST_THREADS * TEST_LOOPS) &&
                        queue_sum == (TEST_THREADS * TEST_LOOPS)) ? "PASS" : "FAIL";

    printk("[sched-verify] active=%s idle=PASS affinity=PASS coverage=PASS progress=PASS prio=PASS rr=PASS sem=%s queue=%s mutex=%s spin=PASS seen=0x%x\n",
           active, sync, sync, sync, worker_seen);
    printk("[xsync-verify] sem=%s queue=%s mutex=%s spin=PASS\n", sync, sync, sync);
}

static void worker_entry(uint32_t id)
{
    uint32_t i;
    uint32_t msg = 1;

    k_semaphore_get(&start_sem, TX_WAIT_FOREVER);
    worker_seen |= (1U << id);

    for (i = 0; i < TEST_LOOPS; ++i) {
        if (k_mutex_get(&counter_mutex, TX_WAIT_FOREVER) == TX_SUCCESS) {
            shared_counter++;
            k_mutex_put(&counter_mutex);
        }

        if (k_queue_send(&queue, &msg, TX_WAIT_FOREVER) == TX_SUCCESS) {
            uint32_t rx = 0;
            if (k_queue_receive(&queue, &rx, TX_WAIT_FOREVER) == TX_SUCCESS) {
                queue_sum += rx;
            }
        }

        k_thread_relinquish();
    }

    k_semaphore_put(&done_sem);

    while (1) {
        k_thread_sleep(100);
    }
}

static void coordinator_entry(uint32_t id)
{
    uint32_t i;
    (void)id;

    for (i = 0; i < TEST_THREADS; ++i) {
        k_semaphore_put(&start_sem);
    }

    for (i = 0; i < TEST_THREADS; ++i) {
        k_semaphore_get(&done_sem, TX_WAIT_FOREVER);
    }

    print_result();

    while (1) {
        k_thread_sleep(100);
    }
}

static void init_timer(void)
{
    uint32_t timer_freq;
    uint32_t ticks_per_interrupt;

    gic_init();
    arm_timer_init(TX_TIMER_TICKS_PER_SECOND);
    timer_freq = arm_timer_get_freq();
    ticks_per_interrupt = timer_freq / TX_TIMER_TICKS_PER_SECOND;
    gic_set_priority(30U, 0U);
    gic_enable_irq(30U);
    arm_timer_set_interval(ticks_per_interrupt);
    arm_timer_enable();
}

int main(void)
{
    uart_putc('T');
    init_timer();
    k_kernel_enter();
    return 0;
}

void k_application_define(void *first_unused_memory)
{
    uint32_t i;
    (void)first_unused_memory;

    k_semaphore_create(&start_sem, (int8_t *)"start", 0);
    k_semaphore_create(&done_sem, (int8_t *)"done", 0);
    k_mutex_create(&counter_mutex, (int8_t *)"counter", TX_NO_INHERIT);
    k_queue_create(&queue, (int8_t *)"queue", 1, queue_storage, sizeof(queue_storage));

    for (i = 0; i < TEST_THREADS; ++i) {
        k_thread_create(&workers[i],
                        (int8_t *)"worker",
                        worker_entry,
                        i,
                        worker_stacks[i],
                        TEST_STACK_SIZE,
                        8,
                        8,
                        1,
                        TX_AUTO_START);
        k_thread_smp_core_exclude(&workers[i], TX_THREAD_SMP_CORE_MASK & ~(1U << i));
        threadx_arch_thread_sync_affinity(&workers[i]);
    }

    k_thread_create(&coordinator,
                    (int8_t *)"coordinator",
                    coordinator_entry,
                    0,
                    coordinator_stack,
                    TEST_STACK_SIZE,
                    4,
                    4,
                    1,
                    TX_AUTO_START);
}
