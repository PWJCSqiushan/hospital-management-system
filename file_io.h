/*
 * Copyright (c) 2026, HIS Team.
 * 文件名称：file_io.h
 * 文件摘要：文件持久化功能头文件
 * 修改记录：【升级6.0 新增】
 * 1. 提供保存/加载全部系统数据的接口。
 * 2. 数据文件格式采用自定义二进制格式。
 */

#pragma once
#ifndef FILE_IO_H
#define FILE_IO_H

#ifdef __cplusplus
extern "C" {
#endif

	/*
	 * 保存全部数据到文件 "his_data.dat"
	 * 返回值：1 表示成功，0 表示失败
	 * 说明：保存内容包含医生、患者（含病历）、药品、病房、预约记录及当前模拟时间。
	 */
	int save_all_data(void);

	/*
	 * 从文件 "his_data.dat" 加载全部数据
	 * 返回值：1 表示成功，0 表示失败（文件不存在或格式错误）
	 * 说明：加载时会清空现有内存数据，然后重建链表。
	 */
	int load_all_data(void);

#ifdef __cplusplus
}
#endif

#endif // FILE_IO_H