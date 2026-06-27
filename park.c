#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define MAX_CAPACITY 10
#define ENTRY_GATES 3
#define EXIT_GATES 2
#define TOTAL_CARS 20
#define QUEUE_SIZE (TOTAL_CARS + ENTRY_GATES)
va_list
typedef struct
{
    int car_id;
    sem_t enter_sem; // 等待入口通行信号
} CarRequest;

sem_t parking_spots;  // 停车位信号量
sem_t entry_gate_sem; // 入口闸口可用计数
sem_t exit_gate_sem;  // 出口闸口可用计数
sem_t queue_slots;    // 等待队列可用位置
sem_t queue_items;    // 等待队列中车辆数量
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gate_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

int available_spots = MAX_CAPACITY;
int entry_gate_free[ENTRY_GATES];
int exit_gate_free[EXIT_GATES];
CarRequest *request_queue[QUEUE_SIZE];
int queue_head = 0;
int queue_tail = 0;

// 安全打印信息
void safe_printf(const char *format, ...)
{
    va_list args;
    pthread_mutex_lock(&print_mutex);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    pthread_mutex_unlock(&print_mutex);
}

// 生产者：将车辆请求放入等待队列
// 这里模拟车辆到达后进入入口等待队列，先获取队列空槽，再写入队列。
// queue_slots 表示队列剩余可写位置，queue_items 表示队列当前待消费数量。
void enqueue_request(CarRequest *req)
{
    sem_wait(&queue_slots);
    pthread_mutex_lock(&queue_mutex);
    request_queue[queue_tail] = req;
    queue_tail = (queue_tail + 1) % QUEUE_SIZE;
    pthread_mutex_unlock(&queue_mutex);
    sem_post(&queue_items);
}

// 消费者：从等待队列取出车辆请求
// 入口闸口线程从队列中消费请求，处理后再通知对应车辆进入停车场。
CarRequest *dequeue_request(void)
{
    CarRequest *req;
    sem_wait(&queue_items);
    pthread_mutex_lock(&queue_mutex);
    req = request_queue[queue_head];
    queue_head = (queue_head + 1) % QUEUE_SIZE;
    pthread_mutex_unlock(&queue_mutex);
    sem_post(&queue_slots);
    return req;
}

// 分配一个空闲入口闸口编号
int allocate_entry_gate(void)
{
    int gate = -1;
    pthread_mutex_lock(&gate_mutex);
    for (int i = 0; i < ENTRY_GATES; i++)
    {
        if (entry_gate_free[i])
        {
            entry_gate_free[i] = 0;
            gate = i + 1;
            break;
        }
    }
    pthread_mutex_unlock(&gate_mutex);
    return gate;
}

// 释放入口闸口
void release_entry_gate(int gate)
{
    pthread_mutex_lock(&gate_mutex);
    entry_gate_free[gate - 1] = 1;
    pthread_mutex_unlock(&gate_mutex);
}

// 分配一个空闲出口闸口编号
int allocate_exit_gate(void)
{
    int gate = -1;
    pthread_mutex_lock(&gate_mutex);
    for (int i = 0; i < EXIT_GATES; i++)
    {
        if (exit_gate_free[i])
        {
            exit_gate_free[i] = 0;
            gate = i + 1;
            break;
        }
    }
    pthread_mutex_unlock(&gate_mutex);
    return gate;
}

// 释放出口闸口
void release_exit_gate(int gate)
{
    pthread_mutex_lock(&gate_mutex);
    exit_gate_free[gate - 1] = 1;
    pthread_mutex_unlock(&gate_mutex);
}

// 入口闸口工作线程：消费等待队列中的车辆请求
// 这是消费者角色，负责把排队车辆转化为实际入场操作。
void *entry_gate_thread(void *arg)
{
    while (1)
    {
        CarRequest *req = dequeue_request();
        if (req == NULL)
        {
            // 收到结束信号，退出
            break;
        }

        sem_wait(&entry_gate_sem);           // 等待入口闸口空闲
        int gate_id = allocate_entry_gate(); // 真实分配一个闸口编号
        safe_printf("车辆 %d 使用入口闸口 %d 进入等待停车\n", req->car_id, gate_id);

        // 等待停车位可用
        sem_wait(&parking_spots);
        pthread_mutex_lock(&gate_mutex);
        available_spots--;
        safe_printf("车辆 %d 已进入停车场，当前空余停车位: %d\n", req->car_id, available_spots);
        pthread_mutex_unlock(&gate_mutex);

        release_entry_gate(gate_id);
        sem_post(&entry_gate_sem);

        // 通知对应车辆线程，入口处理完成，可以继续停车流程
        sem_post(&req->enter_sem);
    }
    return NULL;
}

