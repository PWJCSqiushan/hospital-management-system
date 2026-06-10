/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：doctor.cpp
 * 修改摘要：【升级6.0 最终修正】
 * 1. 修复密码输入回车问题。
 * 2. 医生只接诊挂自己号的患者，按挂号时间顺序自动分配。
 * 3. 急诊科医生可额外手动查找其他患者（紧急情况）。
 * 4. 移除看诊流程中多余的 press_enter_to_continue。
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include "doctor.h"
#include "inpatient.h"
#include "utils.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RED     "\033[31m"

static void process_patient_consultation(Doctor* curr_doc, Patient* target);

static void get_hidden_input(char* pwd, int max_len) {
    int i = 0;
    int ch;
    while (1) {
        ch = _getch();
        if (ch == '\r' || ch == '\n') {
            pwd[i] = '\0';
            break;
        }
        else if (ch == '\b') {
            if (i > 0) {
                i--;
                printf("\b \b");
            }
        }
        else if (i < max_len - 1) {
            pwd[i++] = (char)ch;
            printf("*");
        }
    }
    while (_kbhit()) _getch();
    printf("\n");
}

const char* get_dept_name(int dept_id) {
    switch (dept_id) {
    case 1: return "急诊科";
    case 2: return "内科";
    case 3: return "外科";
    case 4: return "儿科";
    case 5: return "妇科";
    default: return "未知科室";
    }
}

const char* get_title_name(int title_level) {
    switch (title_level) {
    case 1: return "主任医师";
    case 2: return "副主任医师";
    case 3: return "普通医师";
    default: return "未知职级";
    }
}

Doctor* get_doctor_by_id(int doc_id) {
    Doctor* d = g_doctor_head;
    while (d) {
        if (d->id == doc_id) return d;
        d = d->next;
    }
    return NULL;
}

void ShowCurrentDate() {
    char date_str[12];
    get_current_date_str(date_str);
    printf(" [当前系统日期: %s] ", date_str);
}

int CalculateAge(const char* birthDate) {
    int bY, bM, bD;
    if (sscanf(birthDate, "%d-%d-%d", &bY, &bM, &bD) != 3) return 0;
    int year, month, day;
    sscanf(g_current_date, "%d-%d-%d", &year, &month, &day);
    int age = year - bY;
    if (month < bM || (month == bM && day < bD)) age--;
    return age;
}

int SecondaryConfirm(const char* actionDesc) {
    char choice = 0;
    while (1) {
        printf("\n[确认] 是否确定执行 <%s> 吗? (Y/N): ", actionDesc);
        (void)scanf(" %c", &choice);
        while (getchar() != '\n');
        if (choice == 'Y' || choice == 'y') return 1;
        if (choice == 'N' || choice == 'n') {
            printf(">> 提示：已放弃 [%s] 操作。\n", actionDesc);
            return 0;
        }
        printf(COLOR_RED "[错误] 无效输入！请输入 Y (确定) 或 N (放弃)。\n" COLOR_RESET);
    }
}

int ChangeDoctorPassword(Doctor* currDoc) {
    char newPwd[20] = { 0 };
    char confirmPwd[20] = { 0 };
    printf("请输入新密码: ");
    get_hidden_input(newPwd, 20);
    printf("请再次确认密码: ");
    get_hidden_input(confirmPwd, 20);
    if (strcmp(newPwd, confirmPwd) != 0) {
        printf(COLOR_RED "\n>> [错误] 两次输入的密码不一致，修改失败！\n" COLOR_RESET);
        return 0;
    }
    if (SecondaryConfirm("修改登录密码")) {
        (void)strcpy(currDoc->password, newPwd);
        printf(COLOR_GREEN "\n>> [成功] 密码更新完成。系统要求重新登录。\n" COLOR_RESET);
        return 1;
    }
    return 0;
}

void ViewAllPatientsReadonly() {
    clear_screen();
    printf("--- 全院患者名单查询 (只读) ---\n");
    Patient* ap = g_patient_head;
    printf("%-8s | %-22s | %-10s | %-12s\n", "ID", "姓名", "科室", "状态");
    while (ap != NULL) {
        const char* color = COLOR_RESET;
        if (ap->status == 0) color = COLOR_CYAN;
        else if (ap->status == 1 || ap->status == 2) color = COLOR_YELLOW;
        else if (ap->status == 3) color = COLOR_RED;
        else color = COLOR_GREEN;
        printf("%-8d | %-22s | %-10s | %s%d%s\n",
            ap->id, ap->name, get_dept_name(ap->dept_id), color, ap->status, COLOR_RESET);
        ap = ap->next;
    }
    press_enter_to_continue();
}

/* ==================== 医生主入口 ==================== */

