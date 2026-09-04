import os
import json
import sys
import subprocess
import tarfile
import shlex
import shutil

CREATE_DEBOOTSTRAP_ENV = "--create-debootstrap-env" in sys.argv


def run_in_debootstrap_env():
    """Re-exec orangestrap inside a persistent Debian chroot."""
    if os.geteuid() != 0:
        print("orangestrap: --create-debootstrap-env requires root (try sudo)", file=sys.stderr)
        sys.exit(1)

    args = [arg for arg in sys.argv[1:] if arg != "--create-debootstrap-env"]
    if len(args) < 3:
        print("usage: python3 orangestrap.py [--create-debootstrap-env] <sysroot> <action> <package>")
        sys.exit(1)

    def host_tool(name):
        search_path = os.pathsep.join([
            os.environ.get("PATH", ""),
            "/run/wrappers/bin",
            "/run/current-system/sw/bin",
            "/usr/local/sbin",
            "/usr/local/bin",
            "/usr/sbin",
            "/usr/bin",
            "/sbin",
            "/bin",
        ])
        path = shutil.which(name, path=search_path)
        if path is None:
            print(f"orangestrap: required host command not found: {name}", file=sys.stderr)
            sys.exit(1)
        return path

    chroot = host_tool("chroot")
    mount = host_tool("mount")
    umount = host_tool("umount")
    debootstrap = host_tool("debootstrap")

    project_dir = os.path.realpath(os.getcwd())
    sysroot_arg = args[0]
    sysroot_path = os.path.realpath(sysroot_arg)
    os.makedirs(sysroot_path, exist_ok=True)
    env_dir = os.path.join(project_dir, ".orange-build", "debootstrap-env")
    os.makedirs(os.path.dirname(env_dir), exist_ok=True)

    debian_packages = [
        "ca-certificates", "curl", "rsync", "meson", "ninja-build", "gcc", "g++",
        "gcc-15", "g++-15", "clang", "llvm", "lld", "cmake", "make", "git",
        "coreutils", "bash", "tar", "pigz", "patchelf", "help2man", 
        "qemu-utils", "xorriso", "texinfo", "bison", "flex", "autoconf", "automake",
        "libtool", "autogen", "gtk-doc-tools", "doxygen", "libglib2.0-dev", "itstool",
        "libxml2", "libxml2-dev", "valac", "gettext", "python3", "python3-dev",
        "python3-setuptools", "python3-mako", "python3-yaml", "rustc", "cargo", "rustfmt",
        "pkg-config", "glslang-tools", "nasm", "libgmp-dev", "libmpc-dev", "libmpfr-dev",
        "patch", "file", "python3.13", "libxml2-utils", "libgirepository1.0-dev", "gobject-introspection", "gtk-update-icon-cache", "nodejs", "unzip", "locales", "intltool", "gyp", "gperf", "libical-dev",
        "gpg"
    ]

    marker = os.path.join(env_dir, ".orangestrap-debootstrap-ready")
    rootfs_ready = (
        os.path.exists(marker)
        and os.path.isfile(os.path.join(env_dir, "usr/bin/apt-get"))
        and os.path.isfile(os.path.join(env_dir, "bin/sh"))
        and os.path.isfile(os.path.join(env_dir, "usr/bin/g++"))
    )
    if not rootfs_ready:
        if os.path.exists(env_dir) and os.listdir(env_dir):
            print("orangestrap: incomplete Debian environment, recreating it")
            shutil.rmtree(env_dir)
        os.makedirs(env_dir, exist_ok=True)
        print(f"orangestrap: creating Debian environment at {env_dir}")
        subprocess.run([
            debootstrap, "--variant=minbase", "unstable", env_dir,
            "http://deb.debian.org/debian",
        ], check=True)
        resolv_conf = os.path.join(env_dir, "etc", "resolv.conf")
        if os.path.islink(resolv_conf) or os.path.exists(resolv_conf):
            os.unlink(resolv_conf)
        shutil.copyfile(os.path.realpath("/etc/resolv.conf"), resolv_conf)
        with open(os.path.join(env_dir, "etc", "apt", "sources.list"), "w", encoding="utf-8") as f:
            f.write("deb http://deb.debian.org/debian unstable main\n")
        subprocess.run([chroot, env_dir, "/usr/bin/apt-get", "update"], check=True)
        subprocess.run([
            chroot, env_dir, "/usr/bin/apt-get", "install", "-y", "--no-install-recommends",
            *debian_packages,
        ], check=True)
        subprocess.run([
            chroot, env_dir, "/usr/sbin/groupadd", "-r", "messagebus",
        ], check=True)
        subprocess.run([
            chroot, env_dir, "/usr/sbin/useradd", "-r", "-g", "messagebus", "-d", "/var/run/dbus", "-s", "/bin/false", "messagebus",
        ], check=True)
        subprocess.run([
            chroot, env_dir, "/usr/bin/apt-get", "install", "-y", "--no-install-recommends",
            "python3-gi",
        ], check=True)
        with open(marker, "w", encoding="utf-8") as f:
            f.write("ready\n")

    mounts = []
    try:
        for virtual_fs in ("proc", "sys", "dev", "run"):
            target = os.path.join(env_dir, virtual_fs)
            os.makedirs(target, exist_ok=True)
            subprocess.run([mount, "--rbind", f"/{virtual_fs}", target], check=True)
            subprocess.run([mount, "--make-rslave", target], check=True)
            mounts.append(target)

        project_target = os.path.join(env_dir, "src")
        os.makedirs(project_target, exist_ok=True)
        subprocess.run([mount, "--bind", project_dir, project_target], check=True)
        mounts.append(project_target)

        if sysroot_path == project_dir or sysroot_path.startswith(project_dir + os.sep):
            inside_sysroot = "/src/" + os.path.relpath(sysroot_path, project_dir)
        else:
            sysroot_target = os.path.join(env_dir, "sysroot")
            os.makedirs(sysroot_target, exist_ok=True)
            subprocess.run([mount, "--bind", sysroot_path, sysroot_target], check=True)
            mounts.append(sysroot_target)
            inside_sysroot = "/sysroot"

        command = ["/usr/bin/python3", "/src/orangestrap.py", inside_sysroot, *args[1:]]
        env = os.environ.copy()
        env.update({
            "HOME": "/root",
            "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
        })
        
        os.system(f"rm -rf {env_dir}/lib/x86_64-linux-gnu/libintl.so")
        os.system(f"ln -s /lib/x86_64-linux-gnu/libc.so.6 {env_dir}/lib/x86_64-linux-gnu/libintl.so")

        shell_command = "cd /src && exec " + shlex.join(command)
        subprocess.run([chroot, env_dir, "/bin/sh", "-c", shell_command], env=env, check=True)
    finally:
        for target in reversed(mounts):
            subprocess.run([umount, "-R", "-l", target], check=False)


