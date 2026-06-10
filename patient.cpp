/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：patient.cpp
 * 修改摘要：【升级6.0 最终修正】
 * 1. 复查挂号增加状态拦截，仅允许状态4的患者再次挂号。
 * 2. 新增“预约报到”功能，预约患者就诊当天报到激活，自动沿用预约信息。
 * 3. 预约报到时间固定为08:00，确保就诊顺序优先。
 * 4. 姓名验证放宽，允许中文。
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "hospital_system.h"
#include "utils.h"
#include "doctor.h"
#include "inpatient.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"

static void register_patient_flow(void);
static void appointment_flow(void);
static void appointment_checkin(void);          // 新增：预约报到
static void appointment_manage(void); // 查看/取消预约
static void view_medical_advice(void);
static void view_invoice(void);
static void view_my_registration_slip(void);
static Patient* find_patient_by_id_or_phone(const char* action_name);
static int get_waiting_count(int doctor_id);
static int get_appointment_count(int doctor_id, const char* date, int period);
static void draw_patient_menu(int current_select);
static Patient* register_new_patient(void);
static void patient_sign_in(void);
static void appointment_manage(void);

const char* get_status_desc(int status) {
    switch (status) {
    case 0: return "已挂号，待就诊";
    case 1: return "看诊毕，待缴费";
    case 2: return "已缴费，待取药/办理住院";
    case 3: return "已住院";
    case 4: return "就诊结束";
    default: return "状态未知";
    }
}

