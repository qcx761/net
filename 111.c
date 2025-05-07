#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>


int main(){
    

    return 0;
}

int* dailyTemperatures(int* temperatures, int temperaturesSize, int* returnSize) {
    *returnSize = temperaturesSize;
    int* ans = calloc(temperaturesSize, sizeof(int));
    int* st = malloc(temperaturesSize * sizeof(int)); // 用数组模拟栈
    int top = -1; // 栈顶下标
    for (int i = 0; i < temperaturesSize; i++) {
        int t = temperatures[i];
        while (top >= 0 && t > temperatures[st[top]]) {
            int j = st[top--]; // 弹出栈顶
            ans[j] = i - j;
        }
        st[++top] = i; // 入栈
    }
    free(st);
    return ans;
}

