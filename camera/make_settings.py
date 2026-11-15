# Creates src/my_settings.h from the example on the first build, so there's
# always a private place for your Wi-Fi details (Git ignores that file).
Import("env")
import os, shutil

src = os.path.join(env.subst("$PROJECT_DIR"), "src")
mine = os.path.join(src, "my_settings.h")
if not os.path.exists(mine):
    shutil.copyfile(os.path.join(src, "my_settings.example.h"), mine)
    print("*** Created src/my_settings.h - put your Wi-Fi name and password in it, then Upload again ***")