void *car_thread(void *arg)
{
    int car_id = *(int *)arg;
    free(arg);

    CarRequest *req = malloc(sizeof(CarRequest));
    if (req == NULL)
    {
        perror("malloc");
        return NULL;
    }
    req->car_id = car_id;
    sem_init(&req->enter_sem, 0, 0);

    // 模拟车辆随机到达时间
    usleep((rand() % 300 + 100) * 1000);
    safe_printf("车辆 %d 到达停车场，等待进入\n", car_id);

    // 生产者：将车辆入场请求提交到等待队列
    enqueue_request(req);

    // 等待入口闸口线程处理完入场请求
    sem_wait(&req->enter_sem);
    sem_destroy(&req->enter_sem);

    // 车辆成功进入停车场后，模拟停车停留时间
    usleep((rand() % 500 + 200) * 1000);

    safe_printf("车辆 %d 准备离开停车场\n", car_id);
    sem_wait(&exit_gate_sem);
    int exit_gate = allocate_exit_gate();
    safe_printf("车辆 %d 正在使用出口闸口 %d 离开\n", car_id, exit_gate);

    // 释放一个停车位
    sem_post(&parking_spots);
    pthread_mutex_lock(&gate_mutex);
    available_spots++;
    safe_printf("车辆 %d 已离开，当前空余停车位: %d\n", car_id, available_spots);
    pthread_mutex_unlock(&gate_mutex);

    release_exit_gate(exit_gate);
    sem_post(&exit_gate_sem);

    free(req);
    return NULL;
}

int main(void)
{
    pthread_t car_threads[TOTAL_CARS];
    pthread_t gate_threads[ENTRY_GATES];

    srand(time(NULL));

    sem_init(&parking_spots, 0, MAX_CAPACITY);
    sem_init(&entry_gate_sem, 0, ENTRY_GATES);
    sem_init(&exit_gate_sem, 0, EXIT_GATES);
    sem_init(&queue_slots, 0, QUEUE_SIZE);
    sem_init(&queue_items, 0, 0);

    for (int i = 0; i < ENTRY_GATES; i++)
        entry_gate_free[i] = 1;
    for (int i = 0; i < EXIT_GATES; i++)
        exit_gate_free[i] = 1;

    safe_printf("=== 停车场模拟开始 ===\n");
    safe_printf("停车场容量: %d, 入口闸口: %d, 出口闸口: %d, 总车辆: %d\n\n",
                MAX_CAPACITY, ENTRY_GATES, EXIT_GATES, TOTAL_CARS);

    for (int i = 0; i < ENTRY_GATES; i++)
        pthread_create(&gate_threads[i], NULL, entry_gate_thread, NULL);

    for (int i = 0; i < TOTAL_CARS; i++)
    {
        int *car_id = malloc(sizeof(int));
        if (car_id == NULL)
        {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        *car_id = i + 1;
        pthread_create(&car_threads[i], NULL, car_thread, car_id);
    }

    for (int i = 0; i < TOTAL_CARS; i++)
        pthread_join(car_threads[i], NULL);

    // 发送退出信号给入口闸口线程
    for (int i = 0; i < ENTRY_GATES; i++)
        enqueue_request(NULL);

    for (int i = 0; i < ENTRY_GATES; i++)
        pthread_join(gate_threads[i], NULL);

    safe_printf("\n=== 停车场模拟结束 ===\n");

    sem_destroy(&parking_spots);
    sem_destroy(&entry_gate_sem);
    sem_destroy(&exit_gate_sem);
    sem_destroy(&queue_slots);
    sem_destroy(&queue_items);
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&gate_mutex);
    pthread_mutex_destroy(&print_mutex);

    return 0;
}
