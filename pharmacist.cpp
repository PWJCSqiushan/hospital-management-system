/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：pharmacist.cpp
 * 修改摘要：【升级6.0】
 * 1. 所有时间戳获取改为调用模拟时间函数 get_current_datetime_str。
 * 2. 菜单中显示当前系统日期。
 * 3. 保持原有上下键高亮交互菜单及收费、发药、库存、采购功能。
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hospital_system.h"
#include "utils.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RED     "\033[31m"

#define MAX_PURCHASE_QTY 10000

static void show_all_medicines(void);
static void process_payment(void);
static void dispense_drug(void);
static void purchase_medicines(void);
static void draw_pharmacist_menu(int current_select);

static void show_all_medicines() {
    clear_screen();
    printf(COLOR_CYAN "============================== 全院药品库存与进价列表 ==============================\n" COLOR_RESET);
    Medicine* m = g_medicine_head;
    if (!m) {
        printf("暂无药品记录。\n");
    }
    else {
        printf("%-6s | %-20s | %-8s | %-8s | %-6s | %-14s | %s\n",
            "ID", "通用名", "进价", "售价", "库存", "适用科室", "已售");
        printf("------------------------------------------------------------------------------------\n");
        while (m) {
            char dept_str[30] = { 0 };
            if (m->allowed_depts[0] == 1) {
                strcpy(dept_str, "全科通用");
            }
            else {
                for (int i = 1; i <= 5; i++) {
                    if (m->allowed_depts[i]) {
                        if (strlen(dept_str) > 0) strcat(dept_str, "/");
                        switch (i) {
                        case 1: strcat(dept_str, "急"); break;
                        case 2: strcat(dept_str, "内"); break;
                        case 3: strcat(dept_str, "外"); break;
                        case 4: strcat(dept_str, "儿"); break;
                        case 5: strcat(dept_str, "妇"); break;
                        }
                    }
                }
                if (strlen(dept_str) == 0) strcpy(dept_str, "未知");
            }

            printf("%-6d | %-20s | ￥%-6.2f | ￥%-6.2f | %-6d | %-14s | %d\n",
                m->id, m->common_name, m->purchase_price, m->price, m->stock, dept_str, m->total_sold);
            m = m->next;
        }
    }
    printf(COLOR_CYAN "====================================================================================\n" COLOR_RESET);
    press_enter_to_continue();
}

static void process_payment() {
    clear_screen();
    printf(COLOR_YELLOW "--- [收费处 / 结算中心] ---\n" COLOR_RESET);
    int pid = 0;
    printf("请输入待缴费的患者ID: ");
    if (scanf("%d", &pid) != 1) { while (getchar() != '\n'); return; }

    Patient* p = g_patient_head;
    while (p && p->id != pid) p = p->next;

    if (!p) {
        printf(COLOR_RED "\n[错误] 未找到该患者记录。\n" COLOR_RESET);
    }
    else if (p->status == 0) {
        printf(COLOR_YELLOW "\n[提示] 该患者尚未就诊或状态异常，无需缴费。\n" COLOR_RESET);
    }
    else if (p->is_paid == 1) {
        printf(COLOR_GREEN "\n[提示] 该患者已结清当前所有费用，无需重复缴费。\n" COLOR_RESET);
    }
    else if (p->is_paid == 0) {
        float total = p->fee_medicine + p->fee_check + p->fee_treat + p->fee_ward;
        printf("\n================ 账 单 明 细 ================\n");
        printf(" 患者姓名: %-22s  就诊科室: %d\n", p->name, p->dept_id);
        printf(" --------------------------------------------\n");
        printf(" 挂号/诊疗费: %.2f 元\n", p->fee_treat);
        printf(" 检查费用   : %.2f 元\n", p->fee_check);
        printf(" 处方药费   : %.2f 元\n", p->fee_medicine);
        if (p->fee_ward > 0) printf(" 住院床位费 : %.2f 元 (房号: %d)\n", p->fee_ward, p->room_no);
        printf(" --------------------------------------------\n");
        printf(" 应收总额   : " COLOR_YELLOW "%.2f 元\n" COLOR_RESET, total);
        printf("=============================================\n");

        if (ask_yes_no("确认已收到患者款项? (Y/N): ")) {
            MedicalRecord* nr = (MedicalRecord*)malloc(sizeof(MedicalRecord));
            if (nr) {
                (void)strcpy(nr->type, "缴费发票");
                (void)strcpy(nr->doctor_name, "收费处");
                (void)sprintf(nr->content, "诊疗:%.2f 检查:%.2f 药费:%.2f 床位:%.2f | 本次合计:%.2f",
                    p->fee_treat, p->fee_check, p->fee_medicine, p->fee_ward, total);
                get_current_datetime_str(nr->date);
                nr->next = p->record_head;
                p->record_head = nr;
            }
            p->fee_treat = 0.0f; p->fee_check = 0.0f;
            p->fee_medicine = 0.0f; p->fee_ward = 0.0f;
            p->is_paid = 1;
            if (p->status == 1) p->status = 2;
            printf(COLOR_GREEN "\n[成功] 缴费办理完成！电子发票已生成。\n" COLOR_RESET);
        }
    }
    press_enter_to_continue();
}

