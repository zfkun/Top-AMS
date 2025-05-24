"""
convert_html_to_hpp.py
功能：将HTML文件转换为HPP头文件，并添加自定义头尾注释
"""

import argparse

def convert_html_to_hpp(input_file, output_file, header_str="", footer_str=""):
    try:
        # 读取原始HTML内容
        with open(input_file, 'r', encoding='utf-8') as f:
            html_content = f.read()

        # 拼接新内容
        new_content = f"{header_str}\n{html_content}\n{footer_str}"

        # 写入HPP文件
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(new_content)
            
        print(f"转换成功！生成文件: {output_file}")

    except Exception as e:
        print(f"错误: {str(e)}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="HTML转HPP工具")
    parser.add_argument('-i', '--input', default='index.html', help='输入文件路径')
    parser.add_argument('-o', '--output', default='index.hpp', help='输出文件路径')
    parser.add_argument('--header', 
                        default=
                        '#pragma once\n'
                        '#include <string>\n\n' 
                        'inline const std::string web = R"rawliteral(',
                          help='头部自定义字符串')
    
    parser.add_argument('--footer', default=
                        ')rawliteral";',
                          help='尾部自定义字符串')
    
    args = parser.parse_args()
    
    convert_html_to_hpp(
        input_file=args.input,
        output_file=args.output,
        header_str=args.header,
        footer_str=args.footer
    )
# deepseek写的,没仔细看
# 未来也可考虑在这里压缩一下html