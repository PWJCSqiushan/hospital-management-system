/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：utils.cpp
 * 修改摘要：【升级6.0】
 * 1. 保留原有清屏、按键功能。
 * 2. 新增模拟时间管理函数实现，替代系统真实时间。
 * 3. 日期推进逻辑支持跨月、跨年。
 */

#define _CRT_SECURE_NO_WARNINGS
#include "utils.h"
#include "hospital_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <string.h>
#include <time.h>

 /* ==================== 控制台交互函数 ==================== */

void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void press_enter_to_continue() {
    printf("\n按任意键继续...");
    _getch();
}

int get_single_key() {
    return _getch();
}

/* ==================== 模拟时间管理函数 ==================== */

static int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int days_in_month(int year, int month) {
    int days[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (month == 2 && is_leap_year(year))
        return 29;
    return days[month - 1];
}

void init_system_time(int year, int month, int day) {
    sprintf(g_current_date, "%04d-%02d-%02d", year, month, day);
}

void advance_one_day(void) {
    int year, month, day;
    if (sscanf(g_current_date, "%d-%d-%d", &year, &month, &day) != 3) {
        strcpy(g_current_date, "2026-03-09");
        return;
    }

    day++;
    if (day > days_in_month(year, month)) {
        day = 1;
        month++;
        if (month > 12) {
            month = 1;
            year++;
        }
    }
    sprintf(g_current_date, "%04d-%02d-%02d", year, month, day);
}

void get_current_date_str(char* buffer) {
    strcpy(buffer, g_current_date);
}

void get_current_datetime_str(char* buffer) {
    int hour, minute;
    static unsigned int seed = 0;
    if (seed == 0) {
        seed = (unsigned int)time(NULL);
        if (seed == 0) seed = 12345;  // 兜底值
    }

    seed = seed * 1103515245 + 12345;
    hour = 8 + (seed % 11);
    seed = seed * 1103515245 + 12345;
    minute = (seed % 60);

    sprintf(buffer, "%s %02d:%02d", g_current_date, hour, minute);
}

/* 药品类别名称 */
const char* get_medicine_category_name(int category) {
    switch (category) {
    case 0: return "甲类";
    case 1: return "乙类";
    case 2: return "丙类";
    default: return "未知";
    }
}

/* 医保类型名称 */
const char* get_insurance_type_name(int insurance_type) {
    switch (insurance_type) {
    case 0: return "无医保";
    case 1: return "居民医保";
    case 2: return "职工医保";
    default: return "未知";
    }
}

/* 计算医保报销 */
float calculate_insurance_reimbursement(struct Patient* p, int use_insurance,
    float* out_patient_pay, float* out_insurance_pay) {
    if (!p || !use_insurance || p->insurance_type == 0) {
        *out_patient_pay = p ? (p->fee_medicine + p->fee_check + p->fee_treat + p->fee_ward) : 0.0f;
        *out_insurance_pay = 0.0f;
        return *out_patient_pay;
    }

    float medicine_insurance_pay = 0.0f;
    float medicine_patient_pay = 0.0f;

    char temp_pres[512] = {0};
    strncpy(temp_pres, p->prescription, sizeof(temp_pres)-1);

    char* token = strtok(temp_pres, ",");
    while (token) {
        while (*token == ' ') token++;
        char med_name[100] = {0};
        int qty = 1;
        char* star = strchr(token, '*');
        if (star) {
            strncpy(med_name, token, star - token);
            med_name[star - token] = '\0';
            qty = atoi(star + 1);
        } else {
            strncpy(med_name, token, sizeof(med_name)-1);
        }

        Medicine* m = g_medicine_head;
        while (m) {
            if (strcmp(m->common_name, med_name) == 0 || strcmp(m->trade_name, med_name) == 0 || strcmp(m->alias, med_name) == 0) {
                float total_price = m->price * qty;
                if (p->insurance_type == 1) {
                    /* 居民医保 */
                    if (m->category == 0) { medicine_insurance_pay += total_price * 0.7f; medicine_patient_pay += total_price * 0.3f; }
                    else if (m->category == 1) { medicine_patient_pay += total_price * 0.05f; float rem = total_price * 0.95f; medicine_insurance_pay += rem * 0.7f; medicine_patient_pay += rem * 0.3f; }
                    else { medicine_patient_pay += total_price; }
                } else if (p->insurance_type == 2) {
                    /* 职工医保 */
                    if (m->category == 0) { medicine_insurance_pay += total_price * 0.8f; medicine_patient_pay += total_price * 0.2f; }
                    else if (m->category == 1) { medicine_patient_pay += total_price * 0.05f; float rem = total_price * 0.95f; medicine_insurance_pay += rem * 0.8f; medicine_patient_pay += rem * 0.2f; }
                    else { medicine_patient_pay += total_price; }
                }
                break;
            }
            m = m->next;
        }

        token = strtok(NULL, ",");
    }

    float other_total = p->fee_check + p->fee_treat + p->fee_ward;
    float other_ins = 0.0f, other_pat = 0.0f;
    if (p->insurance_type == 1) { other_ins = other_total * 0.5f; other_pat = other_total * 0.5f; }
    else if (p->insurance_type == 2) { other_ins = other_total * 0.7f; other_pat = other_total * 0.3f; }
    else { other_pat = other_total; }

    *out_insurance_pay = medicine_insurance_pay + other_ins;
    *out_patient_pay = medicine_patient_pay + other_pat;
    return *out_insurance_pay + *out_patient_pay;
}

/* 交互式 Y/N 提示，直到用户输入有效为止。返回 1 表示 Y/是，0 表示 N/否。 */
int ask_yes_no(const char* prompt) {
    char c = 0;
    while (1) {
        printf("%s", prompt);
        if (scanf(" %c", &c) != 1) {
            while (getchar() != '\n');
            printf("输入无效，请输入 Y 或 N。\n");
            continue;
        }
        while (getchar() != '\n');
        if (c == 'Y' || c == 'y') return 1;
        if (c == 'N' || c == 'n') return 0;
        printf("输入无效，请输入 Y 或 N。\n");
    }
}