// 提取公共的接诊处理逻辑
static void process_patient_consultation(Doctor* curr_doc, Patient* target) {
    if (target->status != 0) {
        printf("\n [提示] 该患者当前状态非\"待接诊\"，无需重新看诊。\n");
        return;
    }

    printf("\n--- 该患者过往历史医疗记录 ---\n");
    MedicalRecord* mr = target->record_head;
    if (!mr) printf(" (暂无任何过往就诊记录)\n");
    while (mr) {
        printf("[%s] 类型:%s | 责任人:%-8s | 内容:%s\n",
            mr->date, mr->type, mr->doctor_name, mr->content);
        mr = mr->next;
    }

    printf("\n [提示] 正在诊治患者: %s (医生认真检查中...)\n", target->name);

    char all_prescriptions[300] = { 0 };
    char input_buf[100] = { 0 };
    target->fee_medicine = 0.0f;
    printf("\n [药品开具] 支持多次录入。输入'结束'或'无'停止录入。\n");

    while (1) {
        printf(" > 输入处方药品名 (学名/商品名/别名) 或直接输入 药名*数量: ");
        (void)scanf("%s", input_buf);

        if (strcmp(input_buf, "结束") == 0 || strcmp(input_buf, "无") == 0) {
            break;
        }

        /* --- 核心修复：自动分离药名和数量，防止 strcmp 匹配失败 --- */
        int qty = 1;
        char* star_pos = strchr(input_buf, '*');
        if (star_pos != NULL) {
            *star_pos = '\0'; /* 将星号替换为字符串结束符，提取纯药名 */
            qty = atoi(star_pos + 1); /* 提取数量 */
            if (qty <= 0) qty = 1;
        }

        Medicine* matched_med = NULL;
        Medicine* med = g_medicine_head;
        while (med) {
            if (strcmp(med->common_name, input_buf) == 0 ||
                strcmp(med->trade_name, input_buf) == 0 ||
                strcmp(med->alias, input_buf) == 0) {
                matched_med = med;
                break;
            }
            med = med->next;
        }

        /* 如果没有在输入时带星号，则正常询问数量 */
        if (star_pos == NULL && matched_med != NULL) {
            while (1) {
                printf("   请输入开具数量: ");
                if (scanf("%d", &qty) != 1 || qty <= 0) {
                    while (getchar() != '\n');
                    printf("   [错误] 数量无效。\n");
                    printf("   1. 重新输入数量\n");
                    printf("   2. 取消开具此药\n");
                    printf("   请选择: ");
                    int fix;
                    if (scanf("%d", &fix) == 1 && fix == 2) {
                        while (getchar() != '\n');
                        qty = 0;  // 标记取消
                        break;
                    }
                    while (getchar() != '\n');
                    continue;
                }
                break;
            }
            if (qty == 0) continue;  // 跳过这个药
        }

        char temp_format[100] = { 0 };
        if (matched_med) {
            float cost = matched_med->price * qty;
            /* 增加明确提示：说明别名已被成功转换为学名 */
            printf("   [系统匹配] 成功锁定库房药品: [%s] (单价:%.2f元) x%d, 小计: %.2f元\n",
                matched_med->common_name, matched_med->price, qty, cost);

            /* 判断该药是否允许由当前科室开具 */
            int allowed = 0;
            if (matched_med->allowed_depts[0] == 1) allowed = 1;
            else if (curr_doc->dept_id >= 1 && curr_doc->dept_id <= 5 && matched_med->allowed_depts[curr_doc->dept_id] == 1) allowed = 1;

            if (!allowed) {
                char over = 0;
                while (1) {
                    printf("   " COLOR_RED "警告: [%s] 非本科室常用药品，是否确认越权开具此药？(Y/N): " COLOR_RESET, matched_med->common_name);
                    /* 读取单个字符（跳过空白）并验证 */
                    if (scanf(" %c", &over) != 1) {
                        while (getchar() != '\n');
                        printf(COLOR_RED "   [错误] 输入无效，请输入 Y 或 N。\n" COLOR_RESET);
                        continue;
                    }
                    /* 清理行尾多余输入 */
                    while (getchar() != '\n');

                    if (over == 'Y' || over == 'y') {
                        target->fee_medicine += cost;
                        sprintf(temp_format, "%s*%d", matched_med->common_name, qty);
                        break;
                    }
                    else if (over == 'N' || over == 'n') {
                        printf("   [提示] 已取消开具 %s。\n", matched_med->common_name);
                        break;
                    }
                    else {
                        printf(COLOR_RED "   [错误] 请输入 Y (是) 或 N (否)。\n" COLOR_RESET);
                    }
                }
            }
            else {
                target->fee_medicine += cost;
                sprintf(temp_format, "%s*%d", matched_med->common_name, qty);
            }
        }
        else {
            if (star_pos == NULL) {
                while (1) {
                    printf("   请输入开具数量: ");
                    if (scanf("%d", &qty) != 1 || qty <= 0) {
                        while (getchar() != '\n');
                        printf("   [错误] 数量无效。\n");
                        printf("   1. 重新输入\n");
                        printf("   2. 取消开具\n");
                        printf("   请选择: ");
                        int fix;
                        if (scanf("%d", &fix) == 1 && fix == 2) {
                            while (getchar() != '\n');
                            qty = 0;
                            break;
                        }
                        while (getchar() != '\n');
                        continue;
                    }
                    break;
                }
                if (qty == 0) continue;
            }
            printf("   [系统警告] 未匹配到字典药品，按外购药(0元)记录，请确认名称。\n");
            sprintf(temp_format, "%s*%d", input_buf, qty);
        }

        if (strlen(all_prescriptions) > 0) strcat(all_prescriptions, ",");
        strcat(all_prescriptions, temp_format);
    }

    if (strlen(all_prescriptions) == 0) {
        strcpy(all_prescriptions, "无药处方");
    }

    if (SecondaryConfirm("下达本次医嘱并结束诊疗")) {
        (void)strcpy(target->prescription, all_prescriptions);

        float add_check = 0.0f, add_treat = 0.0f;
        printf(" > 附加检查费 附加诊疗费(空格分隔，无则填0 0): ");
        (void)scanf("%f %f", &add_check, &add_treat);
        target->fee_check += add_check;
        target->fee_treat += add_treat;

        if (curr_doc->dept_id == 1) {
            printf(" > 是否需要住院(0:否, 1:常规病房, 2:紧急转入ICU): ");
            (void)scanf("%d", &target->need_inpatient);
            if (target->need_inpatient == 2) {
                if (doctor_admit_to_icu(target)) {
                    printf("\n [紧急提示] 已成功为该患者锁定急诊ICU床位 (房号:%d)！\n", target->room_no);
                }
                else {
                    printf("\n [紧急警告] ICU已满！患者仍需住院，将转为普通流程！\n");
                    target->need_inpatient = 1;
                }
            }
        }
        else {
            printf(" > 是否需要住院(1:是, 0:否): ");
            (void)scanf("%d", &target->need_inpatient);
        }

        printf(" > 医嘱说明: ");
        (void)scanf("%s", target->instructions);

        MedicalRecord* nr = (MedicalRecord*)malloc(sizeof(MedicalRecord));
        if (nr) {
            (void)strcpy(nr->type, "看诊");
            (void)strcpy(nr->doctor_name, curr_doc->name);
            (void)sprintf(nr->content, "处方:[%s]; 说明:%s", target->prescription, target->instructions);
            time_t t_now;
            time(&t_now);
            struct tm* tm_info = localtime(&t_now);
            strftime(nr->date, 20, "%Y-%m-%d %H:%M", tm_info);
            nr->next = target->record_head;
            target->record_head = nr;
        }

        if (target->status != 3) {
            target->status = 1;
        }
        printf("\n [成功] 医嘱已下达，请提示患者前往收费处缴费！\n");
    }
}

