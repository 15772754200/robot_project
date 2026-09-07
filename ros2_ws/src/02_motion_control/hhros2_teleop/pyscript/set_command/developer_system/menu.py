import os
import sys

def display_menu():
    """显示文件选择菜单"""
    print("\n" + "="*50)
    print("developer_system 文件浏览器")
    print("="*50)
    
    # 根据图片中的文件名显示中文菜单选项
    menu_items = [
        "机器人关节映射",
        "模型格式转换", 
        "机器人模型导入",
        "机器人仿真模型迁移",
        "模型重定向",
        "运动状态"
    ]
    
    for i, item in enumerate(menu_items, 1):
        print(f"  [{i}] {item}")
    
    print(f"  [0] 退出程序")
    print("="*50)

def get_filename_by_choice(choice):
    """根据选择返回对应的文件名"""
    file_mapping = {
        1: "joint_mapping.txt",
        2: "model_format_change.txt", 
        3: "model_import.txt",
        4: "model_transfer.txt",
        5: "model_retarget.txt",  # 根据图片修正
    }
    return file_mapping.get(choice)

def get_script_dir():
    """获取脚本所在的目录路径"""
    # 使用 __file__ 获取当前脚本的完整路径
    script_path = os.path.abspath(__file__)
    script_dir = os.path.dirname(script_path)
    return script_dir

def read_file_content(filename):
    """读取并显示文件内容"""
    try:
        # 使用脚本所在目录的路径
        script_dir = get_script_dir()
        file_path = os.path.join(script_dir, filename)
        
        if not os.path.exists(file_path):
            print(f"错误: 文件 '{filename}' 不存在！")
            print(f"搜索路径: {file_path}")
            return False
        
        with open(file_path, 'r', encoding='utf-8') as file:
            content = file.read()
            
        print("\n" + "="*50)
        print(f"文件: {filename}")
        print(f"路径: {file_path}")
        print("="*50)
        print(content)
        
        if not content.strip():
            print("(文件为空)")
            
        print("="*50)
        return True
        
    except UnicodeDecodeError:
        # 如果utf-8解码失败，尝试其他编码
        try:
            with open(file_path, 'r', encoding='gbk') as file:
                content = file.read()
                
            print("\n" + "="*50)
            print(f"文件: {filename}")
            print(f"路径: {file_path}")
            print("="*50)
            print(content)
            
            if not content.strip():
                print("(文件为空)")
                
            print("="*50)
            return True
            
        except Exception as e:
            print(f"读取文件时出错: {e}")
            return False
    except Exception as e:
        print(f"读取文件时出错: {e}")
        return False

def ask_return_to_menu():
    """询问是否返回主菜单"""
    while True:
        choice = input("\n是否返回主菜单? (y/n): ").strip().lower()
        if choice in ['y', 'yes', '是', '']:
            return True
        elif choice in ['n', 'no', '否']:
            return False
        else:
            print("请输入 y/n 或 是/否")

def main():
    """主函数"""
    # 获取脚本所在目录
    script_dir = get_script_dir()
    print(f"脚本所在目录: {script_dir}")
    
    # 根据图片中的文件列表定义所有文件
    all_files = [
        "joint_mapping.txt",
        "model_format_change.txt", 
        "model_import.txt",
        "model_transfer.txt",
        "motion_retarget.txt",  # 根据图片修正
    ]
    
    # 检查存在的文件
    existing_files = {}
    missing_files = []
    
    for choice, filename in enumerate(all_files, 1):
        file_path = os.path.join(script_dir, filename)
        if os.path.exists(file_path):
            existing_files[choice] = file_path
        else:
            missing_files.append(filename)
    
    # 显示文件检查结果
    if existing_files:
        print(f"找到 {len(existing_files)} 个文件:")
        for choice, file_path in existing_files.items():
            print(f"  [{choice}] {os.path.basename(file_path)}")
    
    if missing_files:
        print(f"\n未找到以下 {len(missing_files)} 个文件:")
        for filename in missing_files:
            print(f"  - {filename}")
    
    if not existing_files:
        print(f"\n错误: 在 {script_dir} 中没有找到任何txt文件！")
        print("请确保脚本与txt文件在同一目录下")
        return
    
    while True:
        # 显示菜单
        display_menu()
        
        try:
            # 显示可用选项提示
            available_choices = list(existing_files.keys())
            choices_str = ", ".join([str(c) for c in available_choices])
            
            # 获取用户选择
            choice = input(f"\n请选择要查看的功能 ({choices_str}) 或 0 退出: ").strip()
            
            if choice == '0':
                print("\n感谢使用 developer_system 文件浏览器！")
                break
            
            if not choice.isdigit():
                print("错误: 请输入有效的数字！")
                continue
                
            choice_num = int(choice)
            
            if choice_num in existing_files:
                file_path = existing_files[choice_num]
                filename = os.path.basename(file_path)
                read_file_content(filename)
                
                # 询问是否返回主菜单
                if not ask_return_to_menu():
                    print("\n感谢使用 developer_system 文件浏览器！")
                    break
                    
            elif 1 <= choice_num <= 6:
                # 文件不存在的情况
                filename = get_filename_by_choice(choice_num)
                print(f"错误: 文件 '{filename}' 不存在于当前目录！")
                print(f"当前目录: {script_dir}")
                
                # 询问是否返回主菜单
                if not ask_return_to_menu():
                    print("\n感谢使用 developer_system 文件浏览器！")
                    break
                    
            else:
                print(f"错误: 请输入有效的选项！")
                
        except KeyboardInterrupt:
            print("\n\n程序被用户中断。")
            break
        except Exception as e:
            print(f"发生错误: {e}")
            if not ask_return_to_menu():
                print("\n感谢使用 developer_system 文件浏览器！")
                break

if __name__ == "__main__":
    print("="*50)
    print("developer_system 文件浏览器")
    print("="*50)
    
    main()
