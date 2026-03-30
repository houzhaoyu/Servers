import chardet

def convert_to_utf8(file_path):
    # 1. 检测编码
    with open(file_path, 'rb') as f:
        raw_data = f.read()
        result = chardet.detect(raw_data)
        encoding = result['encoding']
        confidence = result['confidence']
        print(f"检测到编码: {encoding}, 置信度: {confidence}")

    if encoding is None:
        print("无法识别编码")
        return

    # 2. 读取并转换
    try:
        # 使用检测到的编码读取
        content = raw_data.decode(encoding)
        # 写入为 UTF-8
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print("转换成功！")
    except Exception as e:
        print(f"转换失败: {e}")

# 使用方法
convert_to_utf8('MysqlDao.cpp')