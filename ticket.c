#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> // 多线程库
#include <unistd.h>  // usleep() 函数
#include <time.h>    // time() 函数，用于随机数种子

// ===== 宏定义 =====
#define TOTAL_TICKETS 1000 // 总票数
#define MAX_WINDOWS 4      // 售票窗口数
#define MIN_WINDOWS 3      // 最小窗口数（备用）

// ===== 全局变量 =====
int tickets = TOTAL_TICKETS;                       // 剩余票数（共享资源）
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // 互斥锁，保护tickets变量

// ===== 售票窗口线程函数 =====
// 功能：每个窗口线程执行此函数，不断卖票直到售完为止
// 参数：arg - 窗口编号
void *sell_tickets(void *arg)
{
    int windows_id = *(int *)arg; // 获取窗口编号
    int sold_count = 0;           // 该窗口售出票数统计

    while (1)
    {
        // ===== 临界区：加锁保护共享资源 =====
        pthread_mutex_lock(&mutex);

        // 检查票是否已售完
        if (tickets <= 0)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // 卖一张票
        tickets--;
        sold_count++;
        printf("窗口 %d 卖出一张票，剩余 %d 张票\n", windows_id, tickets);

        // ===== 临界区解锁 =====
        pthread_mutex_unlock(&mutex);

        // 模拟售票员的处理时间：随机延迟 10-49 毫秒
        // rand() % 40 生成 0-39，加 10 得到 10-49
        // 乘以 1000 转换为微秒（usleep单位是微秒）
        usleep((rand() % 40 + 10) * 1000);

        // 主动让出 CPU，增加线程切换概率，提高并发效果
        sched_yield();
    }

    printf("窗口 %d 共售出 %d 张票\n", windows_id, sold_count);
    pthread_exit(NULL); // 线程退出
}

int main()
{
    pthread_t tid[MAX_WINDOWS];  // 存储线程ID的数组
    int windows_id[MAX_WINDOWS]; // 存储窗口编号的数组

    // 初始化随机数生成器（以当前时间作为种子，每次运行结果不同）
    srand(time(NULL));

    // ===== 程序启动提示 =====
    printf("=== 窗口售票模拟开始 ===\n");
    printf("总票数: %d, 窗口数: %d\n\n", TOTAL_TICKETS, MAX_WINDOWS);

    // ===== 创建售票窗口线程 =====
    // 创建 MAX_WINDOWS 个线程，每个线程执行 sell_tickets 函数
    for (int i = 0; i < MAX_WINDOWS; i++)
    {
        windows_id[i] = i + 1; // 窗口号从 1 开始
        pthread_create(&tid[i], NULL, sell_tickets, &windows_id[i]);
    }

    // ===== 等待所有线程完成 =====
    // pthread_join 会阻塞当前线程，直到目标线程退出
    for (int i = 0; i < MAX_WINDOWS; i++)
    {
        pthread_join(tid[i], NULL);
    }

    // ===== 程序结束提示和统计 =====
    printf("\n=== 售票完成 ===\n");
    printf("剩余票数: %d\n", tickets);

    return 0;
}