static void dispense_drug() {
    clear_screen();
    printf(COLOR_YELLOW "--- [中心药房 / 发药窗口] ---\n" COLOR_RESET);
    int pid = 0;
    printf("请输入取药患者ID: ");
    if (scanf("%d", &pid) != 1) { while (getchar() != '\n'); return; }

    Patient* p = g_patient_head;
    while (p && p->id != pid) p = p->next;

    if (!p) {
        printf(COLOR_RED "\n[错误] 未找到该患者记录。\n" COLOR_RESET);
    }
    else if (p->is_paid == 0) {
        printf(COLOR_RED "\n[警告] 拦截！该患者尚未结清费用，拒绝发药！请提示患者前往收费处。\n" COLOR_RESET);
    }
    else if (strstr(p->prescription, "[已发]") != NULL) {
        printf(COLOR_YELLOW "\n[提示] 该患者的处方已发放完毕，切勿重复发药！\n" COLOR_RESET);
    }
    else {
        printf("\n核对处方内容: [%s]\n", p->prescription);

        if (strcmp(p->prescription, "无药处方") == 0 || strcmp(p->prescription, "尚未开具") == 0) {
            printf(">> 该患者无实体药品需发放。\n");
        }
        else {
            char* procurement_tag = strstr(p->prescription, "[缺药待采]");
            if (procurement_tag != NULL) {
                printf(COLOR_RED "\n[拦截] 检测到医生开具了未入库的新药，患者未缴纳此项费用！\n" COLOR_RESET);

                char new_med_name[50] = { 0 };
                int need_qty = 1;
                sscanf(procurement_tag, "[缺药待采]%[^*]*%d", new_med_name, &need_qty);

                Medicine* new_med = (Medicine*)malloc(sizeof(Medicine));
                if (new_med) {
                    new_med->id = (g_medicine_head ? g_medicine_head->id + 1 : 6001);
                    printf(">>> 请配合为新药 [%s] 建立档案，以完成补费流转 <<<\n", new_med_name);
                    strcpy(new_med->common_name, new_med_name);
                    printf("请输入该药的别名/俗名: "); (void)scanf("%s", new_med->alias);
                    strcpy(new_med->trade_name, new_med->alias);
                    printf("请输入进货单价: "); (void)scanf("%f", &new_med->purchase_price);
                    printf("请输入对患者的零售单价: "); (void)scanf("%f", &new_med->price);
                    printf("请输入首批进货数量: "); (void)scanf("%d", &new_med->stock);
                    new_med->total_sold = 0;
                    new_med->allowed_depts[0] = 1;
                    for (int i = 1; i <= 5; i++) new_med->allowed_depts[i] = 0;
                    new_med->next = g_medicine_head;
                    g_medicine_head = new_med;

                    char* tag_pos = strstr(p->prescription, "[缺药待采]");
                    if (tag_pos) {
                        memmove(tag_pos, tag_pos + strlen("[缺药待采]"), strlen(tag_pos + strlen("[缺药待采]")) + 1);
                    }
                    float owe_fee = new_med->price * need_qty;
                    p->fee_medicine += owe_fee;
                    p->is_paid = 0;

                    printf(COLOR_GREEN ">> 新药 [%s] 已入库！\n" COLOR_RESET, new_med->common_name);
                    printf(COLOR_YELLOW ">> [注意] 已将新药费用(%.2f元)计入账单。请患者返回【收费处】补交流水账单后再来取药！\n" COLOR_RESET, owe_fee);
                }
                press_enter_to_continue();
                return;
            }

            printf(">> 正在核验处方单，执行全品类发药及扣库流程...\n");
            char temp_pres[1024] = { 0 };
            strcpy(temp_pres, p->prescription);
            float total_refund = 0.0f;
            char refund_detail[1024] = { 0 };

            // 第一步：预检查所有药品
            int can_fulfill_all = 1;
            struct { Medicine* med; int deduct_qty; float refund; } changes[50];
            int change_count = 0;

            char* token = strtok(temp_pres, ",");
            while (token != NULL) {
                while (*token == ' ') token++;
                char d_name[100] = { 0 };
                int d_qty = 1;
                char* star_pos = strchr(token, '*');
                if (star_pos != NULL) {
                    strncpy(d_name, token, star_pos - token);
                    d_name[star_pos - token] = '\0';
                    d_qty = atoi(star_pos + 1);
                }
                else { strcpy(d_name, token); }

                Medicine* m = g_medicine_head;
                int found = 0;
                while (m) {
                    if (strcmp(m->common_name, d_name) == 0 ||
                        strcmp(m->trade_name, d_name) == 0 ||
                        strcmp(m->alias, d_name) == 0) {
                        found = 1;
                        if (m->stock < d_qty) can_fulfill_all = 0;
                        if (change_count < 50) {
                            changes[change_count].med = m;
                            changes[change_count].deduct_qty = (m->stock < d_qty) ? m->stock : d_qty;
                            changes[change_count].refund = (m->stock < d_qty) ? (d_qty - m->stock) * m->price : 0.0f;
                            change_count++;
                        }
                        break;
                    }
                    m = m->next;
                }
                if (!found) printf(COLOR_YELLOW "   [跳过] '%s' 匹配失败，跳过发药。\n" COLOR_RESET, d_name);
                token = strtok(NULL, ",");
            }

            // 第二步：统一执行扣减
            for (int i = 0; i < change_count; i++) {
                changes[i].med->stock -= changes[i].deduct_qty;
                changes[i].med->total_sold += changes[i].deduct_qty;
                if (changes[i].refund > 0) {
                    total_refund += changes[i].refund;
                    char temp_ref[256];
                    snprintf(temp_ref, sizeof(temp_ref), "%s缺%d份(退%.2f元) ",
                        changes[i].med->common_name,
                        (int)(changes[i].refund / changes[i].med->price),
                        changes[i].refund);
                    strcat(refund_detail, temp_ref);
                }
                else {
                    printf(COLOR_GREEN "   [出库成功] " COLOR_YELLOW "%s x%d" COLOR_GREEN " | 剩余库存: %d\n" COLOR_RESET,
                        changes[i].med->common_name, changes[i].deduct_qty, changes[i].med->stock);
                }
            }

            if (total_refund > 0) {
                printf(COLOR_CYAN "\n>> [财务处理] 由于缺药，系统已为患者生成退费凭证：共计退费 ￥%.2f\n" COLOR_RESET, total_refund);
                MedicalRecord* nr = (MedicalRecord*)malloc(sizeof(MedicalRecord));
                if (nr) {
                    (void)strcpy(nr->type, "退费凭证");
                    (void)strcpy(nr->doctor_name, "中心药房");
                    (void)sprintf(nr->content, "缺货退费 | 明细:%s| 退款金额:%.2f", refund_detail, total_refund);
                    get_current_datetime_str(nr->date);
                    nr->next = p->record_head;
                    p->record_head = nr;
                }
            }
        }

        char new_pres[1024] = { 0 };
        sprintf(new_pres, "[已发]%s", p->prescription);
        strcpy(p->prescription, new_pres);

        if (p->need_inpatient == 1) printf(COLOR_YELLOW "\n[完成] 发药结束。提示：患者需转入住院部终端自助办理床位分配！\n" COLOR_RESET);
        else if (p->need_inpatient == 2) { p->status = 3; printf(COLOR_YELLOW "\n[完成] 发药结束。患者已安排急诊ICU床位，转入住院部！\n" COLOR_RESET); }
        else { if (p->status != 3) p->status = 4; printf(COLOR_GREEN "\n[完成] 发药流程结束。本次就诊已完结！\n" COLOR_RESET); }
    }
    press_enter_to_continue();
}

