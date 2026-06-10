/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：admin.cpp
 * 修改摘要：【升级6.0 - 医生管理及持久化支持】
 * 1. 新增医生管理子菜单：查看在职医生、新增医生、开除医生、查看离职医生。
 * 2. 开除医生前检查是否有待诊患者或未来预约，有则拒绝。
 * 3. 离职医生仅标记 is_active = 0，保留历史记录。
 * 4. 所有时间戳改为使用模拟时间函数。
 * 5. 统一姓名显示列宽为 %-22s。
 * 6. 管理员主菜单保持上下键高亮交互。
 */

#define _CRT_SECURE_NO_WARNINGS
#include "hospital_system.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ADMIN_ID       9
#define ADMIN_PWD      "a"

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"
#define COLOR_MAGENTA "\033[35m"

static void admin_patient_query(void);
static void admin_dept_query(void);
static void admin_ward_manage(void);
static void admin_finance_query(void);
static void admin_doctor_manage(void);
static void admin_save_data(void);
static Ward* sort_wards_by_room(void);
static void admin_draw_ward_map(void);
static void admin_doctor_board(void);

static int can_dismiss_doctor(int doctor_id);
static const char* get_status_color(int s);

static const char* dept_name(int id) {
    switch (id) {
    case 1: return "急诊科";
    case 2: return "内科";
    case 3: return "外科";
    case 4: return "儿科";
    case 5: return "妇科";
    default: return "未知科室";
    }
}

static const char* status_str(int s) {
    switch (s) {
    case 0: return "已挂号待接诊";
    case 1: return "看诊完毕待缴费";
    case 2: return "已缴费待取药/住院";
    case 3: return "住院中";
    case 4: return "就诊结束/已出院";
    default: return "状态异常";
    }
}

static const char* ward_type_str(int t) {
    switch (t) {
    case 1: return "普通病房";
    case 2: return "VIP病房";
    case 3: return "ICU";
    default: return "无";
    }
}

static const char* get_status_color(int s) {
    switch (s) {
    case 0: return COLOR_CYAN;
    case 1:
    case 2: return COLOR_YELLOW;
    case 3: return COLOR_RED;
    case 4: return COLOR_GREEN;
    default: return COLOR_RESET;
    }
}

static Ward* sort_wards_by_room(void) {
    Ward* head = NULL;
    Ward* p = g_ward_head;

    while (p != NULL) {
        Ward* w = (Ward*)malloc(sizeof(Ward));
        if (!w) break;
        memcpy(w, p, sizeof(Ward));
        w->next = NULL;

        if (head == NULL || w->room_no < head->room_no) {
            w->next = head;
            head = w;
        }
        else {
            Ward* t = head;
            while (t->next != NULL && t->next->room_no < w->room_no) t = t->next;
            w->next = t->next;
            t->next = w;
        }
        p = p->next;
    }
    return head;
}

static void get_patients_in_room(int room_no, int* pids, int max_beds) {
    for (int i = 0; i < max_beds; i++) pids[i] = 0;
    int count = 0;
    Patient* p = g_patient_head;
    while (p && count < max_beds) {
        if (p->status == 3 && p->room_no == room_no) {
            pids[count++] = p->id;
        }
        p = p->next;
    }
}

static const char* local_title_name(int level) {
    switch (level) {
    case 1: return "主任医师";
    case 2: return "副主任医师";
    case 3: return "普通医师";
    default: return "未知";
    }
}

