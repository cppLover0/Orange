import os
import json
import sys
import subprocess
import tarfile

if len(sys.argv) < 4:
    print("usage: python3 orangestrap.py <sysroot> <action> <package>")
    sys.exit(0)

sysroot=sys.argv[1]
cfg=f"{sysroot}/etc/packages.json"
act=sys.argv[2]

print(f"config: {cfg}, sysroot: {sysroot}")

try:
    with open(cfg, 'r', encoding='utf-8') as f:
        data = json.load(f)
except (FileNotFoundError, json.JSONDecodeError):
    data = {} 

def config_sync(data):
    with open(cfg, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=4)
        f.flush()          

os.system(f"mkdir -p {sysroot}/etc")

default_data = {}

if not os.path.exists(cfg):
    with open(cfg, 'w', encoding='utf-8') as f:
        json.dump(default_data, f, ensure_ascii=False, indent=4)

os.system(f"mkdir -p .orange-build/sources")
os.system(f"mkdir -p .orange-build/builds")
os.system(f"mkdir -p .orange-build/prefix/bin")


def run_shell_script(script_path, script_args, custom_env) -> bool:

    env = os.environ.copy()
    env.update(custom_env)

    command = ["sh", script_path] + script_args

    try:
        result = subprocess.run(
            command,
            env=env,
            capture_output=False,
            text=True,         
            check=True        
        )
    except subprocess.CalledProcessError as e:
        return False
    
    return True


import os
import sys
import subprocess
import tarfile

def download_and_extract(url, target_dir):
    parent_dir = os.path.dirname(target_dir)
    os.makedirs(parent_dir, exist_ok=True)
    
    if os.path.exists(target_dir) and os.listdir(target_dir):
        return

    if url.startswith("git:"):
        actual_url = url[4:]
        print(f"orangestrap: cloning git repo {actual_url}")
        try:
            subprocess.run(['git', 'clone', '--depth', '1', actual_url, target_dir], check=True)
        except subprocess.CalledProcessError:
            print("orangestrap: failed to clone repo")
            os.system(f'rm -rf "{target_dir}"')
            sys.exit(1)
            
    else:
        archive_name = url.split("/")[-1]
        archive_path = os.path.join(parent_dir, archive_name)

        print(f"orangestrap: downloading {url}")
        try:
            subprocess.run(['curl', '-fL', url, '-o', archive_path], check=True)
        except subprocess.CalledProcessError:
            print("orangestrap: failed to download")
            if os.path.exists(archive_path):
                os.remove(archive_path)
            sys.exit(1)

        print(f"orangestrap: unpacking to {parent_dir}")
        try:
            with tarfile.open(archive_path, "r:*") as tar:
                members = tar.getmembers()
                first_member = members[0].name.split('/')[0]
                tar.extractall(path=parent_dir)
            
            extracted_folder = os.path.join(parent_dir, first_member)
            
            if os.path.abspath(extracted_folder) != os.path.abspath(target_dir):
                if os.path.exists(target_dir):
                    os.system(f'rm -rf "{target_dir}"')
                os.rename(extracted_folder, target_dir)

            os.remove(archive_path)
        except Exception as e:
            print(f"failed to unpack: {e}")
            os.system(f'rm -rf "{target_dir}"')
            if os.path.exists(archive_path):
                os.remove(archive_path)
            sys.exit(1)

    for suffix in ["-clean", "-workdir"]:
        path = f"{target_dir}{suffix}"
        os.system(f'rm -rf "{path}"')
        os.system(f'cp -rf "{target_dir}" "{path}"')



