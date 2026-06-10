/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：hospital_functions.cpp
 * 修改摘要：【升级6.0】
 * 1. 医生初始化时设置 is_active = 1（在职）。
 * 2. 保留基础数据初始化功能。
 * 3. 包含 init_hospital_data 的实现，供 main 调用。
 */

#define _CRT_SECURE_NO_WARNINGS
#include "hospital_system.h"
#include "inpatient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_MAGENTA "\033[35m"

 /* ==================== 系统数据初始化 ==================== */

static void init_doctors() {
    if (g_doctor_head != NULL) return;

    /* 预设的20位医生姓名 */
    const char* names[20] = {
        "董宗岳", "苏晴", "顾明轩", "林毅",
        "许若楠", "陈默", "温雅", "周博彦",
        "沈知微", "唐婉清", "宋承泽", "夏沐曦",
        "陆景琛", "方念慈", "郑嘉树", "季星瑶",
        "秦悦", "那日苏", "格桑卓玛", "钟砚辞"
    };

    /*
     * 20名医生，5个科室，每个科室4名医生。
     * 每4人一组：第0位主任(职级1, 50元)，第1位副主任(职级2,30元)，其余为普通医师(职级3,15元)
     */
    for (int i = 0; i < 20; i++) {
        Doctor* d = (Doctor*)malloc(sizeof(Doctor));
        if (d == NULL) return;
        d->id = 1001 + i;
        (void)strcpy(d->name, names[i]);
        (void)strcpy(d->password, "123");

        d->dept_id = (i / 4) + 1; /* 每4人一个科室，1-5 */

        int role_index = i % 4;
        if (role_index == 0) {
            d->title_level = 1; /* 主任医师 */
            d->consultation_fee = 50.0f;
        }
        else if (role_index == 1) {
            d->title_level = 2; /* 副主任医师 */
            d->consultation_fee = 30.0f;
        }
        else {
            d->title_level = 3; /* 普通医师 */
            d->consultation_fee = 15.0f;
        }

        d->is_active = 1; /* 默认在职 */

        d->next = g_doctor_head;
        g_doctor_head = d;
    }
}

//static void init_doctors() {
//    if (g_doctor_head != NULL) return;
//
//    /* 预设的20位医生姓名 */
//    const char* names[20] = {
//        "董宗岳", "苏晴", "顾明轩", "林毅",
//        "许若楠", "陈默", "温雅", "周博彦",
//        "沈知微", "唐婉清", "宋承泽", "夏沐曦",
//        "陆景琛", "方念慈", "郑嘉树", "季星瑶",
//        "秦悦", "那日苏", "格桑卓玛", "钟砚辞"
//    };
//
//    /*
//     * 20名医生，5个科室，每个科室4名医生。
//     * 0:主任(50元) 1:副主任(30元) 2,3:普通医师(15元)
//     */
//    for (int i = 0; i < 20; i++) {
//        Doctor* d = (Doctor*)malloc(sizeof(Doctor));
//        if (d == NULL) return;
//        d->id = 1001 + i;
//        (void)strcpy(d->name, names[i]);
//        (void)strcpy(d->password, "123");
//
//        d->dept_id = (i / 4) + 1; /* 每4人一个科室，1-5 */
//
//        int role_index = i % 4;
//        if (role_index == 0) {
//            d->title_level = 1; /* 主任医师 */
//            d->consultation_fee = 50.0f;
//        }
//        else if (role_index == 1) {
//            d->title_level = 2; /* 副主任医师 */
//            d->consultation_fee = 30.0f;
//        }
//        else {
//            d->title_level = 3; /* 普通医师 */
//            d->consultation_fee = 15.0f;
//        }
//
//        d->is_active = 1; /* 【6.0新增】默认在职 */
//
//        d->next = g_doctor_head;
//        g_doctor_head = d;
//    }
//} 