static void purchase_medicines() {
    clear_screen();
    printf(COLOR_CYAN "--- [药房库房 / 采购进货] ---\n" COLOR_RESET);
    char sname[50];
    printf("请输入需要补库存的药品名称(学名/商品名/别名): ");
    (void)scanf("%s", sname);

    Medicine* m = g_medicine_head;
    while (m) {
        if (strcmp(m->common_name, sname) == 0 || strcmp(m->trade_name, sname) == 0 || strcmp(m->alias, sname) == 0) {
            printf("当前药品: [%s] | 现有库存: %d\n", m->common_name, m->stock);
            int add_qty = 0;
             
            while (1) {
                printf("请输入采购进货的数量(单次最多允许 %d): ", MAX_PURCHASE_QTY);
                if (scanf("%d", &add_qty) == 1) {
                    if (add_qty <= 0) {
                        printf(COLOR_RED "   [错误] 数量必须大于0，请重新输入。\n" COLOR_RESET);
                        continue;
                    }
                    if (add_qty > MAX_PURCHASE_QTY) {
                        printf(COLOR_RED "   [系统限制] 进货数量过大！系统限制单次最大进货数量为: %d。请重新输入。\n" COLOR_RESET, MAX_PURCHASE_QTY);
                        continue;
                    }
                    break;
                }
                else {
                    while (getchar() != '\n');
                    printf(COLOR_RED "   [错误] 输入无效，请输入纯数字。\n" COLOR_RESET);
                }
            }

            m->stock += add_qty;
            printf(COLOR_GREEN ">> 进货成功！该药品最新库存为: %d\n" COLOR_RESET, m->stock);
            press_enter_to_continue();
            return;
        }
        m = m->next;
    }
    /* 未找到该药品，询问是否要新建档案 */
    if (!ask_yes_no(COLOR_YELLOW "未在药库中找到，是否要创建新药品档案并进货？(Y/N): " COLOR_RESET)) {
        printf(COLOR_RED "操作已取消，未新增药品。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    Medicine* new_med = (Medicine*)malloc(sizeof(Medicine));
    if (!new_med) {
        printf(COLOR_RED "内存分配失败，无法创建新药品。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }
    memset(new_med, 0, sizeof(Medicine));

    /* 生成ID */
    int max_id = 6000;
    Medicine* mm = g_medicine_head;
    while (mm) { if (mm->id > max_id) max_id = mm->id; mm = mm->next; }
    new_med->id = max_id + 1;

    strncpy(new_med->common_name, sname, sizeof(new_med->common_name)-1);
    printf("请输入该药的商品名(回车则使用学名): ");
    char tmp[64] = {0};
    while (getchar() != '\n');
    fgets(tmp, sizeof(tmp), stdin);
    if (tmp[0] == '\n' || tmp[0] == '\0') strcpy(new_med->trade_name, new_med->common_name);
    else { tmp[strcspn(tmp, "\n")] = '\0'; strncpy(new_med->trade_name, tmp, sizeof(new_med->trade_name)-1); }

    printf("请输入该药的别名/俗名(回车留空): ");
    fgets(tmp, sizeof(tmp), stdin);
    tmp[strcspn(tmp, "\n")] = '\0'; if (tmp[0] != '\0') strncpy(new_med->alias, tmp, sizeof(new_med->alias)-1);

    printf("请输入进货单价: "); (void)scanf("%f", &new_med->purchase_price);
    printf("请输入对患者的零售单价: "); (void)scanf("%f", &new_med->price);
    printf("请输入首批进货数量: "); (void)scanf("%d", &new_med->stock);
    new_med->total_sold = 0;

    /* 设定适用科室：严格只接受 Y/N */
    if (ask_yes_no("是否为全科通用药？(Y/N): ")) {
        new_med->allowed_depts[0] = 1;
        for (int i = 1; i <= 5; i++) new_med->allowed_depts[i] = 0;
    }
    else {
        new_med->allowed_depts[0] = 0;
        for (int i = 1; i <= 5; i++) {
            char prompt[128];
            snprintf(prompt, sizeof(prompt), "科室 %d (%s) 可用? (Y/N): ", i, get_dept_name(i));
            new_med->allowed_depts[i] = ask_yes_no(prompt);
        }
    }

    int cat = 0;
    while (1) { printf("请输入药品类别 (0:甲类 1:乙类 2:丙类): "); if (scanf("%d", &cat)==1 && cat>=0 && cat<=2) break; while (getchar()!='\n'); printf(COLOR_RED "输入无效\n" COLOR_RESET); }
    new_med->category = cat;

    new_med->next = g_medicine_head;
    g_medicine_head = new_med;

    printf(COLOR_GREEN ">> 新药已建档并入库: ID=%d 名称=%s 库存=%d\n" COLOR_RESET, new_med->id, new_med->common_name, new_med->stock);
    press_enter_to_continue();
}

static void draw_pharmacist_menu(int current_select) {
    clear_screen();
    printf(COLOR_YELLOW "========================================\n");
    printf("            收费与药房管理终端\n");
    printf("========================================\n" COLOR_RESET);

    const char* options[] = {
        " 0. 退出登录",
        " 1. 办理患者缴费 (防逃费拦截)",
        " 2. 药房核对发药 (扣减库存)",
        " 3. 查看全院药品及库存明细",
        " 4. 药房补货采购进货"
    };
    int option_codes[] = { 0, 1, 2, 3, 4 };

    for (int i = 0; i < 5; i++) {
        if (i == current_select) {
            printf("\033[43;30m%s\033[0m\n", options[i]);
        }
        else {
            if (option_codes[i] == 4) printf(COLOR_CYAN "%s\n" COLOR_RESET, options[i]);
            else printf("%s\n", options[i]);
        }
    }
    printf("----------------------------------------\n");
    printf("当前系统日期: %s\n", g_current_date);
    printf("请使用键盘【↑】【↓】选择，按【Enter】确认，或直接输入数字键。\n");
}

void pharmacist_function() {
    clear_screen();
    char acc[20] = { 0 }, pwd[20] = { 0 };
    printf("管理员账号 (默认 admin): "); (void)scanf("%s", acc);
    printf("管理员密码 (默认 123): "); (void)scanf("%s", pwd);

    if (strcmp(acc, "admin") != 0 || strcmp(pwd, "123") != 0) {
        printf(COLOR_RED "\n[错误] 账号或密码不正确！\n" COLOR_RESET);
        press_enter_to_continue(); return;
    }

    int current_select = 1;
    int option_codes[] = { 0, 1, 2, 3, 4 };
    int max_options = 5;
    int run = 1;

    while (run) {
        draw_pharmacist_menu(current_select);
        int key = get_single_key();

        if (key == 224) {
            key = get_single_key();
            if (key == 72) {
                current_select--;
                if (current_select < 0) current_select = max_options - 1;
            }
            else if (key == 80) {
                current_select++;
                if (current_select >= max_options) current_select = 0;
            }
        }
        else if (key == 13) {
            int choice = option_codes[current_select];
            switch (choice) {
            case 1: process_payment(); break;
            case 2: dispense_drug(); break;
            case 3: show_all_medicines(); break;
            case 4: purchase_medicines(); break;
            case 0: run = 0; break;
            }
        }
        else if (key >= '0' && key <= '4') {
            int direct_choice = key - '0';
            for (int i = 0; i < max_options; i++) {
                if (option_codes[i] == direct_choice) {
                    current_select = i;
                    switch (direct_choice) {
                    case 1: process_payment(); break;
                    case 2: dispense_drug(); break;
                    case 3: show_all_medicines(); break;
                    case 4: purchase_medicines(); break;
                    case 0: run = 0; break;
                    }
                    break;
                }
            }
        }
    }
}