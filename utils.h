/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：utils.h
 * 文件摘要：通用工具函数声明
 * 修改记录：【升级6.0】
 * 1. 新增模拟时间管理相关函数声明。
 * 2. 保留原有清屏、按键等待功能。
 */

#pragma once
#ifndef UTILS_H
#define UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

	/* ==================== 控制台交互函数 ==================== */

	/* 清屏函数（跨平台） */
	void clear_screen(void);

	/* 按任意键继续（不回显） */
	void press_enter_to_continue(void);

	/* 读取单个无回显按键（用于方向键菜单交互） */
	int get_single_key(void);

	/* ==================== 模拟时间管理函数（6.0新增） ==================== */

	/*
	 * 初始化系统模拟时间
	 * 参数：year, month, day - 要设置的初始日期
	 */
	void init_system_time(int year, int month, int day);

	/*
	 * 将系统模拟时间推进一天
	 */
	void advance_one_day(void);

	/*
	 * 获取当前模拟日期字符串
	 * 参数：buffer - 输出缓冲区，至少11字节
	 * 格式：YYYY-MM-DD
	 */
	void get_current_date_str(char* buffer);

	/*
	 * 获取当前模拟日期时间字符串
	 * 参数：buffer - 输出缓冲区，至少20字节
	 * 格式：YYYY-MM-DD HH:MM
	 */
	void get_current_datetime_str(char* buffer);

/* 交互式 Y/N 提示，直到用户输入有效为止。返回 1 表示 Y/是，0 表示 N/否。 */
int ask_yes_no(const char* prompt);

#ifdef __cplusplus
}
#endif

#endif // UTILS_H