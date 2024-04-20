import os
import subprocess
import glob

# 指定包含着色器源文件的目录
shader_dir = '.\shaders'

# 遍历目录下的vert和frag文件
for shader_file in glob.glob(os.path.join(shader_dir, '*.vert')) + glob.glob(os.path.join(shader_dir, '*.frag')):
    base_name = os.path.splitext(os.path.basename(shader_file))[0]

    # 构建glslangValidator命令
    command = ['glslangValidator', '-V', shader_file, '-o', f'{base_name}.spv']

    # 执行命令
    subprocess.run(command, check=True)