static void init_medicines() {
    if (g_medicine_head != NULL) return;

    struct {
        const char* c_name; const char* t_name; const char* alias; float price; int depts[6];
    } med_data[20] = {
        {"乙酰水杨酸", "拜阿司匹林", "阿司匹林", 15.5f, {1,0,0,0,0,0}},
        {"阿莫西林胶囊", "阿莫仙", "消炎药", 22.0f, {1,0,0,0,0,0}},
        {"对乙酰氨基酚", "泰诺林", "扑热息痛", 18.0f, {1,0,0,0,0,0}},
        {"布洛芬缓释胶囊", "芬必得", "止痛药", 25.5f, {1,0,0,0,0,0}},
        {"头孢克肟分散片", "世福素", "头孢", 35.0f, {1,0,0,0,0,0}},

        {"蒙脱石散", "思密达", "止泻药", 28.0f, {0,1,1,0,1,0}},
        {"奥美拉唑肠溶胶囊", "洛赛克", "胃药", 45.0f, {0,1,1,1,0,0}},
        {"二甲双胍肠溶片", "格华止", "降糖药", 30.0f, {0,0,1,0,0,0}},
        {"硝苯地平控释片", "拜新同", "降压药", 40.0f, {0,1,1,0,0,0}},
        {"阿奇霉素分散片", "希舒美", "消炎药", 32.0f, {1,0,0,0,0,0}},

        {"小儿氨酚黄那敏颗粒", "护彤", "小儿感冒药", 15.0f, {0,1,0,0,1,0}},
        {"复方丁香开胃贴", "丁桂儿脐贴", "宝宝肚脐贴", 20.0f, {0,0,0,0,1,0}},
        {"头孢丙烯干混悬剂", "施复捷", "小儿头孢", 42.0f, {0,1,0,0,1,0}},

        {"碘伏消毒液", "聚维酮碘", "碘酒", 8.0f, {1,0,0,0,0,0}},
        {"红霉素软膏", "红霉素", "消炎膏", 5.0f, {1,0,0,0,0,0}},
        {"云南白药气雾剂", "云南白药", "喷雾", 55.0f, {0,1,0,1,0,0}},

        {"甲硝唑氯己定洗剂", "洁尔阴", "洗液", 25.0f, {0,0,0,0,0,1}},
        {"盐酸特比萘芬乳膏", "丁克", "脚气膏", 18.0f, {0,0,1,1,0,0}},
        {"硝酸咪康唑栓", "达克宁栓", "妇科栓剂", 38.0f, {0,0,0,0,0,1}},

        {"肾上腺素注射液", "副肾", "急救药", 12.0f, {0,1,0,1,0,0}}
    };

    for (int i = 0; i < 20; i++) {
        Medicine* m = (Medicine*)malloc(sizeof(Medicine));
        if (m == NULL) return;
        m->id = 5001 + i;
        (void)strcpy(m->common_name, med_data[i].c_name);
        (void)strcpy(m->trade_name, med_data[i].t_name);
        (void)strcpy(m->alias, med_data[i].alias);
        m->price = med_data[i].price;
        m->purchase_price = m->price * 0.6f;
        m->total_sold = 0;

        for (int j = 0; j < 6; j++) {
            m->allowed_depts[j] = med_data[i].depts[j];
        }

        m->stock = 1000;
        m->next = g_medicine_head;
        g_medicine_head = m;
    }
}

/* 统一的数据加载入口 */
void init_hospital_data() {
    init_doctors();
    init_medicines();
    init_wards_data();
}

/* ==================== 辅助菜单绘制（保留但主菜单已移至 main.cpp） ==================== */
void display_main_menu() {
    clear_screen();
    printf("========================================\n");
    printf("        欢迎来到医院，你是一名...\n");
    printf("========================================\n");
    printf(COLOR_GREEN " 1. 患者\n" COLOR_RESET);
    printf(COLOR_CYAN " 2. 医生\n" COLOR_RESET);
    printf(COLOR_YELLOW " 3. 药房/收费管理人员\n" COLOR_RESET);
    printf(COLOR_MAGENTA " 4. 医院管理人员 (数据分析)\n" COLOR_RESET);
    printf(" 0. 系统退出\n\n");
    printf(" 5. 推进时间一天\n");
    printf(" 6. 重置/初始化系统数据\n");
    printf(" 7. 手动保存数据\n");
    printf("----------------------------------------\n");
}
