import pickle

# 接受用户输入的文件名
file_name = input("请输入要读取的文件名: ")

# 使用二进制读取模式打开文件
with open(file_name, 'rb') as file:
    loaded_variable = pickle.load(file)

# 现在loaded_variable包含了从文件中加载的变量
print(loaded_variable)