static void admin_draw_ward_map(void) {
    clear_screen();
    printf(COLOR_CYAN "=========================================================================\n");
    printf("                       当前全院病房占用情况示意图\n");
    printf("=========================================================================\n" COLOR_RESET);

    Ward* sorted = sort_wards_by_room();

    printf(COLOR_RED "\n【急诊 ICU 病房区】\n" COLOR_RESET);
    Ward* w = sorted;
    while (w) {
        if (w->type == 3) {
            int pids[10];
            get_patients_in_room(w->room_no, pids, 10);
            printf(" [房间号: %d | 归属: %s]\n", w->room_no, dept_name(w->dept_id));
            printf(" +-----------+-----------+-----------+-----------+-----------+\n");
            for (int i = 0; i < 10; i++) {
                if (i % 5 == 0 && i != 0) printf(" |\n +-----------+-----------+-----------+-----------+-----------+\n");
                if (pids[i] == 0) printf(" |" COLOR_GREEN "  空闲中  " COLOR_RESET);
                else printf(" |" COLOR_RED " %-8d " COLOR_RESET, pids[i]);
            }
            printf(" |\n +-----------+-----------+-----------+-----------+-----------+\n");
        }
        w = w->next;
    }

    printf(COLOR_YELLOW "\n【VIP 病房区】\n" COLOR_RESET);
    w = sorted;
    int vip_count = 0;
    while (w) {
        if (w->type == 2) {
            int pids[1];
            get_patients_in_room(w->room_no, pids, 1);
            if (vip_count % 5 == 0 && vip_count != 0) printf("\n");
            printf(" [%d] ", w->room_no);
            if (pids[0] == 0) printf("[" COLOR_GREEN "  空闲中  " COLOR_RESET "]  ");
            else printf("[" COLOR_RED " %-8d " COLOR_RESET "]  ", pids[0]);
            vip_count++;
        }
        w = w->next;
    }
    printf("\n");

    printf(COLOR_CYAN "\n【普通 病房区】\n" COLOR_RESET);
    w = sorted;
    int normal_count = 0;
    while (w) {
        if (w->type == 1) {
            int pids[2];
            get_patients_in_room(w->room_no, pids, 2);
            if (normal_count % 4 == 0 && normal_count != 0) printf("\n\n");
            printf(" 房%d: ", w->room_no);
            for (int i = 0; i < 2; i++) {
                if (pids[i] == 0) printf("[" COLOR_GREEN " 空闲 " COLOR_RESET "]");
                else printf("[" COLOR_RED "%-6d" COLOR_RESET "]", pids[i]);
            }
            printf("   ");
            normal_count++;
        }
        w = w->next;
    }
    printf("\n\n");

    while (sorted) {
        Ward* d = sorted;
        sorted = sorted->next;
        free(d);
    }
    press_enter_to_continue();
}

static void draw_admin_menu(int current_select) {
    clear_screen();
    printf(COLOR_MAGENTA "=====================================================\n");
    printf("                   管理员主菜单\n");
    printf("=====================================================\n" COLOR_RESET);

    const char* options[] = {
        " 0. 退出登录（返回主菜单）",
        " 1. 患者信息查询",
        " 2. 科室信息查询",
        " 3. 住院部入住情况管理",
        " 4. 资金流动查询",
        " 5. 医生管理",
        " 6. 全院医生排班与待诊看板"
    };
    int option_codes[] = { 0, 1, 2, 3, 4, 5, 6 };

    for (int i = 0; i < 7; i++) {
        if (i == current_select) {
            printf("\033[45;37m%s\033[0m\n", options[i]);
        }
        else {
            if (option_codes[i] == 0) printf(COLOR_CYAN "%s\n" COLOR_RESET, options[i]);
            else printf("%s\n", options[i]);
        }
    }
    printf("=====================================================\n");
    printf("当前系统日期: %s\n", g_current_date);
    printf("请使用键盘【↑】【↓】选择，按【Enter】确认，或直接输入数字键。\n");
}

void admin_function(void) {
    int id;
    char pwd[20];
    clear_screen();
    printf(COLOR_MAGENTA "=====================================================\n");
    printf("                 医院管理员控制台\n");
    printf("=====================================================\n" COLOR_RESET);
    printf("请输入管理员工号：");
    if (scanf("%d", &id) != 1) { while (getchar() != '\n'); return; }
    printf("请输入管理员密码：");
    (void)scanf("%s", pwd);

    if (id != ADMIN_ID || strcmp(pwd, ADMIN_PWD) != 0) {
        printf(COLOR_RED "\n权限不足，登录失败！\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    int current_select = 1;
    int option_codes[] = { 0, 1, 2, 3, 4, 5, 6 };
    int max_options = 7;
    int run = 1;

    while (run) {
        draw_admin_menu(current_select);
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
            case 1: admin_patient_query(); break;
            case 2: admin_dept_query(); break;
            case 3: admin_ward_manage(); break;
            case 4: admin_finance_query(); break;
            case 5: admin_doctor_manage(); break;
            case 6: admin_doctor_board(); break;
            case 0: admin_save_data(); run = 0; break;
            }
        }
        else if (key >= '0' && key <= '6') {
            int direct_choice = key - '0';
            for (int i = 0; i < max_options; i++) {
                if (option_codes[i] == direct_choice) {
                    current_select = i;
                    switch (direct_choice) {
                    case 1: admin_patient_query(); break;
                    case 2: admin_dept_query(); break;
                    case 3: admin_ward_manage(); break;
                    case 4: admin_finance_query(); break;
                    case 5: admin_doctor_manage(); break;
                    case 6: admin_doctor_board(); break;
                    case 0: admin_save_data(); run = 0; break;
                    }
                    break;
                }
            }
        }
    }
}