if CREATE_DEBOOTSTRAP_ENV:
    run_in_debootstrap_env()
    sys.exit(0)

if len(sys.argv) < 4:
    print("usage: python3 orangestrap.py <sysroot> <action> <package>")
    sys.exit(1)

sysroot=sys.argv[1]
cfg=f"{sysroot}/etc/packages.json"
act=sys.argv[2]

from_dir=os.getcwd()

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


def run_shell_script(script_path, script_args, custom_env, should_use_host_env) -> bool:

    if should_use_host_env == False:
        env = custom_env
    else:
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

def download_and_extract(url, target_dir, gitflags = None):
    parent_dir = os.path.dirname(target_dir)
    os.makedirs(parent_dir, exist_ok=True)
    
    if os.path.exists(target_dir) and os.listdir(target_dir):
        return

    if url.startswith("src:"):
        actual_url = url[4:]
        print(f"orangestrap: using from source dir {actual_url}")
        try:
            os.system(f"cp -rf \"{from_dir}/{actual_url}\" \"{target_dir}\"")
        except:
            print("orangestrap: failed to copy source dir")
            sys.exit(1)
    elif url.startswith("package:"):
        package = url[8:]
        print(f"orangestrap: using from already existing package {package}")
        try:
            os.system(f"cp -rf .orange-build/sources/{package}-workdir \"{target_dir}\"")
            os.system(f"rm -rf \"{target_dir}\"/.orange-*")
        except:
            print("orangestrap: failed to copy source dir")
            sys.exit(1)
    elif url.startswith("git:"):
        actual_url = url[4:]
        print(f"orangestrap: cloning git repo {url[4:]}")
        
        cmd = ['git', 'clone', '--depth', '1']
        
        if isinstance(gitflags, list):
            cmd.extend(gitflags)
        
        cmd.extend([actual_url, target_dir])

        print(f"orangestrap: git cmd {cmd}, git flags {gitflags}")
        
        try:
            subprocess.run(cmd, check=True)
        except subprocess.CalledProcessError as e:
            print(f"orangestrap: failed to clone git repo: {e}")
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
                
                first_member = None
                for m in members:
                    clean_name = os.path.normpath(m.name)
                    if clean_name and clean_name != ".":
                        first_member = clean_name.split(os.sep)[0]
                        break
                        
                tar.extractall(path=parent_dir)

            extracted_folder = os.path.join(parent_dir, first_member) if first_member else parent_dir
            
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

    os.system(f"rm -rf {sysroot}/usr/lib/*.la")

    recipe_data = {}
    if os.path.exists(f"recipes/{pkg}.json"):
        with open(f"recipes/{pkg}.json", 'r', encoding='utf-8') as f:
            recipe_data = json.load(f)
    elif os.path.exists(f"recipes-host/{pkg}.json"):
        with open(f"recipes-host/{pkg}.json", 'r', encoding='utf-8') as f:
            recipe_data = json.load(f)
    else:
        print(f"there's no recipe recipes/{pkg}.json")
        sys.exit(-1)

    if "installed" not in data:
        data["installed"] = {}
        config_sync(data)

    if pkg in data["installed"]:
        if data["installed"][pkg] == True:
            return
    
    for dep in recipe_data["deps"]: 
        print(f"orangestrap: cheching dep {pkg}-{dep}")
        install_pkg(dep)

    os.system(f"mkdir -p .orange-build/builds/{pkg}")

    envp = {}
    envp["source_dir"] = os.path.realpath(f".orange-build/sources/{pkg}-workdir")
    envp["dest_dir"] = os.path.realpath(sysroot)
    envp["build_dir"] = os.path.realpath(f".orange-build/builds/{pkg}")
    envp["build_support"] = os.path.realpath("build-support")
    envp["pkg_lib"] = os.path.realpath("build-support/pkg_lib.sh") # pkg lib is sh file with helpers
    envp["host_dest_dir"] = os.path.realpath(f".orange-build/prefix")
    envp["tests_dir"] = os.path.realpath("tests")
    envp["distro_base_dir"] = os.path.realpath("distro_base")
    envp["sources"] = os.path.realpath(f".orange-build/sources/")
    envp["nixos_dir"] = os.path.realpath(".orange-build/nixos")

    should_use_host_env = True

    if recipe_data["use_orange_prefix"] == True:
        envp["PATH"] = os.path.realpath(".orange-build/prefix/bin") + os.pathsep + os.environ.get("PATH", "")     
        should_use_host_env = False  

    gitflags1 = []

    if "git_flags" in recipe_data:
        gitflags1 = recipe_data["git_flags"]

    print(f"{pkg} {f".orange-build/sources/{pkg}"} {os.path.dirname(".orange-build/sources/{pkg}")}")

    download_and_extract(recipe_data["url"], f".orange-build/sources/{pkg}", gitflags=gitflags1)

    if not os.path.exists(f".orange-build/sources/{pkg}-workdir/.orange-patched") and os.path.exists(f"patches/{pkg}.diff"):
        full_patch=os.path.realpath(f"patches/{pkg}.diff")
        os.system(f"cd .orange-build/sources/{pkg}-workdir && patch -p1 < {full_patch}")
        os.system(f"echo .keep > .orange-build/sources/{pkg}-workdir/.orange-patched")

    if not os.path.exists(f".orange-build/sources/{pkg}-workdir/.orange-prepare"):
        print(f"orangestrap: preparing {pkg}")
        envp["action"] = "prepare"
        ret = run_shell_script(f"instructions/{pkg}.sh", [], envp, should_use_host_env)
        if ret == False:
            print(f"Failed to prepare {pkg}")
            sys.exit(-1)
        os.system(f"echo .keep > .orange-build/sources/{pkg}-workdir/.orange-prepare")

    if not os.path.exists(f".orange-build/sources/{pkg}-workdir/.orange-configure"):
        print(f"orangestrap: configuring {pkg}")
        envp["action"] = "configure"
        ret = run_shell_script(f"instructions/{pkg}.sh", [], envp, should_use_host_env)
        if ret == False:
            print(f"Failed to configure {pkg}")
            sys.exit(-1)
        os.system(f"echo .keep > .orange-build/sources/{pkg}-workdir/.orange-configure")

    if not os.path.exists(f".orange-build/sources/{pkg}-workdir/.orange-build"):
        print(f"orangestrap: building {pkg}")
        envp["action"] = "build"
        ret = run_shell_script(f"instructions/{pkg}.sh", [], envp, should_use_host_env)
        if ret == False:
            print(f"Failed to build {pkg}")
            sys.exit(-1)
        os.system(f"echo .keep > .orange-build/sources/{pkg}-workdir/.orange-build")

    print(f"orangestrap: installing {pkg}")

    envp["action"] = "install"
    ret = run_shell_script(f"instructions/{pkg}.sh", [], envp, should_use_host_env)

    if ret == False:
        print(f"Failed to build {pkg}")
        sys.exit(-1)

    if "installed" not in data:
        data["installed"] = {}
        config_sync(data)

    data["installed"][pkg] = True
    config_sync(data)

if act == "nixos_create_env":
    print("orangestrap: creating nixos env")
    os.system("mkdir -p .orange-build/nixos/usr/include .orange-build/nixos/usr/lib .orange-build/nixos/usr/bin .orange-build/nixos/usr/lib64")
    os.system("sh build-support/prepare_nixos_env.sh .orange-build/nixos")

if act == "build":
    install_pkg(sys.argv[3])

if act == "weak_rebuild":
    os.system(f"rm -rf .orange-build/sources/{sys.argv[3]}-workdir/.orange-build")

    if "installed" not in data:
        data["installed"] = {}
        config_sync(data)

    if sys.argv[3] in data["installed"]:
        data["installed"][sys.argv[3]] = False

    config_sync(data)

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