def install_pkg(pkg):
    recipe_data = {}
    if os.path.exists(f"recipes/{pkg}.json"):
        with open(f"recipes/{pkg}.json", 'r', encoding='utf-8') as f:
            recipe_data = json.load(f)
    elif os.path.exists(f"recipes-host/{pkg}.json"):
        with open(f"recipes-host/{pkg}.json", 'r', encoding='utf-8') as f:
            recipe_data = json.load(f)
    else:
        print(f"there's no recipe {f"recipes/{pkg}.json"}")
        sys.exit(-1)

    if "installed" not in data:
        data["installed"] = {}
        config_sync(data)

    if pkg in data["installed"]:
        if data["installed"][pkg] == True:
            return
    
    for dep in recipe_data["deps"]: 
        install_pkg(dep)

    os.system(f"mkdir -p .orange-build/builds/{pkg}")

    envp = {}
    envp["source_dir"] = os.path.realpath(f".orange-build/sources/{pkg}-workdir")
    envp["dest_dir"] = os.path.realpath(sysroot)
    envp["build_dir"] = os.path.realpath(f".orange-build/builds/{pkg}")
    envp["build_support"] = os.path.realpath("build-support")
    envp["pkg_lib"] = os.path.realpath("build-support/pkg_lib.sh") # pkg lib is sh file with helpers
    envp["host_dest_dir"] = os.path.realpath(f".orange-build/prefix")
    if recipe_data["use_orange_prefix"] == True:
        envp["PATH"] = os.path.realpath(".orange-build/prefix/bin") + os.pathsep + os.environ.get("PATH", "")

    download_and_extract(recipe_data["url"], f".orange-build/sources/{pkg}")

    if not os.path.exists(f".orange-build/sources/{pkg}-workdir/.orange-patched") and os.path.exists(f"patches/{pkg}.diff"):
        full_patch=os.path.realpath(f"patches/{pkg}.diff")
        os.system(f"cd .orange-build/sources/{pkg}-workdir && patch -p1 < {full_patch}")
        os.system(f"echo .keep > .orange-build/sources/{pkg}-workdir/.orange-patched")

    if not os.path.exists(f".orange-build/sources/{pkg}-workdir/.orange-prepare"):
        print(f"orangestrap: preparing {pkg}")
        envp["action"] = "prepare"
        ret = run_shell_script(f"instructions/{pkg}.sh", [], envp)
        if ret == False:
            print(f"Failed to prepare {pkg}")
            sys.exit(-1)
        os.system(f"echo .keep > .orange-build/sources/{pkg}-workdir/.orange-prepare")

    if not os.path.exists(f".orange-build/sources/{pkg}-workdir/.orange-configure"):
        print(f"orangestrap: configuring {pkg}")
        envp["action"] = "configure"
        ret = run_shell_script(f"instructions/{pkg}.sh", [], envp)
        if ret == False:
            print(f"Failed to configure {pkg}")
            sys.exit(-1)
        os.system(f"echo .keep > .orange-build/sources/{pkg}-workdir/.orange-configure")

    if not os.path.exists(f".orange-build/sources/{pkg}-workdir/.orange-build"):
        print(f"orangestrap: building {pkg}")
        envp["action"] = "build"
        ret = run_shell_script(f"instructions/{pkg}.sh", [], envp)
        if ret == False:
            print(f"Failed to build {pkg}")
            sys.exit(-1)
        os.system(f"echo .keep > .orange-build/sources/{pkg}-workdir/.orange-build")

    # shell scripts wants this argv: install/build
    # envp: source_dir, build_dir, dest_dir, prefix always /usr, build_support and pkg_lib
    print(f"orangestrap: installing {pkg}")

    envp["action"] = "install"
    ret = run_shell_script(f"instructions/{pkg}.sh", [], envp)

    if ret == False:
        print(f"Failed to build {pkg}")
        sys.exit(-1)

    if "installed" not in data:
        data["installed"] = {}
        config_sync(data)

    data["installed"][pkg] = True
    config_sync(data)

if act == "build":
    install_pkg(sys.argv[3])

if act == "rebuild":
    os.system(f"rm -rf .orange-build/sources/{sys.argv[3]}-workdir .orange-build/builds/{sys.argv[3]}")
    os.system(f"cp -rf .orange-build/sources/{sys.argv[3]} .orange-build/sources/{sys.argv[3]}-workdir")

    if "installed" not in data:
        data["installed"] = {}
        config_sync(data)

    if sys.argv[3] in data["installed"]:
        data["installed"][sys.argv[3]] = False

    config_sync(data)

    install_pkg(sys.argv[3])

if act == "full_rebuild":
    os.system(f"rm -rf .orange-build/sources/{sys.argv[3]}-workdir .orange-build/sources/{sys.argv[3]}-clean .orange-build/sources/{sys.argv[3]} .orange-build/builds/{sys.argv[3]}")

    if "installed" not in data:
        data["installed"] = {}
        config_sync(data)

    if sys.argv[3] in data["installed"]:
        data["installed"][sys.argv[3]] = False

    config_sync(data)

    install_pkg(sys.argv[3])

config_sync(data)