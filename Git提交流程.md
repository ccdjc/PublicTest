git init
git add .            # 现在 .gitignore 会生效，跳过被忽略的文件
git commit -m "提交的备注"
检查一下将要提交的文件：git status
 关联远程仓库并推送
git remote add origin git@github.com:ccdjc/PublicTest.git
git push -u origin master --force