static void admin_patient_query(void) {
    int current_select = 1;
    const char* options[] = {
        " 0. 返回上一级菜单",
        " 1. 查看全部患者",
        " 2. 按患者ID查询",
        " 3. 按患者姓名查询",
        " 4. 按科室查询",
        " 5. 按就诊状态查询",
        " 6. 导出患者信息至文件"
    };
    int option_codes[] = { 0, 1, 2, 3, 4, 5, 6 };
    int max_options = 7;
    int way = -1;

    while (way == -1) {
        clear_screen();
        printf(COLOR_CYAN "=====================================================\n");
        printf("                   患者信息查询\n");
        printf("=====================================================\n" COLOR_RESET);

        for (int i = 0; i < max_options; i++) {
            if (i == current_select) {
                printf("\033[45;37m%s\033[0m\n", options[i]);
            }
            else {
                printf("%s\n", options[i]);
            }
        }
        printf("=====================================================\n");
        printf("当前系统日期: %s\n", g_current_date);
        printf("请使用键盘【↑】【↓】选择，按【Enter】确认，或直接输入数字键。\n");

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
            way = option_codes[current_select];
        }
        else if (key >= '0' && key <= '6') {
            way = key - '0';
            for (int i = 0; i < max_options; i++) {
                if (option_codes[i] == way) { current_select = i; break; }
            }
        }
    }
    if (way == 0) return;

    clear_screen();
    Patient* p = g_patient_head;
    if (!p) {
        printf("暂无患者数据。\n");
        press_enter_to_continue();
        return;
    }

    int target_id = 0, target_dept = 0, target_status = -1;
    char target_name[30] = { 0 };

    if (way == 2) { printf("请输入患者ID："); (void)scanf("%d", &target_id); }
    if (way == 3) { printf("请输入患者姓名："); (void)scanf("%s", target_name); }
    if (way == 4) { printf("请输入科室编号(1-5)："); (void)scanf("%d", &target_dept); }
    if (way == 5) { printf("请输入就诊状态(0-4)："); (void)scanf("%d", &target_status); }

    if (way == 6) {
        FILE* f = fopen("patient_list.txt", "w");
        if (f) {
            Patient* tp = g_patient_head;
            while (tp) {
                fprintf(f, "ID:%d 姓名:%s 科室:%s 状态:%s 缴费:%s\n",
                    tp->id, tp->name, dept_name(tp->dept_id),
                    status_str(tp->status), tp->is_paid ? "已缴" : "未缴");
                tp = tp->next;
            }
            fclose(f);
            printf(COLOR_GREEN "导出成功：patient_list.txt\n" COLOR_RESET);
        }
        else printf(COLOR_RED "导出失败！\n" COLOR_RESET);
        press_enter_to_continue();
        return;
    }

    printf("\n");
    while (p) {
        int ok = 1;
        if (way == 2 && p->id != target_id) ok = 0;
        if (way == 3 && strcmp(p->name, target_name) != 0) ok = 0;
        if (way == 4 && p->dept_id != target_dept) ok = 0;
        if (way == 5 && p->status != target_status) ok = 0;

        if (ok) {
            const char* s_col = get_status_color(p->status);
            printf(" ID:%-8d | 姓名:%-22s | 性别:%-4s | 科室:%-8s | 状态:%s%-16s%s\n",
                p->id, p->name, p->gender, dept_name(p->dept_id),
                s_col, status_str(p->status), COLOR_RESET);

            printf(" 费用账单 | 诊疗:%-6.2f | 检查:%-6.2f | 药品:%-6.2f | 住院:%-6.2f | 结清情况:%s\n",
                p->fee_treat, p->fee_check, p->fee_medicine, p->fee_ward,
                p->is_paid ? COLOR_GREEN "已结清" COLOR_RESET : COLOR_RED "未结清" COLOR_RESET);

            printf(" 临床医嘱 | 住院类型:%-8s | 房号:%-6d | 入院日期:%-12s | 出院日期:%-12s\n",
                ward_type_str(p->ward_type), p->room_no,
                strlen(p->admit_date) ? p->admit_date : "无",
                strlen(p->discharge_date) ? p->discharge_date : "无");
            printf(" 医嘱说明:%s\n", p->instructions);
            printf("--------------------------------------------------------------------------------------\n");
        }
        p = p->next;
    }

    press_enter_to_continue();
}

