#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import re
import glob

# 定义测试目录
TEST_DIRS = ["api_tests", "cmd_tests"]
README_PATH = "README.md"

# 定义注释格式的正则表达式
# 匹配 // @TestInfo: 模块名|测试用例名|用例说明
TEST_INFO_PATTERN = re.compile(r'^\s*//\s*@TestInfo:\s*([^|]+)\|([^|]+)\|(.+)$')

def extract_test_info(file_path):
    """从测试文件中提取测试信息"""
    test_info_list = []
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
        for i, line in enumerate(lines):
            match = TEST_INFO_PATTERN.match(line)
            if match:
                module = match.group(1).strip()
                test_case = match.group(2).strip()
                description = match.group(3).strip()
                
                # 查找下一个t.Run行，确认测试用例名称
                for j in range(i+1, min(i+5, len(lines))):
                    if 't.Run(' in lines[j]:
                        test_info_list.append({
                            'module': module,
                            'test_case': test_case,
                            'description': description,
                            'file': os.path.basename(file_path)
                        })
                        break
    
    return test_info_list

def generate_test_table(test_info_list):
    """生成测试表格的Markdown内容"""
    if not test_info_list:
        return "暂无测试用例信息。"
    
    table = "| 功能模块 | 测试用例 | 用例说明 | 测试文件 |\n"
    table += "| ------- | ------- | ------- | ------- |\n"
    
    # 按模块和测试用例名称排序
    sorted_info = sorted(test_info_list, key=lambda x: (x['module'], x['test_case']))
    
    for info in sorted_info:
        table += f"| {info['module']} | {info['test_case']} | {info['description']} | {info['file']} |\n"
    
    return table

def update_readme(table_content):
    """更新README.md文件中的测试表格"""
    readme_content = ""
    table_start_marker = "<!-- TEST_TABLE_START -->"
    table_end_marker = "<!-- TEST_TABLE_END -->"
    
    # 如果README.md不存在，创建一个基本的README
    if not os.path.exists(README_PATH):
        readme_content = f"""# NFC Analysis Platform 测试

本目录包含 NFC Analysis Platform 的自动化测试。

## 测试用例

{table_start_marker}
{table_content}
{table_end_marker}
"""
        with open(README_PATH, 'w', encoding='utf-8') as f:
            f.write(readme_content)
        return
    
    # 读取现有的README内容
    with open(README_PATH, 'r', encoding='utf-8') as f:
        readme_content = f.read()
    
    # 检查是否存在表格标记
    if table_start_marker not in readme_content or table_end_marker not in readme_content:
        # 如果没有标记，在文件末尾添加表格
        readme_content += f"\n\n## 测试用例\n\n{table_start_marker}\n{table_content}\n{table_end_marker}\n"
    else:
        # 如果有标记，替换表格内容
        start_pos = readme_content.find(table_start_marker) + len(table_start_marker)
        end_pos = readme_content.find(table_end_marker)
        readme_content = readme_content[:start_pos] + "\n" + table_content + "\n" + readme_content[end_pos:]
    
    # 写入更新后的README内容
    with open(README_PATH, 'w', encoding='utf-8') as f:
        f.write(readme_content)

def main():
    """主函数"""
    all_test_info = []
    
    # 遍历测试目录
    for test_dir in TEST_DIRS:
        if not os.path.exists(test_dir):
            continue
        
        # 查找所有Go测试文件
        test_files = glob.glob(os.path.join(test_dir, "*_test.go"))
        for test_file in test_files:
            test_info = extract_test_info(test_file)
            all_test_info.extend(test_info)
    
    # 生成表格内容
    table_content = generate_test_table(all_test_info)
    
    # 更新README.md
    update_readme(table_content)
    
    print(f"已更新测试表格，共 {len(all_test_info)} 个测试用例。")

if __name__ == "__main__":
    main() 