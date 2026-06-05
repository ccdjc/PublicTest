# -----代码上传-----
git init
git add .            # 现在 .gitignore 会生效，跳过被忽略的文件
git commit -m "提交的备注"
git push

# -----版本回溯-----
# 1. 查看所有历史提交，找到你想恢复的 commit ID
git log --oneline
# 2. 切换到某个历史版本（例如 834619a）
git checkout 834619a
# 此时工作目录的文件就是该版本的内容

# 回到最新版本
git checkout master   # 或 main