static void admin_dept_query(void) {
    int current_select = 1;
    const char* options[] = {
        " 0. 返回上一级菜单",
        " 1. 查看全部科室统计"
    };
    int option_codes[] = { 0, 1 };
    int max_options = 2;
    int op = -1;

    while (op == -1) {
        clear_screen();
        printf(COLOR_CYAN "=====================================================\n");
        printf("                   科室信息查询\n");
        printf("=====================================================\n" COLOR_RESET);

        for (int i = 0; i < max_options; i++) {
            if (i == current_select) {
                printf("\033[45;37m%s\033[0m\n", options[i]);
            }
            else {
                printf("%s\n", options[i]);
            }
        }
        printf("=====================================================\n");
        printf("当前系统日期: %s\n", g_current_date);
        printf("请使用键盘【↑】【↓】选择，按【Enter】确认，或直接输入数字键。\n");

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
            op = option_codes[current_select];
        }
        else if (key >= '0' && key <= '1') {
            op = key - '0';
            for (int i = 0; i < max_options; i++) {
                if (option_codes[i] == op) { current_select = i; break; }
            }
        }
    }

    if (op == 0) return;

    clear_screen();
    int cnt[6] = { 0 };
    float inc[6] = { 0 };
    Patient* p = g_patient_head;

    while (p) {
        int d = p->dept_id;
        if (d >= 1 && d <= 5) {
            cnt[d]++;
            inc[d] += p->fee_treat + p->fee_check + p->fee_medicine + p->fee_ward;
        }
        p = p->next;
    }

    for (int i = 1; i <= 5; ++i) {
        printf(COLOR_CYAN "【%s】(%d)\n" COLOR_RESET, dept_name(i), i);
        printf("  接诊人数：%d\n", cnt[i]);
        printf("  科室营收：%.2f 元\n", inc[i]);
        printf("  科室医生：");

        Doctor* d = g_doctor_head;
        while (d) {
            if (d->dept_id == i && d->is_active == 1) printf("%s ", d->name);
            d = d->next;
        }
        printf("\n-----------------------------------------------------\n");
    }

    press_enter_to_continue();
}