/* 查看/取消预约（仅能取消未就诊的预约） */
static void appointment_manage(void) {
    clear_screen();
    printf(COLOR_CYAN "--- [查看/取消预约] ---\n" COLOR_RESET);

    Patient* p = find_patient_by_id_or_phone("查看/取消预约");
    if (!p) { press_enter_to_continue(); return; }

    // 列出该患者的预约
    Appointment* a = g_appointment_head;
    int found = 0;
    int idx = 1;
    int total = 0;
    while (a) { if (a->patient_id == p->id) total++; a = a->next; }
    if (total == 0) {
        printf(COLOR_YELLOW "\n未发现您的任何预约记录。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    a = g_appointment_head;
    printf("\n您的预约列表：\n");
    while (a) {
        if (a->patient_id == p->id) {
            Doctor* d = get_doctor_by_id(a->doctor_id);
            printf(" %d. 医生:%s (工号:%d) 日期:%s 时段:%s\n", idx++, d ? d->name : "未知", a->doctor_id, a->date, a->period==0?"上午":"下午");
            found = 1;
        }
        a = a->next;
    }

    if (!found) {
        printf(COLOR_YELLOW "\n未发现您的任何预约记录。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    // 询问是否取消所有预约
    if (!ask_yes_no("\n是否取消所有预约？(Y/N): ")) {
        printf(COLOR_YELLOW "\n操作已取消，保留原预约。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    // 删除该患者的所有预约记录
    Appointment* cur = g_appointment_head;
    Appointment* prev = NULL;
    while (cur) {
        if (cur->patient_id == p->id) {
            Appointment* to_del = cur;
            if (prev) prev->next = cur->next;
            else g_appointment_head = cur->next;
            cur = cur->next;
            free(to_del);
        }
        else {
            prev = cur;
            cur = cur->next;
        }
    }

    // 添加病历记录说明已取消预约
    MedicalRecord* mr = (MedicalRecord*)malloc(sizeof(MedicalRecord));
    if (mr) {
        strcpy(mr->type, "系统处理");
        strcpy(mr->doctor_name, "系统");
        strcpy(mr->content, "患者取消预约。");
        get_current_datetime_str(mr->date);
        mr->next = p->record_head;
        p->record_head = mr;
    }

    printf(COLOR_GREEN "\n已取消全部预约。\n" COLOR_RESET);
    press_enter_to_continue();
}

static Patient* register_new_patient(void) {
    Patient* p = (Patient*)malloc(sizeof(Patient));
    if (p == NULL) return NULL;
    memset(p, 0, sizeof(Patient));

    while (1) {
        printf("姓名 (支持中英文，不含数字和特殊符号): ");
        (void)scanf("%29s", p->name);
        int valid = 1;
        for (int i = 0; p->name[i] != '\0'; i++) {
            unsigned char ch = (unsigned char)p->name[i];
            if (isdigit(ch) || ch == '@' || ch == '#' || ch == '$' || ch == '%' || ch == '&' || ch == '*') {
                valid = 0;
                break;
            }
        }
        if (valid) break;
        printf(COLOR_RED "[错误] 姓名不能包含数字或特殊符号，请重新输入。\n" COLOR_RESET);
    }

    while (1) {
        printf("性别 (男/女/其他): ");
        (void)scanf("%9s", p->gender);
        if (strcmp(p->gender, "男") == 0 || strcmp(p->gender, "女") == 0 || strcmp(p->gender, "其他") == 0) break;
        printf(COLOR_RED "[错误] 性别输入不合法，请仅输入\"男\"、\"女\"或\"其他\"。\n" COLOR_RESET);
    }

    while (1) {
        int y, m, d;
        printf("出生日期 (格式 YYYY-MM-DD): ");
        (void)scanf("%11s", p->birth_date);
        if (strlen(p->birth_date) == 10 && sscanf(p->birth_date, "%d-%d-%d", &y, &m, &d) == 3) {
            if (y > 1900 && y < 2027 && m >= 1 && m <= 12 && d >= 1 && d <= 31) break;
        }
        printf(COLOR_RED "[错误] 日期格式非法或内容不符合逻辑，请重新输入。\n" COLOR_RESET);
    }

    while (1) {
        printf("电话 (必须11位纯数字): ");
        (void)scanf("%19s", p->phone);
        int valid = 1;
        if (strlen(p->phone) != 11) valid = 0;
        for (size_t i = 0; i < strlen(p->phone); i++) {
            if (!isdigit(p->phone[i])) valid = 0;
        }
        if (valid) break;
        printf(COLOR_RED "[错误] 手机号必须为11位纯数字，请重新输入。\n" COLOR_RESET);
    }

    int max_id = 2026000;
    Patient* temp = g_patient_head;
    while (temp != NULL) {
        if (temp->id > max_id) max_id = temp->id;
        temp = temp->next;
    }
    p->id = max_id + 1;

    p->record_head = NULL;
    p->fee_medicine = 0.0f;
    p->fee_check = 0.0f;
    p->fee_treat = 0.0f;
    p->fee_ward = 0.0f;
    p->is_paid = 0;
    p->room_no = 0;
    p->ward_type = 0;
    p->need_inpatient = 0;

    p->next = g_patient_head;
    g_patient_head = p;

    /* 询问医保类型 */
    while (1) {
        printf("医保类型 (0: 无医保 1: 居民医保 2: 职工医保): ");
        if (scanf("%d", &p->insurance_type) == 1 && (p->insurance_type >= 0 && p->insurance_type <= 2)) break;
        while (getchar() != '\n');
        printf(COLOR_RED "[错误] 请输入 0、1 或 2。\n" COLOR_RESET);
    }
    p->checked_in = 0;

    printf(COLOR_GREEN "\n建档成功！您的患者ID为: %d，请牢记。\n" COLOR_RESET, p->id);
    press_enter_to_continue();
    return p;
}

// 自助缴费（支持按ID或手机号验证）
static void patient_self_payment(void) {
    clear_screen();
    printf(COLOR_YELLOW "--- [自助缴费] ---\n" COLOR_RESET);

    int auth_way = 0;
    printf("请选择验证方式：1. 患者ID  2. 手机号\n请输入选择: ");
    if (scanf("%d", &auth_way) != 1 || (auth_way != 1 && auth_way != 2)) { while (getchar() != '\n'); printf(COLOR_RED "无效输入。\n" COLOR_RESET); return; }

    Patient* p = NULL;
    if (auth_way == 1) {
        int pid = 0;
        printf("请输入患者ID: ");
        if (scanf("%d", &pid) != 1) { while (getchar() != '\n'); printf(COLOR_RED "无效ID输入。\n" COLOR_RESET); return; }
        p = g_patient_head;
        while (p && p->id != pid) p = p->next;
    }
    else {
        char phone[32];
        printf("请输入预留手机号以验证身份: ");
        if (scanf("%31s", phone) != 1) { while (getchar() != '\n'); return; }
        p = g_patient_head;
        while (p && strcmp(p->phone, phone) != 0) p = p->next;
    }

    if (!p) {
        printf(COLOR_RED "\n[错误] 未找到该患者记录。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    if (p->status == 0) {
        printf(COLOR_YELLOW "\n[提示] 该患者尚未就诊或状态异常，无需缴费。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }
    if (p->is_paid == 1) {
        printf(COLOR_GREEN "\n[提示] 您的费用已结清，无需重复缴费。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    float total = p->fee_medicine + p->fee_check + p->fee_treat + p->fee_ward;
    printf("\n================ 账 单 明 细 ================\n");
    printf(" 患者姓名: %-22s  就诊科室: %s\n", p->name, get_dept_name(p->dept_id));
    printf(" 医保类型: %s\n", get_insurance_type_name(p->insurance_type));
    printf(" --------------------------------------------\n");
    printf(" 挂号/诊疗费: %.2f 元\n", p->fee_treat);
    printf(" 检查费用   : %.2f 元\n", p->fee_check);
    printf(" 处方药费   : %.2f 元\n", p->fee_medicine);
    if (p->fee_ward > 0) printf(" 住院床位费 : %.2f 元 (房号: %d)\n", p->fee_ward, p->room_no);
    printf(" --------------------------------------------\n");
    printf(" 应收总额   : " COLOR_YELLOW "%.2f 元\n" COLOR_RESET, total);
    printf("=============================================\n");

    int use_insurance = 0;
    if (p->insurance_type != 0) {
        if (ask_yes_no("\n是否使用医保报销？(Y/N): ")) use_insurance = 1;
    }

    float patient_pay = 0.0f, insurance_pay = 0.0f;
    if (use_insurance) {
        calculate_insurance_reimbursement(p, 1, &patient_pay, &insurance_pay);
        printf("\n========== 医保报销明细 ==========");
        printf("\n 总费用: %.2f 元\n", total);
        printf(" 医保报销: %.2f 元\n", insurance_pay);
        printf(" 个人自付: %.2f 元\n", patient_pay);
        printf(" ==================================\n");
    }
    else { patient_pay = total; insurance_pay = 0.0f; }

    if (ask_yes_no("\n确认已收到患者款项？(Y/N): ")) {
        MedicalRecord* nr = (MedicalRecord*)malloc(sizeof(MedicalRecord));
        if (nr) {
            (void)strcpy(nr->type, "缴费发票");
            (void)strcpy(nr->doctor_name, "患者自助");
            if (use_insurance) {
                sprintf(nr->content, "诊疗:%.2f 检查:%.2f 药费:%.2f 床位:%.2f | 医保报销:%.2f | 个人支付:%.2f | 本次合计:%.2f",
                    p->fee_treat, p->fee_check, p->fee_medicine, p->fee_ward, insurance_pay, patient_pay, total);
            } else {
                sprintf(nr->content, "诊疗:%.2f 检查:%.2f 药费:%.2f 床位:%.2f | 本次合计:%.2f",
                    p->fee_treat, p->fee_check, p->fee_medicine, p->fee_ward, total);
            }
            get_current_datetime_str(nr->date);
            nr->next = p->record_head;
            p->record_head = nr;
        }

        if (use_insurance && insurance_pay > 0) {
            MedicalRecord* ir = (MedicalRecord*)malloc(sizeof(MedicalRecord));
            if (ir) {
                strcpy(ir->type, "医保报销");
                strcpy(ir->doctor_name, "医保系统");
                sprintf(ir->content, "报销金额:%.2f元，个人支付:%.2f元", insurance_pay, patient_pay);
                get_current_datetime_str(ir->date);
                ir->next = p->record_head;
                p->record_head = ir;
            }
        }

        p->fee_treat = 0.0f; p->fee_check = 0.0f; p->fee_medicine = 0.0f; p->fee_ward = 0.0f;
        p->is_paid = 1;
        if (p->status == 1) p->status = 2;
        printf(COLOR_GREEN "\n[成功] 缴费办理完成！电子发票已生成。\n" COLOR_RESET);
        if (use_insurance) printf(" 其中医保报销: %.2f 元\n", insurance_pay);
    }

    press_enter_to_continue();
}

static Patient* find_patient_by_id_or_phone(const char* action_name) {
    int way = 0;
    printf("\n>>> 即将办理 [%s] <<<\n", action_name);
    printf(" 1. 按患者ID查询\n 2. 按手机号查询\n 请选择认证方式: ");
    if (scanf("%d", &way) != 1 || (way != 1 && way != 2)) {
        while (getchar() != '\n');
        printf(COLOR_RED "无效输入。\n" COLOR_RESET);
        return NULL;
    }

    Patient* p = g_patient_head;
    if (way == 1) {
        int pid;
        printf("请输入患者ID: ");
        (void)scanf("%d", &pid);
        while (p && p->id != pid) p = p->next;
    }
    else {
        char phone[20];
        printf("请输入11位预留手机号: ");
        (void)scanf("%s", phone);
        while (p && strcmp(p->phone, phone) != 0) p = p->next;
    }

    if (p != NULL) {
        printf(COLOR_CYAN "\n[系统找到匹配记录，请核对患者身份]\n" COLOR_RESET);
        printf(" ID:%-8d | 姓名:%-22s | 性别:%-2s | 生日:%s\n 手机:%-11s | 最近挂号科室:%s\n",
            p->id, p->name, p->gender, p->birth_date, p->phone, get_dept_name(p->dept_id));

        if (ask_yes_no("\n这是您吗？ 输入 Y 进入下一步，输入 N 取消: ")) {
            return p;
        }
        else {
            printf(COLOR_YELLOW ">> 操作已取消。\n" COLOR_RESET);
            return NULL;
        }
    }
    printf(COLOR_RED "未查找到匹配的患者记录。\n" COLOR_RESET);
    return NULL;
}

static int get_waiting_count(int doctor_id) {
    int count = 0;
    Patient* p = g_patient_head;
    while (p) {
        if (p->doctor_id == doctor_id && p->status == 0) count++;
        p = p->next;
    }
    return count;
}

static int get_appointment_count(int doctor_id, const char* date, int period) {
    int count = 0;
    Appointment* a = g_appointment_head;
    while (a) {
        if (a->doctor_id == doctor_id && strcmp(a->date, date) == 0 && a->period == period) {
            count++;
        }
        a = a->next;
    }
    return count;
}

void register_patient_flow() {
    clear_screen();
    printf(COLOR_CYAN "--- [患者挂号登记] ---\n" COLOR_RESET);

    int reg_type = 0;
    while (1) {
        printf(" 1. 新患者注册登记\n 2. 复查患者挂号 (仅限就诊结束患者)\n 请选择: ");
        if (scanf("%d", &reg_type) == 1 && (reg_type == 1 || reg_type == 2)) break;
        printf(COLOR_RED "[错误] 指令无效，请重新输入数字 1 或 2！\n" COLOR_RESET);
        while (getchar() != '\n');
    }

    Patient* p = NULL;
    if (reg_type == 2) {
        p = find_patient_by_id_or_phone("复诊挂号");
        if (p == NULL) {
            printf(COLOR_YELLOW ">> 认证失败或取消，系统将返回。\n" COLOR_RESET);
            press_enter_to_continue();
            return;
        }
        /* 如果该患者已有预约，禁止重复现场挂号 */
        {
            Appointment* a = g_appointment_head;
            int has_app = 0;
            while (a) {
                if (a->patient_id == p->id) { has_app = 1; break; }
                a = a->next;
            }
            if (has_app) {
                printf(COLOR_YELLOW "\n检测到您已有预约，不能进行现场复诊挂号。如需改为现场挂号，请先取消预约。\n" COLOR_RESET);
                press_enter_to_continue();
                return;
            }
        }
        // 检查状态：只允许状态为4（就诊结束）的患者复查挂号
        if (p->status != 4) {
            printf(COLOR_RED "\n[拦截] 您当前就诊流程尚未结束（状态：%s），请先完成或结束当前就诊后再挂号。\n" COLOR_RESET, get_status_desc(p->status));
            press_enter_to_continue();
            return;
        }
        printf(COLOR_GREEN ">> 欢迎回来，%s！\n" COLOR_RESET, p->name);
        // 重置费用和状态，开始新就诊周期
        p->fee_treat = 0.0f; p->fee_check = 0.0f;
        p->fee_medicine = 0.0f; p->fee_ward = 0.0f;
        (void)strcpy(p->prescription, "尚未开具");
        p->need_inpatient = 0;
        p->room_no = 0;
        p->ward_type = 0;
        p->is_paid = 0;
    }

    if (reg_type == 1) {
        p = register_new_patient();
        if (p == NULL) {
            printf(COLOR_RED "注册失败，请重试。\n" COLOR_RESET);
            press_enter_to_continue();
            return;
        }
    }

    /* 如果已有预约，禁止再次预约 */
    if (p != NULL) {
        Appointment* acheck = g_appointment_head;
        while (acheck) {
            if (acheck->patient_id == p->id) {
                printf(COLOR_YELLOW "\n检测到您已有预约，不能重复预约。如需更改请先取消现有预约。\n" COLOR_RESET);
                press_enter_to_continue();
                return;
            }
            acheck = acheck->next;
        }
    }

    int age = CalculateAge(p->birth_date);
    int is_male = (strcmp(p->gender, "男") == 0);

    while (1) {
        printf("\n就诊科室 (1:急诊 2:内科 3:外科 4:儿科 5:妇科): ");
        if (scanf("%d", &p->dept_id) != 1) {
            while (getchar() != '\n');
            continue;
        }

        if (p->dept_id < 1 || p->dept_id > 5) {
            printf(COLOR_RED "[错误] 无效的科室编号，请重新输入。\n" COLOR_RESET);
            continue;
        }
        if (age < 18 && p->dept_id != 1 && p->dept_id != 4) {
            printf(COLOR_RED "[拦截] 未满18岁的未成年患者只能挂 急诊科(1) 或 儿科(4)！\n" COLOR_RESET);
            continue;
        }
        if (age >= 18 && p->dept_id == 4) {
            printf(COLOR_RED "[拦截] 18岁及以上的成年患者不能挂 儿科(4)！\n" COLOR_RESET);
            continue;
        }
        if (is_male && p->dept_id == 5) {
            printf(COLOR_RED "[拦截] 男性患者不能挂 妇科(5)！\n" COLOR_RESET);
            continue;
        }
        break;
    }

    Doctor* selected_doc = NULL;

    if (p->dept_id == 1) {
        printf(COLOR_YELLOW ">> 您选择了急诊科，系统正为您极速分配急诊值班医师...\n" COLOR_RESET);
        Doctor* d = g_doctor_head;
        while (d != NULL) {
            if (d->dept_id == 1 && d->is_active == 1) {
                selected_doc = d;
                break;
            }
            d = d->next;
        }
    }
    else {
        printf("\n" COLOR_CYAN "============== 【 %s 】排班医生列表 ==============\n" COLOR_RESET, get_dept_name(p->dept_id));
        printf("%-6s | %-22s | %-12s | %-8s | %-8s\n", "工号", "姓名", "职级", "待诊人数", "挂号费");
        printf("--------------------------------------------------------------------------------\n");

        Doctor* d = g_doctor_head;
        int doc_count = 0;
        while (d != NULL) {
            if (d->dept_id == p->dept_id && d->is_active == 1) {
                printf("%-6d | %-22s | %-12s | %-8d | ￥%.2f\n",
                    d->id, d->name, get_title_name(d->title_level),
                    get_waiting_count(d->id), d->consultation_fee);
                doc_count++;
            }
            d = d->next;
        }

        if (doc_count == 0) {
            printf(COLOR_RED "\n[提示] 该科室当前无在职医生，挂号失败。\n" COLOR_RESET);
            press_enter_to_continue();
            return;
        }

        printf("--------------------------------------------------------------------------------\n");
        while (1) {
            int input_doc_id = 0;
            printf("请输入您选择的医生工号: ");
            (void)scanf("%d", &input_doc_id);

            d = g_doctor_head;
            while (d) {
                if (d->id == input_doc_id && d->dept_id == p->dept_id && d->is_active == 1) {
                    selected_doc = d;
                    break;
                }
                d = d->next;
            }
            if (selected_doc != NULL) break;
            printf(COLOR_RED "[错误] 未找到该工号的在职医生，请重试。\n" COLOR_RESET);
        }
    }

    if (selected_doc == NULL) {
        printf(COLOR_RED "\n[系统错误] 医生分配失败。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    p->doctor_id = selected_doc->id;
    p->status = 0;
    p->fee_treat += selected_doc->consultation_fee;

    MedicalRecord* r = (MedicalRecord*)malloc(sizeof(MedicalRecord));
    if (r != NULL) {
        (void)strcpy(r->type, "挂号");
        (void)strcpy(r->doctor_name, "系统中心");
        (void)sprintf(r->content, "科室:%s, 医生:%s, 挂号费:%.2f元", get_dept_name(p->dept_id), selected_doc->name, selected_doc->consultation_fee);
        get_current_datetime_str(r->date);
        r->next = p->record_head;
        p->record_head = r;
    }

    (void)strcpy(p->prescription, "尚未开具");
    (void)strcpy(p->instructions, "请在候诊区耐心等待");
    p->checked_in = 0;  // 初始化签到状态

    printf(COLOR_GREEN "\n正在打印挂号单……挂号单打印成功！\n" COLOR_RESET);
    printf("----------------------------------------\n");
    printf("        医院门诊挂号单 (存根)\n");
    printf("----------------------------------------\n");
    printf(" 序号: %-10d (待诊状态)\n", p->id);
    printf(" 姓名: %-22s  性别: %-4s\n", p->name, p->gender);
    printf(" 科室: %-10s  医生: %-10s (%s)\n", get_dept_name(p->dept_id), selected_doc->name, get_title_name(selected_doc->title_level));
    printf(" 挂号费: %.2f 元\n", selected_doc->consultation_fee);
    printf(" 状态: %s\n", get_status_desc(p->status));
    printf("----------------------------------------\n");
    if (p->dept_id != 1) {  // 非急诊科患者
        printf("\n[重要提示] 挂号完成后请及时在终端进行签到，以免延误就诊！\n");
    }
    press_enter_to_continue();
}

static void appointment_flow(void) {
    clear_screen();
    printf(COLOR_CYAN "--- [预约挂号] ---\n" COLOR_RESET);

    Patient* p = NULL;
    int auth_choice = 0;

    printf("\n请选择身份认证方式：\n");
    printf(" 1. 按患者ID查询\n");
    printf(" 2. 按手机号查询\n");
    printf(" 3. 新患者注册并预约\n");
    printf(" 0. 取消\n");
    printf("请选择: ");
    if (scanf("%d", &auth_choice) != 1) {
        while (getchar() != '\n');
        printf(COLOR_RED "无效输入。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }
    while (getchar() != '\n');

    if (auth_choice == 0) {
        printf(COLOR_YELLOW ">> 操作已取消。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }
    else if (auth_choice == 3) {
        p = register_new_patient();
        if (p == NULL) {
            printf(COLOR_RED "注册失败，预约取消。\n" COLOR_RESET);
            press_enter_to_continue();
            return;
        }
    }
    else if (auth_choice == 1 || auth_choice == 2) {
        p = find_patient_by_id_or_phone("预约挂号");
        if (p == NULL) {
            printf(COLOR_YELLOW ">> 认证失败或取消，系统将返回。\n" COLOR_RESET);
            press_enter_to_continue();
            return;
        }
    }
    else {
        printf(COLOR_RED "无效选项。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    int age = CalculateAge(p->birth_date);
    int is_male = (strcmp(p->gender, "男") == 0);
    int dept_id;

    while (1) {
        printf("\n预约科室 (1:急诊 2:内科 3:外科 4:儿科 5:妇科): ");
        if (scanf("%d", &dept_id) != 1) {
            while (getchar() != '\n');
            continue;
        }
        if (dept_id < 1 || dept_id > 5) {
            printf(COLOR_RED "[错误] 无效的科室编号，请重新输入。\n" COLOR_RESET);
            continue;
        }
        if (dept_id == 1) {
            printf(COLOR_RED "[拦截] 急诊科不接受预约挂号，请选择其他科室或改为现场挂号。\n" COLOR_RESET);
            continue;
        }
        if (age < 18 && dept_id != 1 && dept_id != 4) {
            printf(COLOR_RED "[拦截] 未满18岁的未成年患者只能预约 急诊科(1) 或 儿科(4)！\n" COLOR_RESET);
            continue;
        }
        if (age >= 18 && dept_id == 4) {
            printf(COLOR_RED "[拦截] 18岁及以上的成年患者不能预约 儿科(4)！\n" COLOR_RESET);
            continue;
        }
        if (is_male && dept_id == 5) {
            printf(COLOR_RED "[拦截] 男性患者不能预约 妇科(5)！\n" COLOR_RESET);
            continue;
        }
        break;
    }

    printf("\n" COLOR_CYAN "============== 【 %s 】可预约医生 ==============\n" COLOR_RESET, get_dept_name(dept_id));
    printf("%-6s | %-22s | %-12s\n", "工号", "姓名", "职级");
    printf("----------------------------------------------\n");

    Doctor* d = g_doctor_head;
    int doc_count = 0;
    while (d) {
        if (d->dept_id == dept_id && d->is_active == 1) {
            printf("%-6d | %-22s | %-12s\n", d->id, d->name, get_title_name(d->title_level));
            doc_count++;
        }
        d = d->next;
    }

    if (doc_count == 0) {
        printf(COLOR_RED "\n该科室暂无在职医生可预约。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    int doctor_id;
    while (1) {
        printf("\n请输入要预约的医生工号: ");
        if (scanf("%d", &doctor_id) != 1) {
            while (getchar() != '\n');
            printf(COLOR_RED "输入无效。\n" COLOR_RESET);
            continue;
        }
        d = g_doctor_head;
        while (d) {
            if (d->id == doctor_id && d->dept_id == dept_id && d->is_active == 1) break;
            d = d->next;
        }
        if (d) break;
        printf(COLOR_RED "工号无效或医生不在职，请重试。\n" COLOR_RESET);
    }

    char available_dates[7][12];
    char temp_date[12];
    get_current_date_str(temp_date);
    strcpy(available_dates[0], temp_date);

    int year, month, day;
    sscanf(temp_date, "%d-%d-%d", &year, &month, &day);
    for (int i = 1; i < 7; i++) {
        day++;
        if (day > 30) {
            day = 1;
            month++;
            if (month > 12) {
                month = 1;
                year++;
            }
        }
        sprintf(available_dates[i], "%04d-%02d-%02d", year, month, day);
    }

    printf("\n可选预约日期：\n");
    for (int i = 0; i < 7; i++) {
        printf(" %d. %s\n", i + 1, available_dates[i]);
    }

    int date_choice;
    while (1) {
        printf("请选择日期序号 (1-7): ");
        if (scanf("%d", &date_choice) == 1 && date_choice >= 1 && date_choice <= 7) break;
        while (getchar() != '\n');
        printf(COLOR_RED "无效选择。\n" COLOR_RESET);
    }
    char* selected_date = available_dates[date_choice - 1];

    printf("\n可选时段：\n 1. 上午 (08:00-12:00)\n 2. 下午 (13:00-17:00)\n");
    int period_choice;
    while (1) {
        printf("请选择时段 (1/2): ");
        if (scanf("%d", &period_choice) == 1 && (period_choice == 1 || period_choice == 2)) break;
        while (getchar() != '\n');
        printf(COLOR_RED "无效选择。\n" COLOR_RESET);
    }
    int period = period_choice - 1;

    int current_appointments = get_appointment_count(doctor_id, selected_date, period);
    if (current_appointments >= 5) {
        printf(COLOR_RED "\n很抱歉，该医生在该时段预约已满（上限5人），请选择其他时段或医生。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    Appointment* new_app = (Appointment*)malloc(sizeof(Appointment));
    if (!new_app) {
        printf(COLOR_RED "内存分配失败。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }
    /* 记录患者预约的科室与医生，方便后续显示和签到处理 */
    p->dept_id = dept_id;
    p->doctor_id = doctor_id;

    new_app->patient_id = p->id;
    new_app->doctor_id = doctor_id;
    strcpy(new_app->date, selected_date);
    new_app->period = period;
    new_app->next = g_appointment_head;
    g_appointment_head = new_app;

    MedicalRecord* mr = (MedicalRecord*)malloc(sizeof(MedicalRecord));
    if (mr) {
        strcpy(mr->type, "预约挂号");
        strcpy(mr->doctor_name, "系统中心");
        sprintf(mr->content, "预约科室:%s 医生:%d 日期:%s %s",
            get_dept_name(dept_id), doctor_id, selected_date, period == 0 ? "上午" : "下午");
        get_current_datetime_str(mr->date);
        mr->next = p->record_head;
        p->record_head = mr;
    }

    printf(COLOR_GREEN "\n预约成功！请于 %s %s 前来就诊，并于当天使用“预约报到”功能激活。\n" COLOR_RESET,
        selected_date, period == 0 ? "上午" : "下午");
    press_enter_to_continue();
}

// 新增：预约报到功能
static void appointment_checkin(void) {
    clear_screen();
    printf(COLOR_CYAN "--- [预约报到] ---\n" COLOR_RESET);

    Patient* p = find_patient_by_id_or_phone("预约报到");
    if (p == NULL) {
        press_enter_to_continue();
        return;
    }

    // 检查患者当前状态，必须为4（就诊结束）或无就诊记录（新患者状态也可能是4或从未挂号）
    if (p->status != 4 && p->status != 0) {
        // 如果状态是0，说明已经挂过号但未就诊，不能重复报到
        if (p->status == 0) {
            printf(COLOR_RED "\n[错误] 您已经挂过号了，当前状态为待接诊，无需重复报到。\n" COLOR_RESET);
        }
        else {
            printf(COLOR_RED "\n[错误] 您当前就诊流程尚未结束（状态：%s），请先完成当前就诊。\n" COLOR_RESET, get_status_desc(p->status));
        }
        press_enter_to_continue();
        return;
    }

    char today[12];
    get_current_date_str(today);

    // 查找患者当天的预约记录
    Appointment* target_app = NULL;
    Appointment* app = g_appointment_head;
    while (app) {
        if (app->patient_id == p->id && strcmp(app->date, today) == 0) {
            target_app = app;
            break;
        }
        app = app->next;
    }

    if (target_app == NULL) {
        printf(COLOR_RED "\n[错误] 您今天（%s）没有预约记录，请先预约或确认预约日期。\n" COLOR_RESET, today);
        press_enter_to_continue();
        return;
    }

    // 验证医生是否存在且在职
    Doctor* doc = get_doctor_by_id(target_app->doctor_id);
    if (doc == NULL || doc->is_active == 0) {
        printf(COLOR_RED "\n[错误] 您预约的医生当前不可用，请联系管理员。\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    // 激活就诊：设置状态为0，指定医生和科室（沿用预约信息）
    p->dept_id = doc->dept_id;
    p->doctor_id = doc->id;
    p->status = 0;
    p->fee_treat = doc->consultation_fee;  // 产生挂号费
    p->fee_check = 0.0f;
    p->fee_medicine = 0.0f;
    p->fee_ward = 0.0f;
    p->is_paid = 0;
    strcpy(p->prescription, "尚未开具");
    strcpy(p->instructions, "预约报到，请在候诊区等候");

    // 生成挂号记录，时间固定为当天08:00
    MedicalRecord* r = (MedicalRecord*)malloc(sizeof(MedicalRecord));
    if (r) {
        strcpy(r->type, "挂号");
        strcpy(r->doctor_name, "系统中心");
        sprintf(r->content, "科室:%s, 医生:%s, 挂号费:%.2f元 (预约报到)",
            get_dept_name(p->dept_id), doc->name, doc->consultation_fee);
        sprintf(r->date, "%s 08:00", today);
        r->next = p->record_head;
        p->record_head = r;
    }

    printf(COLOR_GREEN "\n报到成功！您的挂号信息如下：\n" COLOR_RESET);
    printf("----------------------------------------\n");
    printf("        医院门诊挂号单 (预约优先)\n");
    printf("----------------------------------------\n");
    printf(" 序号: %-10d\n", p->id);
    printf(" 姓名: %-22s  性别: %-4s\n", p->name, p->gender);
    printf(" 科室: %-10s  医生: %-10s (%s)\n", get_dept_name(p->dept_id), doc->name, get_title_name(doc->title_level));
    printf(" 挂号费: %.2f 元\n", doc->consultation_fee);
    printf(" 状态: %s\n", get_status_desc(p->status));
    printf(" [提示] 您为预约患者，就诊顺序将优先于现场挂号患者。\n");
    printf("----------------------------------------\n");
    press_enter_to_continue();
}

// 其余函数保持不变（view_medical_advice, view_invoice, view_my_registration_slip, draw_patient_menu, patient_function）
// 因篇幅所限，此处省略，但它们在完整文件中是存在的，且与之前版本一致。

static void view_medical_advice(void) {
    clear_screen();
    Patient* p = find_patient_by_id_or_phone("查看医嘱");

    if (p == NULL) {}
    else if (p->status == 0) {
        printf(COLOR_YELLOW "您的当前流程为 [%s]，请耐心等待医生叫号。\n" COLOR_RESET, get_status_desc(p->status));
    }
    else {
        printf(COLOR_GREEN "\n你正在就诊，医生认真检查中……检查完毕……\n正在开具医嘱…医嘱已开具！\n" COLOR_RESET);
        printf("==================== 医 嘱 单 ====================\n");
        printf(" 患者ID: %-10d   姓名: %-22s\n", p->id, p->name);
        printf(" ------------------------------------------------\n");
        printf(" 处方内容: %s\n", p->prescription);
        printf(" 住院要求: %s\n", p->need_inpatient ? "【是】" : "【否】");
        printf(" ------------------------------------------------\n");

        printf(" 【当前待缴费情况】:\n");
        printf(" 药  费: %-8.2f  检查费: %-8.2f\n", p->fee_medicine, p->fee_check);
        printf(" 诊疗费: %-8.2f  住院费: %-8.2f\n", p->fee_treat, p->fee_ward);
        printf(" 总计待缴: %-8.2f\n", p->fee_medicine + p->fee_check + p->fee_treat + p->fee_ward);

        printf(" 缴费状态: %s\n", p->is_paid ? COLOR_GREEN "已全部缴清" COLOR_RESET : COLOR_RED "包含未缴费项 (请前往收费处结账)" COLOR_RESET);
        printf(" ------------------------------------------------\n");
        printf(" 医嘱说明: %s\n", p->instructions);
        printf(" 当前流程提示: %s\n", get_status_desc(p->status));
        printf("==================================================\n");
    }
    press_enter_to_continue();
}

static void view_invoice(void) {
    clear_screen();
    Patient* p = find_patient_by_id_or_phone("打印电子发票");

    if (p == NULL) {}
    else {
        const char* pay_status = p->is_paid ? (COLOR_GREEN "[已结清]" COLOR_RESET) : (COLOR_RED "[欠费中/未结清]" COLOR_RESET);

        printf("\n================ 医院专用电子发票 ================\n");
        printf(" 业务单号: HIS%08d      财务状态: %s\n", p->id, pay_status);
        printf(" 患者姓名: %-22s     科室: %-10s\n", p->name, get_dept_name(p->dept_id));
        printf(" ------------------------------------------------\n");

        MedicalRecord* mr = p->record_head;
        float grand_total = 0.0f;
        int invoice_count = 0;

        while (mr != NULL) {
            if (strcmp(mr->type, "缴费发票") == 0) {
                printf(" [%s]\n   > (收) %s\n", mr->date, mr->content);
                float current_total = 0.0f;
                const char* ptr = strstr(mr->content, "本次合计:");
                if (ptr) {
                    sscanf(ptr, "本次合计:%f", &current_total);
                    grand_total += current_total;
                }
                invoice_count++;
            }
            else if (strcmp(mr->type, "退费凭证") == 0) {
                printf(COLOR_YELLOW " [%s]\n   > (退) %s\n" COLOR_RESET, mr->date, mr->content);
                float current_refund = 0.0f;
                const char* ptr = strstr(mr->content, "退款金额:");
                if (ptr) {
                    sscanf(ptr, "退款金额:%f", &current_refund);
                    grand_total -= current_refund;
                }
                invoice_count++;
            }
            mr = mr->next;
        }

        if (invoice_count == 0) {
            printf(" (暂无已缴费或退款的发票记录)\n");
        }
        else {
            printf(" ------------------------------------------------\n");
            printf(COLOR_GREEN " 历史总计已缴(扣除退款): ￥ %.2f\n" COLOR_RESET, grand_total);
        }
        printf("==================================================\n");

        if (p->is_paid == 0) {
            printf(COLOR_RED " [温馨提示] 您当前仍有未结清的新账单，请前往收费处办理。\n" COLOR_RESET);
        }
    }
    press_enter_to_continue();
}

static void view_my_registration_slip(void) {
    clear_screen();
    Patient* p = find_patient_by_id_or_phone("打印全景挂号单");

    if (p != NULL) {
        Doctor* d = get_doctor_by_id(p->doctor_id);

        printf("\n==================== 患 者 综 合 信 息 单 ====================\n");
        printf(" 患者ID: %-10d | 姓名: %-22s | 电话: %s\n", p->id, p->name, p->phone);
        printf(" 出生日期: %-12s | 挂号科室: %s\n", p->birth_date, get_dept_name(p->dept_id));
        printf(" 主治医生: %-10s | 当前状态: %s\n", d ? d->name : "未分配", get_status_desc(p->status));

        if (p->room_no > 0) {
            printf(" 住院病房号: %-8d | 入院日期: %s\n", p->room_no, p->admit_date);
        }
        else {
            printf(" 住院病房号: 无\n");
        }

        printf(" ----------------- [ 历 史 诊 疗 记 录 ] -----------------\n");
        MedicalRecord* mr = p->record_head;
        if (!mr) printf("  (暂无任何过往就诊记录)\n");
        while (mr) {
            printf("  [%s] %s | %s\n", mr->date, mr->type, mr->content);
            mr = mr->next;
        }
        printf("==============================================================\n");
    }
    press_enter_to_continue();
}

static void draw_patient_menu(int current_select) {
    clear_screen();
    printf(COLOR_GREEN "========================================\n");
    printf("           欢迎使用自助患者终端\n");
    printf("========================================\n" COLOR_RESET);

    const char* options[] = {
        " 0. 返回上级菜单",
        " 1. 门诊挂号 (现场办理)",
        " 2. 预约挂号 (提前预约)",
        " 3. 预约报到 (预约患者专用)",
        " 4. 查看医嘱 (看诊后操作)",
        " 5. 打印电子发票 (包含历史明细)",
        " 6. 自助办理住院 (按医嘱操作)",
        " 7. 自助办理出院 (结清费用后操作)",
        " 8. 打印挂号单 (凭ID/手机号操作)"
    };
    int option_codes[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };

    for (int i = 0; i < 9; i++) {
        if (i == current_select) {
            printf("\033[42;37m%s\033[0m\n", options[i]);
        }
        else {
            printf("%s\n", options[i]);
        }
    }
    printf("----------------------------------------\n");
    printf("当前系统日期: %s\n", g_current_date);
    printf("请使用键盘【↑】【↓】选择，按【Enter】确认，或直接输入数字键。\n");
}

static void patient_sign_in() {
    clear_screen();
    printf("=====================================================\n");
    printf("                 患者签到系统\n");
    printf("=====================================================\n");

    int search_id;
    printf("请输入您的患者ID进行签到 (输入0返回): ");
    if (scanf("%d", &search_id) != 1) {
        while (getchar() != '\n');
        press_enter_to_continue();
        return;
    }

    if (search_id == 0) return;  

    Patient* p = g_patient_head;
    while (p != NULL && p->id != search_id) {
        p = p->next;
    }

    if (p == NULL) {
        printf("\n[错误] 未找到该患者记录，请先完成挂号！\n");
        press_enter_to_continue();
        return;
    }

    /* 检查是否存在预约记录 */
    Appointment* ap = g_appointment_head;
    int has_any_app = 0;
    Appointment* app_today = NULL;
    Appointment* first_app = NULL;
    while (ap) {
        if (ap->patient_id == p->id) {
            has_any_app = 1;
            if (first_app == NULL) first_app = ap;
            if (strcmp(ap->date, g_current_date) == 0) {
                app_today = ap;
                break;
            }
        }
        ap = ap->next;
    }

    if (has_any_app) {
        if (!app_today) {
            printf(COLOR_YELLOW "\n检测到您已有预约（%s），仅允许预约当天签到，请在预约日使用预约报到或取消预约后再现场挂号。\n" COLOR_RESET,
                first_app ? first_app->date : "未知日期");
            press_enter_to_continue();
            return;
        }

        /* 存在当天预约，检查当前时段是否与预约时段匹配（上午/下午，以小时判断，12点和12点多视为非时段） */
        time_t now = time(NULL);
        struct tm nowt;
#ifdef _WIN32
        localtime_s(&nowt, &now);
#else
        localtime_r(&now, &nowt);
#endif
        int hour = nowt.tm_hour;
        int current_period = -1; // 0=上午,1=下午
        if (hour < 12) current_period = 0;
        else if (hour >= 13 && hour < 24) current_period = 1;
        else current_period = -1;

        if (current_period == -1) {
            printf(COLOR_YELLOW "\n当前不在可签到时段（08:00-12:00 或 13:00-17:00），预约患者仅能在预约时段签到。\n" COLOR_RESET);
            press_enter_to_continue();
            return;
        }
        if (current_period != app_today->period) {
            printf(COLOR_YELLOW "\n您的预约时段为 %s，当前时段不匹配，无法签到。\n" COLOR_RESET,
                app_today->period == 0 ? "上午" : "下午");
            press_enter_to_continue();
            return;
        }

        /* 若通过时段校验，则将患者状态视为待诊，允许签到流程继续 */
        p->status = 0;
        /* 确保患者信息与预约一致 */
        /* p->dept_id and p->doctor_id 在预约时已被设置 */
    }
    else {
        /* 非预约患者，按原逻辑仅允许 status==0 签到 */
        if (p->status != 0) {
            printf("\n[提示] 当前患者状态为 [%s]，无需签到。\n", get_status_desc(p->status));
            press_enter_to_continue();
            return;
        }
    }

    // 急诊科患者无需签到
    if (p->dept_id == 1) {
        printf("\n[提示] 急诊科患者无需签到，请直接前往急诊室候诊。\n");
        press_enter_to_continue();
        return;
    }

    // 检查是否已经签到
    SignInQueue* check = g_sign_in_head;
    while (check != NULL) {
        if (check->patient_id == p->id && check->is_valid == 1) {
            printf("\n[提示] 您已完成签到，无需重复签到！\n");
            press_enter_to_continue();
            return;
        }
        check = check->next;
    }

    printf("\n=====================================================\n");
    printf("                 签到信息确认\n");
    printf("=====================================================\n");
    printf(" 患者姓名：%s\n", p->name);
    printf(" 性    别：%s\n", p->gender);
    printf(" 出生日期：%s\n", p->birth_date);
    printf(" 联系电话：%s\n", p->phone);
    printf(" 挂号科室：%s\n", get_dept_name(p->dept_id));
    printf(" 分配医生：");
    Doctor* d = g_doctor_head;
    const char* doc_name_display = "未分配";
    int doc_title_level = 0;
    while (d != NULL) {
        if (d->id == p->doctor_id) {
            doc_name_display = d->name;
            doc_title_level = d->title_level;
            break;
        }
        d = d->next;
    }
    printf("%s (", doc_name_display);
    switch (doc_title_level) {
    case 3: printf("主任医师"); break;
    case 2: printf("副主任医师"); break;
    default: printf("主治医师"); break;
    }
    printf(")\n");
    printf("=====================================================\n");

    // 二次确认
    char confirm[10];
    printf("\n是否确认签到？(Y/N): ");
    while (getchar() != '\n');
    scanf("%c", &confirm[0]);

    if (confirm[0] == 'Y' || confirm[0] == 'y') {
        // 计算前面还有几人
        int ahead_count = 0;
        SignInQueue* sq = g_sign_in_head;
        while (sq != NULL) {
            if (sq->dept_id == p->dept_id && sq->is_valid == 1) {
                ahead_count++;
            }
            sq = sq->next;
        }

        // 创建签到记录
        SignInQueue* new_sign = (SignInQueue*)malloc(sizeof(SignInQueue));
        if (new_sign != NULL) {
            new_sign->patient_id = p->id;
            strcpy(new_sign->patient_name, p->name);
            new_sign->dept_id = p->dept_id;
            new_sign->doctor_id = p->doctor_id;
            time(&new_sign->sign_in_time);
            new_sign->is_valid = 1;
            new_sign->next = g_sign_in_head;
            g_sign_in_head = new_sign;
            p->checked_in = 1;  // 标记患者已签到

            clear_screen();
            printf("\n=====================================================\n");
            printf("                 签到成功！\n");
            printf("=====================================================\n");
            printf(" 患者姓名：%s\n", p->name);
            printf(" 挂号科室：%s\n", get_dept_name(p->dept_id));
            printf(" 分配医生：%s\n", doc_name_display);
            printf(" 前面等候人数：%d 人\n", ahead_count);
            printf("=====================================================\n");
            printf("\n[提示] 请耐心等待叫号，过号需重新签到！\n");
            // 如果患者当天有预约，清除当天的预约记录（避免重复保留）
            Appointment* cur = g_appointment_head;
            Appointment* prev = NULL;
            while (cur) {
                if (cur->patient_id == p->id && strcmp(cur->date, g_current_date) == 0) {
                    Appointment* to_del = cur;
                    if (prev) prev->next = cur->next;
                    else g_appointment_head = cur->next;
                    cur = cur->next;
                    free(to_del);
                }
                else {
                    prev = cur;
                    cur = cur->next;
                }
            }
        }
    }
    else {
        printf("\n[提示] 签到已取消，请尽快完成签到以免延误就诊。\n");
    }

    press_enter_to_continue();
}

void patient_function()
{
    int current_select = 1;
    const char* options[] = {
        " 0. 返回上级菜单",
        " 1. 门诊挂号 (自助办理)",
        " 2. 预约挂号 (提前预约)",
        " 3. 预约管理 (查看/取消预约)",
        " 4. 患者签到 (挂号后请先签到)",
        " 5. 查看医嘱 (看诊后操作)",
        " 6. 打印电子发票 (包含历史明细)",
        " 7. 自助办理住院 (按医嘱操作)",
        " 8. 自助办理出院 (结清费用后操作)",
        " 9. 打印挂号单 (凭ID操作)",
        "10. 自助缴费 (按ID或手机号)"
    };
    int option_codes[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    int max_options = 11;
    int run = 1;

    while (run) {
        clear_screen();
        printf(COLOR_GREEN "========================================\n");
        printf("           欢迎使用自助患者终端\n");
        printf("========================================\n" COLOR_RESET);

        for (int i = 0; i < max_options; i++) {
            if (i == current_select) {
                printf("\033[42;37m%s\033[0m\n", options[i]);
            }
            else {
                printf("%s\n", options[i]);
            }
        }
        printf("----------------------------------------\n");
        printf("当前系统日期: %s\n", g_current_date);
        printf("请使用键盘【↑】【↓】选择，按【Enter】确认，或直接输入数字键。\n");

        int key = get_single_key();
        int choice = -1;

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
            choice = option_codes[current_select];
            switch (choice) {
            case 1: register_patient_flow(); break;
            case 2: appointment_flow(); break;
            case 3: appointment_manage(); break;
            case 4: patient_sign_in(); break;
            case 5: view_medical_advice(); break;
            case 6: view_invoice(); break;
            case 10: patient_self_payment(); break;
            case 7: {
                printf("请输入您的患者ID办理住院: ");
                int pid;
                if (scanf("%d", &pid) == 1) {
                    Patient* p = g_patient_head;
                    while (p && p->id != pid) p = p->next;
                    if (p) patient_admission_flow(p);
                    else { printf("未查找到ID。\n"); press_enter_to_continue(); }
                }
                break;
            }
            case 8: {
                printf("请输入您的患者ID办理出院: ");
                int pid;
                if (scanf("%d", &pid) == 1) {
                    Patient* p = g_patient_head;
                    while (p && p->id != pid) p = p->next;
                    if (p) patient_discharge_flow(p);
                    else { printf("未查找到ID。\n"); press_enter_to_continue(); }
                }
                break;
            }
            case 9: view_my_registration_slip(); break;
            case 0: run = 0; break;
            }
        }
        else if (key >= '0' && key <= '9') {
            choice = key - '0';
            for (int i = 0; i < max_options; i++) {
                if (option_codes[i] == choice) { current_select = i; break; }
            }
            switch (choice) {
            case 1: register_patient_flow(); break;
            case 2: appointment_flow(); break;
            case 3: appointment_manage(); break;
            case 4: patient_sign_in(); break;
            case 5: view_medical_advice(); break;
            case 6: view_invoice(); break;
            case 7: {
                printf("请输入您的患者ID办理住院: ");
                int pid;
                if (scanf("%d", &pid) == 1) {
                    Patient* p = g_patient_head;
                    while (p && p->id != pid) p = p->next;
                    if (p) patient_admission_flow(p);
                    else { printf("未查找到ID。\n"); press_enter_to_continue(); }
                }
                break;
            }
            case 8: {
                printf("请输入您的患者ID办理出院: ");
                int pid;
                if (scanf("%d", &pid) == 1) {
                    Patient* p = g_patient_head;
                    while (p && p->id != pid) p = p->next;
                    if (p) patient_discharge_flow(p);
                    else { printf("未查找到ID。\n"); press_enter_to_continue(); }
                }
                break;
            }
            case 9: view_my_registration_slip(); break;
            case 0: run = 0; break;
            }
        }
    }
}

//void patient_function() {
//    int current_select = 1;
//    int option_codes[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
//    int max_options = 9;
//    int run = 1;
//
//    while (run) {
//        draw_patient_menu(current_select);
//        int key = get_single_key();
//
//        if (key == 224) {
//            key = get_single_key();
//            if (key == 72) {
//                current_select--;
//                if (current_select < 0) current_select = max_options - 1;
//            }
//            else if (key == 80) {
//                current_select++;
//                if (current_select >= max_options) current_select = 0;
//            }
//        }
//        else if (key == 13) {
//            int choice = option_codes[current_select];
//            Patient* p = NULL;
//            switch (choice) {
//            case 1: register_patient_flow(); break;
//            case 2: appointment_flow(); break;
//            case 3: appointment_checkin(); break;
//            case 4: view_medical_advice(); break;
//            case 5: view_invoice(); break;
//            case 6:
//                p = find_patient_by_id_or_phone("办理住院");
//                if (p) patient_admission_flow(p);
//                break;
//            case 7:
//                p = find_patient_by_id_or_phone("办理出院");
//                if (p) patient_discharge_flow(p);
//                break;
//            case 8: view_my_registration_slip(); break;
//            case 0: run = 0; break;
//            }
//        }
//        else if (key >= '0' && key <= '8') {
//            int direct_choice = key - '0';
//            for (int i = 0; i < max_options; i++) {
//                if (option_codes[i] == direct_choice) {
//                    current_select = i;
//                    Patient* p = NULL;
//                    switch (direct_choice) {
//                    case 1: register_patient_flow(); break;
//                    case 2: appointment_flow(); break;
//                    case 3: appointment_checkin(); break;
//                    case 4: view_medical_advice(); break;
//                    case 5: view_invoice(); break;
//                    case 6:
//                        p = find_patient_by_id_or_phone("办理住院");
//                        if (p) patient_admission_flow(p);
//                        break;
//                    case 7:
//                        p = find_patient_by_id_or_phone("办理出院");
//                        if (p) patient_discharge_flow(p);
//                        break;
//                    case 8: view_my_registration_slip(); break;
//                    case 0: run = 0; break;
//                    }
//                    break;
//                }
//            }
//        }
//    }
//}