void doctor_function(void) {
    clear_screen();
    if (g_doctor_head == NULL) {
        printf(COLOR_RED "\n************************************************************\n");
        printf("[系统错误] 医生数据未加载！\n");
        printf("************************************************************\n" COLOR_RESET);
        press_enter_to_continue(); return;
    }

    printf(COLOR_CYAN "========================================\n");
    printf("         HIS 医生终端安全登录\n");
    printf("========================================\n" COLOR_RESET);
    int login_id = 0;
    char login_pwd[20] = { 0 };
    printf("工号: ");
    if (scanf("%d", &login_id) != 1) { while (getchar() != '\n'); return; }
    while (getchar() != '\n');
    printf("密码: ");
    get_hidden_input(login_pwd, 20);

    Doctor* curr_doc = g_doctor_head;
    while (curr_doc) {
        if (curr_doc->id == login_id && strcmp(curr_doc->password, login_pwd) == 0) {
            if (curr_doc->is_active == 0) {
                printf(COLOR_RED "\n[错误] 该医生账号已离职，无法登录！\n" COLOR_RESET);
                press_enter_to_continue();
                return;
            }
            break;
        }
        curr_doc = curr_doc->next;
    }

    if (curr_doc == NULL) {
        printf(COLOR_RED "\n[错误] 工号或密码不正确，登录失败！\n" COLOR_RESET);
        press_enter_to_continue(); return;
    }

    int current_select = 0;
    int option_codes[] = { 1, 2, 3, 0 };
    const char* options[] = {
        " 1. 开始看诊(查找患者)",
        " 2. 查询全院患者(只读)",
        " 3. 修改个人密码",
        " 0. 退出登录"
    };
    int max_options = 4;
    int run = 1;

    while (run) {
        clear_screen();
        ShowCurrentDate();
        printf("\n" COLOR_CYAN "========================================================================\n");
        printf(" 【医生仪表盘】 | 姓名: %-22s | 工号: %-4d | 职级: %-10s \n",
            curr_doc->name, curr_doc->id, get_title_name(curr_doc->title_level));
        printf(" 所属科室: %-10s  | 权限: 诊疗/开药/查询 \n", get_dept_name(curr_doc->dept_id));
        printf("========================================================================\n\n" COLOR_RESET);

        printf(" --- [您的待诊患者列表] ---\n");
        printf(" %-8s | %-22s | %-6s | %-4s | %-14s | %-12s \n", "患者ID", "姓名", "性别", "年龄", "挂号类型", "流程状态");
        printf(" ----------------------------------------------------------------------\n");

        Patient* p = g_patient_head;
        int p_count = 0;
        while (p != NULL) {
            if (p->doctor_id == curr_doc->id && p->status == 0) {
                Doctor* pat_doc = get_doctor_by_id(p->doctor_id);
                const char* pat_type = pat_doc ? get_title_name(pat_doc->title_level) : "未知";
                const char* status_text;
                const char* status_color;
                if (p->checked_in == 1) {
                    status_text = "已签到";
                    status_color = COLOR_GREEN;
                }
                else {
                    status_text = "未签到";
                    status_color = COLOR_RED;
                }
                printf(" %-8d | %-22s | %-6s | %-4d | %-14s | %s%-12s" COLOR_RESET " \n",
                    p->id, p->name, p->gender, CalculateAge(p->birth_date), pat_type, status_color, status_text);
                p_count++;
            }
            p = p->next;
        }
        if (p_count == 0) printf(" (您当前没有待诊患者)\n");
        printf(" ----------------------------------------------------------------------\n");

        printf("\n【医生操作菜单】\n");
        for (int i = 0; i < max_options; i++) {
            if (i == current_select) {
                printf("\033[46;37m%s\033[0m\n", options[i]);
            }
            else {
                printf("%s\n", options[i]);
            }
        }
        printf("----------------------------------------------------------------------\n");
        printf("请使用键盘【↑】【↓】选择，按【Enter】确认，或直接输入数字键。\n");

        int key = get_single_key();
        int choice = -1;

        if (key == 224) {
            key = get_single_key();
            if (key == 72) { current_select--; if (current_select < 0) current_select = max_options - 1; }
            else if (key == 80) { current_select++; if (current_select >= max_options) current_select = 0; }
            continue;
        }
        else if (key == 13) {
            choice = option_codes[current_select];
        }
        else if (key >= '0' && key <= '3') {
            choice = key - '0';
            for (int i = 0; i < max_options; i++) {
                if (option_codes[i] == choice) { current_select = i; break; }
            }
        }
        else { continue; }

        if (choice == 1) {
            // 急诊科和非急诊科有不同的接诊逻辑
            if (curr_doc->dept_id == 1) {
                // 急诊科：按ID或姓名选择患者
                int s_way = 0;
                while (1) {
                    printf("\n查找方式: 1.按ID查找  2.按姓名查找  0.返回: ");
                    if (scanf("%d", &s_way) == 1 && (s_way >= 0 && s_way <= 2)) break;
                    printf("[错误] 无效选择，请输入 0、1 或 2！\n");
                    while (getchar() != '\n');
                }

                if (s_way == 0) {
                    press_enter_to_continue();
                    continue;  // 返回医生主菜单
                }
                Patient* target = NULL;
                if (s_way == 1) {
                    int tid = 0;
                    printf("请输入患者ID: ");
                    (void)scanf("%d", &tid);
                    target = g_patient_head;
                    while (target && target->id != tid) target = target->next;
                }
                else {
                    char sname[30] = { 0 };
                    printf("请输入姓名: ");
                    (void)scanf("%s", sname);
                    Patient* matches[50] = { 0 };
                    int m_count = 0;
                    Patient* tempP = g_patient_head;
                    while (tempP) {
                        if (strcmp(tempP->name, sname) == 0) matches[m_count++] = tempP;
                        tempP = tempP->next;
                    }
                    if (m_count > 1) {
                        printf("\n查找到同名患者，请根据ID和生日选择:\n");
                        for (int i = 0; i < m_count; i++)
                            printf("[%d] ID:%d | 生日:%s | 性别:%s\n", i + 1, matches[i]->id, matches[i]->birth_date, matches[i]->gender);
                        int s_idx = 0;
                        printf("请选择序号: ");
                        (void)scanf("%d", &s_idx);
                        if (s_idx > 0 && s_idx <= m_count) target = matches[s_idx - 1];
                    }
                    else if (m_count == 1) target = matches[0];
                }

                // 急诊科接诊逻辑
                if (target && target->dept_id == curr_doc->dept_id) {
                    process_patient_consultation(curr_doc, target);
                }
                else {
                    printf("\n [错误] 未找到该患者，或该患者不属于急诊科。\n");
                }
            }
            else {
                // 非急诊科：按签到顺序叫号，同时支持按ID/姓名查找
                printf("\n接诊模式选择：\n");
                printf(" 1. 按签到顺序叫号 (推荐)\n");
                printf(" 2. 按患者ID或姓名查找 (辅助)\n");
                printf(" 0. 返回上一级菜单\n");
                printf("请选择: ");

                int call_mode = 0;
                if (scanf("%d", &call_mode) != 1) {
                    while (getchar() != '\n');
                    press_enter_to_continue();
                    continue;
                }

                if (call_mode == 0) continue; 

                if (call_mode == 1) {
                    SignInQueue* current = g_sign_in_head;
                    Patient* target = NULL;
                    time_t earliest_time = 0;
                    SignInQueue* earliest_sign = NULL;

                    // 第一步：找到本科室签到时间最早的有效记录
                    while (current != NULL) {
                        if (current->dept_id == curr_doc->dept_id && current->is_valid == 1) {
                            Patient* check_p = g_patient_head;
                            while (check_p != NULL) {
                                if (check_p->id == current->patient_id && check_p->status == 0) {
                                    if (earliest_sign == NULL || current->sign_in_time < earliest_time) {
                                        earliest_time = current->sign_in_time;
                                        earliest_sign = current;
                                        target = check_p;
                                    }
                                    break;
                                }
                                check_p = check_p->next;
                            }
                        }
                        current = current->next;
                    }

                    // 第二步：如果找到了最早签到的患者，进行接诊
                    if (target != NULL && earliest_sign != NULL) {
                        Doctor* assigned_doc = g_doctor_head;
                        while (assigned_doc != NULL && assigned_doc->id != target->doctor_id) {
                            assigned_doc = assigned_doc->next;
                        }

                        if (assigned_doc != NULL && curr_doc->title_level >= assigned_doc->title_level) {
                            printf("\n=====================================================\n");
                            printf("  叫号：请 %s 患者到诊室就诊\n", target->name);
                            printf("=====================================================\n");
                            printf(" 患者ID：%d  姓名：%s\n", target->id, target->name);
                            printf(" 性别：%s  年龄：%d\n", target->gender, CalculateAge(target->birth_date));
                            printf(" 签到时间：%s", ctime(&earliest_sign->sign_in_time));
                            printf("=====================================================\n");

                            printf("\n请确认患者是否本人到场？(Y/N): ");
                            char confirm_presence[10];
                            while (getchar() != '\n');
                            scanf("%c", &confirm_presence[0]);

                            if (confirm_presence[0] == 'Y' || confirm_presence[0] == 'y') {
                                process_patient_consultation(curr_doc, target);
                                earliest_sign->is_valid = 0;
                                press_enter_to_continue();
                                continue;  // 返回医生主菜单
                            }
                            else {
                                printf("\n[过号] 患者 %s 未到场，签到作废，需重新签到排队。\n", target->name);
                                earliest_sign->is_valid = 0;
                                press_enter_to_continue();
                                continue;  // 返回医生主菜单
                            }
                        }
                    }
                    else {
                        printf("\n[提示] 当前无待接诊的本科室患者。\n");
                    }
                    press_enter_to_continue();
                    continue;  // 返回医生主菜单
                }
                else if (call_mode == 2) {
                    // 按ID或姓名查找（辅助功能）
                    int s_way = 0;
                    while (1) {
                        printf("\n查找方式: 1.按ID查找  2.按姓名查找  0.返回: ");
                        if (scanf("%d", &s_way) == 1 && (s_way >= 0 && s_way <= 2)) break;
                        printf("[错误] 无效选择，请输入 0、1 或 2！\n");
                        while (getchar() != '\n');
                    }

                    if (s_way == 0) {
                        press_enter_to_continue();
                        continue;  // 返回医生主菜单
                    }

                    Patient* target = NULL;
                    if (s_way == 1) {
                        int tid = 0;
                        printf("请输入患者ID: ");
                        (void)scanf("%d", &tid);
                        target = g_patient_head;
                        while (target && target->id != tid) target = target->next;
                    }
                    else {
                        char sname[30] = { 0 };
                        printf("请输入姓名: ");
                        (void)scanf("%s", sname);
                        Patient* matches[50] = { 0 };
                        int m_count = 0;
                        Patient* tempP = g_patient_head;
                        while (tempP) {
                            if (strcmp(tempP->name, sname) == 0) matches[m_count++] = tempP;
                            tempP = tempP->next;
                        }
                        if (m_count > 1) {
                            printf("\n查找到同名患者，请根据ID和生日选择:\n");
                            for (int i = 0; i < m_count; i++)
                                printf("[%d] ID:%d | 生日:%s | 性别:%s\n", i + 1, matches[i]->id, matches[i]->birth_date, matches[i]->gender);
                            int s_idx = 0;
                            printf("请选择序号: ");
                            (void)scanf("%d", &s_idx);
                            if (s_idx > 0 && s_idx <= m_count) target = matches[s_idx - 1];
                        }
                        else if (m_count == 1) target = matches[0];
                    }

                    if (target && target->dept_id == curr_doc->dept_id) {
                        // 职称权限检查
                        Doctor* assigned_doc = g_doctor_head;
                        while (assigned_doc != NULL && assigned_doc->id != target->doctor_id) {
                            assigned_doc = assigned_doc->next;
                        }

                        if (assigned_doc != NULL && curr_doc->title_level >= assigned_doc->title_level) {
                            process_patient_consultation(curr_doc, target);

                            // 删除对应的签到记录
                            SignInQueue* prev = NULL;
                            SignInQueue* curr = g_sign_in_head;
                            while (curr != NULL) {
                                if (curr->patient_id == target->id && curr->is_valid == 1) {
                                    if (prev == NULL) {
                                        g_sign_in_head = curr->next;
                                    }
                                    else {
                                        prev->next = curr->next;
                                    }
                                    free(curr);
                                    break;
                                }
                                prev = curr;
                                curr = curr->next;
                            }
                        }
                        else {
                            printf("\n [权限不足] 您无权接诊该患者\n");
                        }
                    }
                    else {
                        printf("\n [错误] 未找到该患者，或该患者不属于您的接诊范围。\n");
                    }
                }
            }
            press_enter_to_continue();
        }

        //if (choice == 1) {
        //    Patient* target = NULL;
        //    int is_emergency = (curr_doc->dept_id == 1);

        //    // 1. 自动获取挂了自己号的最早患者
        //    Patient* earliest = NULL;
        //    char earliest_time[20] = "";
        //    Patient* p_temp = g_patient_head;
        //    while (p_temp) {
        //        if (p_temp->doctor_id == curr_doc->id && p_temp->status == 0) {
        //            MedicalRecord* mr = p_temp->record_head;
        //            while (mr) {
        //                if (strcmp(mr->type, "挂号") == 0) {
        //                    if (strlen(earliest_time) == 0 || strcmp(mr->date, earliest_time) < 0) {
        //                        strcpy(earliest_time, mr->date);
        //                        earliest = p_temp;
        //                    }
        //                    break;
        //                }
        //                mr = mr->next;
        //            }
        //        }
        //        p_temp = p_temp->next;
        //    }

        //    if (earliest != NULL) {
        //        target = earliest;
        //        printf("\n[系统] 自动分配您的下一位待诊患者：ID:%d 姓名:%s 挂号时间:%s\n",
        //            target->id, target->name, earliest_time);
        //    }
        //    else {
        //        printf(COLOR_YELLOW "\n您当前没有待诊患者。\n" COLOR_RESET);
        //        // 急诊科医生可以手动查找其他患者
        //        if (is_emergency) {
        //            printf("\n急诊科医生可选择手动查找其他患者（紧急情况）。\n");
        //            printf("按 Y 手动查找，按其他键返回: ");
        //            char manual_choice;
        //            (void)scanf(" %c", &manual_choice);
        //            while (getchar() != '\n');
        //            if (manual_choice == 'Y' || manual_choice == 'y') {
        //                int s_way = 0;
        //                while (1) {
        //                    printf("\n查找方式: 1.按ID查找  2.按姓名查找: ");
        //                    if (scanf("%d", &s_way) == 1 && (s_way == 1 || s_way == 2)) break;
        //                    printf(COLOR_RED "[错误] 无效选择，请输入 1 或 2！\n" COLOR_RESET);
        //                    while (getchar() != '\n');
        //                }

        //                target = NULL;
        //                if (s_way == 1) {
        //                    int tid = 0; printf("请输入患者ID: "); (void)scanf("%d", &tid);
        //                    target = g_patient_head;
        //                    while (target && target->id != tid) target = target->next;
        //                }
        //                else {
        //                    char sname[30] = { 0 }; printf("请输入姓名: "); (void)scanf("%s", sname);
        //                    Patient* matches[50] = { 0 }; int m_count = 0;
        //                    Patient* tempP = g_patient_head;
        //                    while (tempP) {
        //                        if (strcmp(tempP->name, sname) == 0) {
        //                            matches[m_count++] = tempP;
        //                        }
        //                        tempP = tempP->next;
        //                    }

        //                    if (m_count > 1) {
        //                        printf("\n查找到同名患者，请选择:\n");
        //                        for (int i = 0; i < m_count; i++) {
        //                            printf("[%d] ID:%d | 科室:%s | 状态:%d\n",
        //                                i + 1, matches[i]->id, get_dept_name(matches[i]->dept_id), matches[i]->status);
        //                        }
        //                        int s_idx = 0; printf("请选择序号: "); (void)scanf("%d", &s_idx);
        //                        if (s_idx > 0 && s_idx <= m_count) target = matches[s_idx - 1];
        //                    }
        //                    else if (m_count == 1) target = matches[0];
        //                }
        //                if (target == NULL) {
        //                    printf(COLOR_RED "未找到该患者。\n" COLOR_RESET);
        //                    press_enter_to_continue();
        //                    continue;
        //                }
        //            }
        //            else {
        //                press_enter_to_continue();
        //                continue;
        //            }
        //        }
        //        else {
        //            press_enter_to_continue();
        //            continue;
        //        }
        //    }

        //    if (target) {
        //        if (target->status != 0) {
        //            printf(COLOR_YELLOW "\n [提示] 该患者当前状态非\"待接诊\"，无需重新看诊。\n" COLOR_RESET);
        //            press_enter_to_continue();
        //            continue;
        //        }

        //        // 急诊科手动查找的患者可能挂的是其他医生的号，需要权限检查
        //        if (target->doctor_id != curr_doc->id) {
        //            Doctor* target_doc = get_doctor_by_id(target->doctor_id);
        //            if (target_doc && curr_doc->title_level > target_doc->title_level) {
        //                printf(COLOR_RED "\n [权限拒绝] 您(职级:%d)的权限不足以接诊挂号给高级别医生(%d)的患者！\n" COLOR_RESET,
        //                    curr_doc->title_level, target_doc->title_level);
        //                press_enter_to_continue();
        //                continue;
        //            }
        //        }

        //        printf("\n--- 该患者过往临床医疗记录 ---\n");
        //        MedicalRecord* mr = target->record_head;
        //        int valid_record_count = 0;
        //        while (mr) {
        //            if (strcmp(mr->type, "缴费发票") != 0 && strcmp(mr->type, "退费凭证") != 0 && strcmp(mr->type, "收费") != 0) {
        //                printf("[%s] 类型:%s | 责任人:%-8s | 内容:%s\n", mr->date, mr->type, mr->doctor_name, mr->content);
        //                valid_record_count++;
        //            }
        //            mr = mr->next;
        //        }
        //        if (valid_record_count == 0) printf(" (暂无临床相关的过往就诊记录)\n");

        //        printf(COLOR_GREEN "\n [提示] 正在诊治患者: %s (医生认真检查中...)\n" COLOR_RESET, target->name);

        //        char all_prescriptions[300] = { 0 };
        //        char input_buf[100] = { 0 };
        //        target->fee_medicine = 0.0f;
        //        printf("\n [药品开具] 支持多次录入。输入'结束'或'无'停止录入。\n");

        //        while (1) {
        //            printf(" > 输入处方药品名 (学名/商品名/别名) 或直接输入 药名*数量: ");
        //            (void)scanf("%s", input_buf);
        //            while (getchar() != '\n');

        //            if (strcmp(input_buf, "结束") == 0 || strcmp(input_buf, "无") == 0) break;

        //            int qty = 1;
        //            int qty_error = 0;
        //            char* star_pos = strchr(input_buf, '*');
        //            if (star_pos != NULL) {
        //                *star_pos = '\0';
        //                qty = atoi(star_pos + 1);
        //                if (qty <= 0) qty_error = 1;
        //            }

        //            Medicine* matched_med = NULL;
        //            Medicine* m = g_medicine_head;
        //            while (m) {
        //                if (strcmp(m->common_name, input_buf) == 0 ||
        //                    strcmp(m->trade_name, input_buf) == 0 ||
        //                    strcmp(m->alias, input_buf) == 0) {
        //                    matched_med = m;
        //                    break;
        //                }
        //                m = m->next;
        //            }

        //            char temp_format[150] = { 0 };

        //            if (matched_med) {
        //                int has_permission = 0;
        //                if (matched_med->allowed_depts[0] == 1) has_permission = 1;
        //                else if (matched_med->allowed_depts[curr_doc->dept_id] == 1) has_permission = 1;

        //                if (!has_permission) {
        //                    printf(COLOR_RED "   [警告] 该药品(%s)非本科室(%s)常用/专用药！\n" COLOR_RESET,
        //                        matched_med->common_name, get_dept_name(curr_doc->dept_id));
        //                    char force_open;
        //                    printf("   是否强制越权开具？(Y继续 / N取消): ");
        //                    (void)scanf(" %c", &force_open);
        //                    while (getchar() != '\n');
        //                    if (force_open != 'Y' && force_open != 'y') {
        //                        printf(COLOR_YELLOW "   [取消] 已撤销该药品的录入。\n" COLOR_RESET);
        //                        continue;
        //                    }
        //                    printf(COLOR_YELLOW "   [记录] 医生强制越权开具了此药品。\n" COLOR_RESET);
        //                }

        //                if (qty_error) {
        //                    printf(COLOR_RED "   [异常] 检测到后缀数量输入异常(为0或非纯数字)。\n" COLOR_RESET);
        //                    int fix_choice = 0;
        //                    while (1) {
        //                        printf("   请确认操作: 1. 放弃开具此药  2. 修正数量为默认 1 份 \n   请选择: ");
        //                        if (scanf("%d", &fix_choice) == 1 && (fix_choice == 1 || fix_choice == 2)) {
        //                            while (getchar() != '\n'); break;
        //                        }
        //                        while (getchar() != '\n');
        //                        printf(COLOR_RED "   [错误] 无效选择，请输入 1 或 2！\n" COLOR_RESET);
        //                    }
        //                    if (fix_choice == 1) continue;
        //                    else qty = 1;
        //                }
        //                else if (star_pos == NULL) {
        //                    printf("   请输入开具数量: ");
        //                    if (scanf("%d", &qty) != 1 || qty <= 0) {
        //                        while (getchar() != '\n');
        //                        printf(COLOR_RED "   [异常] 检测到数量输入异常(为0或非纯数字)。\n" COLOR_RESET);
        //                        int fix_choice = 0;
        //                        while (1) {
        //                            printf("   请确认操作: 1. 放弃开具此药  2. 修正数量为默认 1 份 \n   请选择: ");
        //                            if (scanf("%d", &fix_choice) == 1 && (fix_choice == 1 || fix_choice == 2)) {
        //                                while (getchar() != '\n'); break;
        //                            }
        //                            while (getchar() != '\n');
        //                            printf(COLOR_RED "   [错误] 无效选择，请输入 1 或 2！\n" COLOR_RESET);
        //                        }
        //                        if (fix_choice == 1) continue;
        //                        else qty = 1;
        //                    }
        //                    else { while (getchar() != '\n'); }
        //                }

        //                float cost = matched_med->price * qty;
        //                printf(COLOR_CYAN "   [系统匹配] 成功锁定库房药品: [%s] (单价:%.2f元) x%d, 小计: %.2f元\n" COLOR_RESET,
        //                    matched_med->common_name, matched_med->price, qty, cost);

        //                target->fee_medicine += cost;
        //                snprintf(temp_format, sizeof(temp_format), "%s*%d", matched_med->common_name, qty);

        //                if (strlen(all_prescriptions) + strlen(temp_format) + 2 < sizeof(all_prescriptions)) {
        //                    if (strlen(all_prescriptions) > 0) strcat(all_prescriptions, ",");
        //                    strcat(all_prescriptions, temp_format);
        //                }
        //                else {
        //                    printf(COLOR_RED "   [警告] 处方容量已达上限，无法继续添加该药！\n" COLOR_RESET);
        //                }
        //            }
        //            else {
        //                printf(COLOR_YELLOW "   [系统警告] 药品字典中未找到药品 '%s'！\n" COLOR_RESET, input_buf);
        //                printf("   1. 我输错了，取消添加此药。\n");
        //                printf("   2. 确认开具此药，并通知药房采购进货。\n");
        //                int unk_choice = 0;
        //                while (1) {
        //                    printf("   请选择处理方式: ");
        //                    if (scanf("%d", &unk_choice) == 1 && (unk_choice == 1 || unk_choice == 2)) {
        //                        while (getchar() != '\n'); break;
        //                    }
        //                    while (getchar() != '\n');
        //                    printf(COLOR_RED "   [错误] 选项无效，仅允许输入 1 或 2！\n" COLOR_RESET);
        //                }
        //                if (unk_choice == 2) {
        //                    if (star_pos == NULL) {
        //                        printf("   请输入需要采购的开具数量: ");
        //                        if (scanf("%d", &qty) != 1 || qty <= 0) { qty = 1; }
        //                        while (getchar() != '\n');
        //                    }
        //                    snprintf(temp_format, sizeof(temp_format), "[缺药待采]%s*%d", input_buf, qty);
        //                    if (strlen(all_prescriptions) + strlen(temp_format) + 2 < sizeof(all_prescriptions)) {
        //                        if (strlen(all_prescriptions) > 0) strcat(all_prescriptions, ",");
        //                        strcat(all_prescriptions, temp_format);
        //                        printf(COLOR_GREEN "   [记录成功] 已将采购需求写入处方，将流转至药房终端处理。\n" COLOR_RESET);
        //                    }
        //                    else {
        //                        printf(COLOR_RED "   [警告] 处方容量已达上限，无法继续添加！\n" COLOR_RESET);
        //                    }
        //                }
        //                else { printf("   [取消] 已撤销该条药品的录入。\n"); }
        //            }
        //        }

        //        if (strlen(all_prescriptions) == 0) strcpy(all_prescriptions, "无药处方");

        //        if (SecondaryConfirm("下达本次医嘱并结束诊疗")) {
        //            (void)strcpy(target->prescription, all_prescriptions);
        //            float add_check = 0.0f, add_treat = 0.0f;
        //            printf(" > 附加检查费 附加诊疗费(空格分隔，无则填0 0): ");
        //            (void)scanf("%f %f", &add_check, &add_treat);
        //            while (getchar() != '\n');
        //            target->fee_check += add_check;
        //            target->fee_treat += add_treat;
        //            target->is_paid = 0;

        //            if (curr_doc->dept_id == 1) {
        //                while (1) {
        //                    printf(" > 是否需要住院(0:否, 1:常规病房, 2:紧急转入ICU): ");
        //                    if (scanf("%d", &target->need_inpatient) == 1 && (target->need_inpatient >= 0 && target->need_inpatient <= 2)) {
        //                        while (getchar() != '\n'); break;
        //                    }
        //                    while (getchar() != '\n');
        //                    printf(COLOR_RED "   [错误] 只能输入0, 1 或 2！\n" COLOR_RESET);
        //                }
        //                if (target->need_inpatient == 2) {
        //                    if (doctor_admit_to_icu(target)) {
        //                        printf(COLOR_CYAN "\n [紧急提示] 已成功为该患者锁定急诊ICU床位！\n" COLOR_RESET);
        //                    }
        //                    else {
        //                        printf(COLOR_RED "\n [紧急警告] ICU已满！患者转为普通流程！\n" COLOR_RESET);
        //                        target->need_inpatient = 1;
        //                    }
        //                }
        //            }
        //            else {
        //                while (1) {
        //                    printf(" > 是否需要住院(1:是, 0:否): ");
        //                    if (scanf("%d", &target->need_inpatient) == 1 && (target->need_inpatient == 0 || target->need_inpatient == 1)) {
        //                        while (getchar() != '\n'); break;
        //                    }
        //                    while (getchar() != '\n');
        //                    printf(COLOR_RED "   [错误] 只能输入0 或 1！\n" COLOR_RESET);
        //                }
        //            }

        //            printf(" > 医嘱说明: ");
        //            (void)scanf("%s", target->instructions);

        //            MedicalRecord* nr = (MedicalRecord*)malloc(sizeof(MedicalRecord));
        //            if (nr) {
        //                (void)strcpy(nr->type, "看诊");
        //                (void)strcpy(nr->doctor_name, curr_doc->name);
        //                (void)sprintf(nr->content, "处方:[%s]; 说明:%s", target->prescription, target->instructions);
        //                get_current_datetime_str(nr->date);
        //                nr->next = target->record_head;
        //                target->record_head = nr;
        //            }

        //            if (target->status != 3) target->status = 1;
        //            printf(COLOR_GREEN "\n [成功] 医嘱已下达，请提示患者前往收费处缴费！\n" COLOR_RESET);
        //        }
        //    }
        //    else {
        //        printf(COLOR_RED "\n [错误] 未找到该患者。\n" COLOR_RESET);
        //    }
        //    press_enter_to_continue();
        //}
        else if (choice == 2) {
            ViewAllPatientsReadonly();
        }
        else if (choice == 3) {
            if (ChangeDoctorPassword(curr_doc)) break;
            press_enter_to_continue();
        }
        else if (choice == 0) {
            run = 0;
        }
    }
}