static void admin_ward_manage(void) {
    int current_select = 1;
    const char* options[] = {
        " 0. 返回上一级菜单",
        " 1. 查看全部病房（按房号升序）",
        " 2. 手动办理患者出院",
        COLOR_YELLOW " 6. 当前病房占用情况示意图 (格子图)" COLOR_RESET
    };
    int option_codes[] = { 0, 1, 2, 6 };
    int max_options = 4;
    int op = -1;

    while (op == -1) {
        clear_screen();
        printf(COLOR_CYAN "=====================================================\n");
        printf("                 住院部入住管理\n");
        printf("=====================================================\n" COLOR_RESET);

        for (int i = 0; i < max_options; i++) {
            if (i == current_select) {
                printf("\033[45;37m%s\033[0m\n", options[i]);
            }
            else {
                if (option_codes[i] == 6) printf(COLOR_YELLOW "%s\n" COLOR_RESET, options[i]);
                else printf("%s\n", options[i]);
            }
        }
        printf("=====================================================\n");
        printf("当前系统日期: %s\n", g_current_date);
        printf("请使用键盘【↑】【↓】选择，按【Enter】确认，或直接输入数字键。\n");

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
            op = option_codes[current_select];
        }
        else if (key == '0' || key == '1' || key == '2' || key == '6') {
            op = key - '0';
            for (int i = 0; i < max_options; i++) {
                if (option_codes[i] == op) { current_select = i; break; }
            }
        }
    }

    if (op == 0) return;

    if (op == 6) {
        admin_draw_ward_map();
        return;
    }

    clear_screen();
    Ward* sorted = sort_wards_by_room();
    Ward* w = sorted;

    if (!w) {
        printf("无病房数据。\n");
        press_enter_to_continue();
        return;
    }

    if (op == 2) {
        int method, target_val;
        printf(" 1. 按患者ID办理出院\n 2. 按房间号办理出院\n 请选择: ");
        if (scanf("%d", &method) != 1 || (method != 1 && method != 2)) {
            while (getchar() != '\n'); return;
        }

        Patient* target_p = NULL;

        if (method == 1) {
            printf("请输入患者ID: ");
            (void)scanf("%d", &target_val);
            Patient* p = g_patient_head;
            while (p) {
                if (p->status == 3 && p->id == target_val) {
                    target_p = p;
                    break;
                }
                p = p->next;
            }
        }
        else if (method == 2) {
            printf("请输入房间号: ");
            (void)scanf("%d", &target_val);

            Patient* room_patients[10];
            int p_count = 0;
            Patient* p = g_patient_head;

            while (p) {
                if (p->status == 3 && p->room_no == target_val) {
                    if (p_count < 10) room_patients[p_count++] = p;
                }
                p = p->next;
            }

            if (p_count == 0) {
                printf(COLOR_YELLOW ">> 该房间当前为空或不存在。\n" COLOR_RESET);
                press_enter_to_continue();
                return;
            }

            printf("\n房间 [%d] 当前占用患者列表：\n", target_val);
            for (int i = 0; i < p_count; i++) {
                printf(" [%d] ID: %-8d | 姓名: %-22s | 费用结清状态: %s\n",
                    i + 1, room_patients[i]->id, room_patients[i]->name,
                    room_patients[i]->is_paid ? COLOR_GREEN"已结清" COLOR_RESET : COLOR_RED"未结清" COLOR_RESET);
            }

            int select_idx = 0;
            printf("\n请输入要办理出院的患者对应序号 (输入 0 取消): ");
            if (scanf("%d", &select_idx) == 1 && select_idx > 0 && select_idx <= p_count) {
                target_p = room_patients[select_idx - 1];
            }
            else {
                printf(COLOR_YELLOW ">> 操作已取消。\n" COLOR_RESET);
                while (sorted) { Ward* d = sorted; sorted = sorted->next; free(d); }
                press_enter_to_continue();
                return;
            }
        }

        if (target_p) {
            printf("\n【准备办理出院】\n患者信息：ID:%d | 姓名:%s | 病房号:%d\n", target_p->id, target_p->name, target_p->room_no);

            if (target_p->is_paid == 0) {
                printf(COLOR_RED "[警告] 该患者仍有未结清的账单！强制出院可能导致账目不平。\n" COLOR_RESET);
            }

            if (ask_yes_no("请再次确认办理出院操作？(Y/N): ")) {
                target_p->status = 4;
                Ward* wt = g_ward_head;
                while (wt) {
                    if (wt->room_no == target_p->room_no && wt->occupied_beds > 0) {
                        wt->occupied_beds--; break;
                    }
                    wt = wt->next;
                }
                get_current_date_str(target_p->discharge_date);
                target_p->room_no = 0;
                printf(COLOR_GREEN "该患者已成功办理出院。\n" COLOR_RESET);
            }
            else {
                printf(COLOR_YELLOW "出院操作已取消。\n" COLOR_RESET);
            }
        }
        else {
            printf(COLOR_RED "未找到匹配且正在住院中的患者。\n" COLOR_RESET);
        }
        press_enter_to_continue();
        return;
    }

    printf("\n病房列表（按房间号升序）：\n\n");
    int total = 0, occ = 0;
    while (w) {
        int empty = w->total_beds - w->occupied_beds;

        printf("房号:%d %-6s | 归属:%s | 总床:%d 占用:%d 空床:%d | 日费:%.0f\n",
            w->room_no, ward_type_str(w->type), dept_name(w->dept_id),
            w->total_beds, w->occupied_beds, empty, w->price_per_day);

        total += w->total_beds;
        occ += w->occupied_beds;
        w = w->next;
    }

    printf(COLOR_GREEN "\n全院住院入住率：%.1f %% \n" COLOR_RESET, total ? (float)occ / total * 100.0f : 0.0f);

    while (sorted) { Ward* d = sorted; sorted = sorted->next; free(d); }
    press_enter_to_continue();
}

static void admin_finance_query(void) {
    int current_select = 1;
    const char* options[] = {
        " 0. 返回上一级菜单",
        " 1. 查看全院总收入明细",
        " 2. 查看各科室营收占比",
        " 3. 导出资金流水至文件"
    };
    int option_codes[] = { 0, 1, 2, 3 };
    int max_options = 4;
    int op = -1;

    while (op == -1) {
        clear_screen();
        printf(COLOR_CYAN "=====================================================\n");
        printf("                   资金流动查询\n");
        printf("=====================================================\n" COLOR_RESET);

        for (int i = 0; i < max_options; i++) {
            if (i == current_select) {
                printf("\033[45;37m%s\033[0m\n", options[i]);
            }
            else {
                printf("%s\n", options[i]);
            }
        }
        printf("=====================================================\n");
        printf("当前系统日期: %s\n", g_current_date);
        printf("请使用键盘【↑】【↓】选择，按【Enter】确认，或直接输入数字键。\n");

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
            op = option_codes[current_select];
        }
        else if (key >= '0' && key <= '3') {
            op = key - '0';
            for (int i = 0; i < max_options; i++) {
                if (option_codes[i] == op) { current_select = i; break; }
            }
        }
    }

    if (op == 0) return;

    clear_screen();
    float t = 0, c = 0, m = 0, wd = 0;
    float dept[6] = { 0 };

    Patient* p = g_patient_head;
    while (p) {
        MedicalRecord* mr = p->record_head;
        while (mr) {
            if (strcmp(mr->type, "缴费发票") == 0) {
                float curr_t = 0, curr_c = 0, curr_m = 0, curr_wd = 0;

                // 改用短标识符解析，避免中文匹配问题
                const char* ptr_t = strstr(mr->content, "诊疗:");
                if (ptr_t) { float val; if (sscanf(ptr_t, "诊疗:%f", &val) == 1) curr_t = val; }
                const char* ptr_c = strstr(mr->content, "检查:");
                if (ptr_c) { float val; if (sscanf(ptr_c, "检查:%f", &val) == 1) curr_c = val; }
                const char* ptr_m = strstr(mr->content, "药费:");
                if (ptr_m) { float val; if (sscanf(ptr_m, "药费:%f", &val) == 1) curr_m = val; }
                const char* ptr_wd = strstr(mr->content, "床位:");
                if (ptr_wd) { float val; if (sscanf(ptr_wd, "床位:%f", &val) == 1) curr_wd = val; }

                t += curr_t;
                c += curr_c;
                m += curr_m;
                wd += curr_wd;

                float invoice_total = curr_t + curr_c + curr_m + curr_wd;
                if (p->dept_id >= 1 && p->dept_id <= 5) {
                    dept[p->dept_id] += invoice_total;
                }
            }
            else if (strcmp(mr->type, "退费凭证") == 0) {
                float refund = 0;
                const char* p_ref = strstr(mr->content, "退款金额:");
                if (p_ref) {
                    sscanf(p_ref, "退款金额:%f", &refund);
                    m -= refund;
                    if (p->dept_id >= 1 && p->dept_id <= 5) dept[p->dept_id] -= refund;
                }
            }
            mr = mr->next;
        }
        p = p->next;
    }

    float total = t + c + m + wd;

    float total_medicine_cost = 0.0f;
    Medicine* med = g_medicine_head;
    while (med) {
        total_medicine_cost += (med->purchase_price * med->total_sold);
        med = med->next;
    }
    float net_profit = total - total_medicine_cost;

    if (op == 1) {
        printf(COLOR_CYAN "================ 全院财务报表 ================\n" COLOR_RESET);
        printf(" [营业收入明细]\n");
        printf("  诊疗收入：￥%.2f\n", t);
        printf("  检查收入：￥%.2f\n", c);
        printf("  药品收入：￥%.2f\n", m);
        printf("  住院收入：￥%.2f\n", wd);
        printf(" ---------------------------------------------\n");
        printf(COLOR_GREEN "  全院总营业额(流水)：￥%.2f\n" COLOR_RESET, total);
        printf("\n [营业成本核算]\n");
        printf("  已售药品进货总成本：￥%.2f\n", total_medicine_cost);
        printf(" ---------------------------------------------\n");
        if (net_profit >= 0)
            printf(COLOR_MAGENTA "  全院毛利润 (流水 - 药耗)：￥%.2f\n" COLOR_RESET, net_profit);
        else
            printf(COLOR_RED "  全院毛利润 (流水 - 药耗)：￥%.2f (亏损!)\n" COLOR_RESET, net_profit);
    }
    else if (op == 2) {
        printf("各科室营收占比：\n");
        for (int i = 1; i <= 5; ++i) {
            printf("%-6s：" COLOR_YELLOW "%6.2f %% \n" COLOR_RESET,
                dept_name(i), total ? dept[i] / total * 100.0f : 0.0f);
        }
    }
    else if (op == 3) {
        FILE* f = fopen("finance_report.txt", "w");
        if (f) {
            fprintf(f, "诊疗:%.2f 检查:%.2f 药品:%.2f 住院:%.2f 总计:%.2f\n",
                t, c, m, wd, total);
            fclose(f);
            printf(COLOR_GREEN "导出成功：finance_report.txt\n" COLOR_RESET);
        }
        else printf(COLOR_RED "导出失败！\n" COLOR_RESET);
    }

    press_enter_to_continue();
}

static void admin_doctor_board(void) {
    clear_screen();
    printf(COLOR_CYAN "=========================================================================\n");
    printf("                       全院医生排班与当前待诊看板\n");
    printf("=========================================================================\n" COLOR_RESET);

    for (int dept = 1; dept <= 5; dept++) {
        printf(COLOR_YELLOW "\n【%s】\n" COLOR_RESET, dept_name(dept));
        printf("%-8s | %-22s | %-14s | %-8s | %s\n", "医生工号", "姓名", "职级", "挂号费", "当前待诊人数");
        printf("-------------------------------------------------------------------------\n");

        Doctor* d = g_doctor_head;
        int found = 0;
        while (d) {
            if (d->dept_id == dept && d->is_active == 1) {
                int wait_cnt = 0;
                Patient* p = g_patient_head;
                while (p) {
                    if (p->doctor_id == d->id && p->status == 0) wait_cnt++;
                    p = p->next;
                }

                printf(" %-7d | %-22s | %-14s | ￥%-6.2f | " COLOR_RED "%-4d人" COLOR_RESET "\n",
                    d->id, d->name, local_title_name(d->title_level), d->consultation_fee, wait_cnt);
                found = 1;
            }
            d = d->next;
        }
        if (!found) printf("  (暂无在职医生)\n");
    }
    printf("\n");
    press_enter_to_continue();
}

static int can_dismiss_doctor(int doctor_id) {
    Patient* p = g_patient_head;
    while (p) {
        if (p->doctor_id == doctor_id && p->status == 0) {
            return 0;
        }
        p = p->next;
    }

    Appointment* a = g_appointment_head;
    while (a) {
        if (a->doctor_id == doctor_id) {
            if (strcmp(a->date, g_current_date) >= 0) {
                return 0;
            }
        }
        a = a->next;
    }
    return 1;
}

static void admin_doctor_manage(void) {
    int current_select = 1;
    const char* options[] = {
        " 0. 返回上一级菜单",
        " 1. 查看在职医生列表",
        " 2. 新增医生",
        " 3. 开除医生（标记离职）",
        " 4. 查看已离职医生"
    };
    int option_codes[] = { 0, 1, 2, 3, 4 };
    int max_options = 5;
    int op = -1;

    while (op == -1) {
        clear_screen();
        printf(COLOR_CYAN "=====================================================\n");
        printf("                   医生管理\n");
        printf("=====================================================\n" COLOR_RESET);

        for (int i = 0; i < max_options; i++) {
            if (i == current_select) {
                printf("\033[45;37m%s\033[0m\n", options[i]);
            }
            else {
                printf("%s\n", options[i]);
            }
        }
        printf("=====================================================\n");
        printf("当前系统日期: %s\n", g_current_date);
        printf("请使用键盘【↑】【↓】选择，按【Enter】确认，或直接输入数字键。\n");

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
            op = option_codes[current_select];
        }
        else if (key >= '0' && key <= '4') {
            op = key - '0';
            for (int i = 0; i < max_options; i++) {
                if (option_codes[i] == op) { current_select = i; break; }
            }
        }
    }

    if (op == 0) return;

    clear_screen();

    if (op == 1) {
        printf(COLOR_CYAN "在职医生列表：\n" COLOR_RESET);
        printf("%-8s | %-22s | %-8s | %-14s | %-8s\n", "工号", "姓名", "科室", "职级", "挂号费");
        printf("-------------------------------------------------------------------------------\n");
        Doctor* d = g_doctor_head;
        int count = 0;
        while (d) {
            if (d->is_active == 1) {
                printf("%-8d | %-22s | %-8s | %-14s | ￥%-6.2f\n",
                    d->id, d->name, dept_name(d->dept_id), local_title_name(d->title_level), d->consultation_fee);
                count++;
            }
            d = d->next;
        }
        if (count == 0) printf("暂无在职医生。\n");
        press_enter_to_continue();
    }
    else if (op == 2) {
        Doctor* new_doc = (Doctor*)malloc(sizeof(Doctor));
        if (!new_doc) {
            printf("内存分配失败。\n");
            press_enter_to_continue();
            return;
        }
        memset(new_doc, 0, sizeof(Doctor));

        int max_id = 1000;
        Doctor* d = g_doctor_head;
        while (d) {
            if (d->id > max_id) max_id = d->id;
            d = d->next;
        }
        new_doc->id = max_id + 1;

        printf("请输入医生姓名: ");
        (void)scanf("%s", new_doc->name);
        printf("请输入初始密码: ");
        (void)scanf("%s", new_doc->password);
        printf("所属科室 (1:急诊 2:内科 3:外科 4:儿科 5:妇科): ");
        (void)scanf("%d", &new_doc->dept_id);
        printf("职级 (1:主任医师 2:副主任医师 3:普通医师): ");
        (void)scanf("%d", &new_doc->title_level);
        printf("挂号费: ");
        (void)scanf("%f", &new_doc->consultation_fee);

        new_doc->is_active = 1;

        new_doc->next = g_doctor_head;
        g_doctor_head = new_doc;

        printf(COLOR_GREEN "医生 %s (工号:%d) 添加成功！\n" COLOR_RESET, new_doc->name, new_doc->id);
        press_enter_to_continue();
    }
    else if (op == 3) {
        printf("请输入要开除的医生工号: ");
        int doc_id;
        (void)scanf("%d", &doc_id);

        Doctor* d = g_doctor_head;
        while (d && d->id != doc_id) d = d->next;

        if (!d) {
            printf(COLOR_RED "未找到该医生。\n" COLOR_RESET);
        }
        else if (d->is_active == 0) {
            printf(COLOR_YELLOW "该医生已是离职状态。\n" COLOR_RESET);
        }
        else {
            if (!can_dismiss_doctor(doc_id)) {
                printf(COLOR_RED "无法开除！该医生仍有待诊患者或未来预约。\n" COLOR_RESET);
            }
            else {
                printf("确认开除医生 %s 吗？(Y/N): ", d->name);
                char confirm;
                (void)scanf(" %c", &confirm);
                if (confirm == 'Y' || confirm == 'y') {
                    d->is_active = 0;
                    printf(COLOR_GREEN "医生 %s 已被标记为离职。\n" COLOR_RESET, d->name);
                }
                else {
                    printf("操作已取消。\n");
                }
            }
        }
        press_enter_to_continue();
    }
    else if (op == 4) {
        printf(COLOR_YELLOW "已离职医生列表：\n" COLOR_RESET);
        printf("%-8s | %-22s | %-8s | %-14s\n", "工号", "姓名", "科室", "职级");
        printf("----------------------------------------------------------\n");
        Doctor* d = g_doctor_head;
        int count = 0;
        while (d) {
            if (d->is_active == 0) {
                printf("%-8d | %-22s | %-8s | %-14s\n",
                    d->id, d->name, dept_name(d->dept_id), local_title_name(d->title_level));
                count++;
            }
            d = d->next;
        }
        if (count == 0) printf("暂无离职医生。\n");
        press_enter_to_continue();
    }
    else {
        printf(COLOR_RED "无效选项。\n" COLOR_RESET);
        press_enter_to_continue();
    }
}

static void admin_save_data(void) {
    if (save_all_data()) {
        printf(COLOR_GREEN "系统数据已持久化保存。\n" COLOR_RESET);
    }
    else {
        printf(COLOR_RED "数据保存失败！\n" COLOR_RESET);
    }
    press_enter_to_continue();
}